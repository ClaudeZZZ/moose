//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NonlinearEigenSystem.h"

// MOOSE includes
#include "DirichletBC.h"
#include "EigenDirichletBC.h"
#include "EigenProblem.h"
#include "IntegratedBC.h"
#include "KernelBase.h"
#include "NodalBC.h"
#include "TimeIntegrator.h"
#include "SlepcSupport.h"
#include "DGKernelBase.h"
#include "ScalarKernel.h"

#include "libmesh/eigen_system.h"
#include "libmesh/libmesh_config.h"
#include "libmesh/petsc_matrix.h"
#include "libmesh/sparse_matrix.h"

#if LIBMESH_HAVE_SLEPC

namespace Moose
{

void
assemble_matrix(EquationSystems & es, const std::string & system_name)
{
  EigenProblem * p = es.parameters.get<EigenProblem *>("_eigen_problem");
  EigenSystem & eigen_system = es.get_system<EigenSystem>(system_name);
  NonlinearEigenSystem & eigen_nl = p->getNonlinearEigenSystem();

  // If it is a linear generalized eigenvalue problem,
  // we assemble A and B together
  if (!p->isNonlinearEigenvalueSolver() && eigen_system.generalized())
  {
    p->computeJacobianAB(*eigen_system.current_local_solution.get(),
                         *eigen_system.matrix_A,
                         *eigen_system.matrix_B,
                         eigen_nl.nonEigenMatrixTag(),
                         eigen_nl.eigenMatrixTag());
#if LIBMESH_HAVE_SLEPC
    if (p->negativeSignEigenKernel())
      MatScale(static_cast<PetscMatrix<Number> &>(*eigen_system.matrix_B).mat(), -1.0);
#endif
    return;
  }

  if (!p->isNonlinearEigenvalueSolver())
  {
    p->computeJacobianTag(*eigen_system.current_local_solution.get(),
                          *eigen_system.matrix_A,
                          eigen_nl.nonEigenMatrixTag());
  }
  else
  {
    Mat petsc_mat_A = static_cast<PetscMatrix<Number> &>(*eigen_system.matrix_A).mat();

    PetscObjectComposeFunction((PetscObject)petsc_mat_A,
                               "formJacobian",
                               Moose::SlepcSupport::mooseSlepcEigenFormJacobianA);
    PetscObjectComposeFunction((PetscObject)petsc_mat_A,
                               "formFunction",
                               Moose::SlepcSupport::mooseSlepcEigenFormFunctionA);

    PetscObjectComposeFunction((PetscObject)petsc_mat_A,
                               "formFunctionAB",
                               Moose::SlepcSupport::mooseSlepcEigenFormFunctionAB);

    PetscContainer container;
    PetscContainerCreate(eigen_system.comm().get(), &container);
    PetscContainerSetPointer(container, p);
    PetscObjectCompose((PetscObject)petsc_mat_A, "formJacobianCtx", nullptr);
    PetscObjectCompose((PetscObject)petsc_mat_A, "formJacobianCtx", (PetscObject)container);
    PetscObjectCompose((PetscObject)petsc_mat_A, "formFunctionCtx", nullptr);
    PetscObjectCompose((PetscObject)petsc_mat_A, "formFunctionCtx", (PetscObject)container);
    PetscContainerDestroy(&container);

    // Let libmesh do not close matrices before solve
    eigen_system.eigen_solver->set_close_matrix_before_solve(false);
  }
  if (eigen_system.generalized())
  {
    if (eigen_system.matrix_B)
    {
      if (!p->isNonlinearEigenvalueSolver())
      {
        p->computeJacobianTag(*eigen_system.current_local_solution.get(),
                              *eigen_system.matrix_B,
                              eigen_nl.eigenMatrixTag());
      }
      else
      {
        Mat petsc_mat_B = static_cast<PetscMatrix<Number> &>(*eigen_system.matrix_B).mat();

        PetscObjectComposeFunction((PetscObject)petsc_mat_B,
                                   "formJacobian",
                                   Moose::SlepcSupport::mooseSlepcEigenFormJacobianB);
        PetscObjectComposeFunction((PetscObject)petsc_mat_B,
                                   "formFunction",
                                   Moose::SlepcSupport::mooseSlepcEigenFormFunctionB);

        PetscContainer container;
        PetscContainerCreate(eigen_system.comm().get(), &container);
        PetscContainerSetPointer(container, p);
        PetscObjectCompose((PetscObject)petsc_mat_B, "formFunctionCtx", nullptr);
        PetscObjectCompose((PetscObject)petsc_mat_B, "formFunctionCtx", (PetscObject)container);
        PetscObjectCompose((PetscObject)petsc_mat_B, "formJacobianCtx", nullptr);
        PetscObjectCompose((PetscObject)petsc_mat_B, "formJacobianCtx", (PetscObject)container);
        PetscContainerDestroy(&container);
      }
    }
    else
      mooseError("It is a generalized eigenvalue problem but matrix B is empty\n");
  }
}
}

NonlinearEigenSystem::NonlinearEigenSystem(EigenProblem & eigen_problem, const std::string & name)
  : NonlinearSystemBase(
        eigen_problem, eigen_problem.es().add_system<TransientEigenSystem>(name), name),
    _transient_sys(eigen_problem.es().get_system<TransientEigenSystem>(name)),
    _eigen_problem(eigen_problem),
    _n_eigen_pairs_required(eigen_problem.getNEigenPairsRequired()),
    _work_rhs_vector_AX(addVector("work_rhs_vector_Ax", false, PARALLEL)),
    _work_rhs_vector_BX(addVector("work_rhs_vector_Bx", false, PARALLEL))
{
  sys().attach_assemble_function(Moose::assemble_matrix);

  _Ax_tag = eigen_problem.addVectorTag("Ax_tag");

  _Bx_tag = eigen_problem.addVectorTag("Eigen");

  _A_tag = eigen_problem.addMatrixTag("A_tag");

  _B_tag = eigen_problem.addMatrixTag("Eigen");
}

void
NonlinearEigenSystem::initialSetup()
{
  NonlinearSystemBase::initialSetup();
  // DG kernels
  addEigenTagToMooseObjects(_dg_kernels);
  // Regular kernels
  addEigenTagToMooseObjects(_kernels);
  // Nodal BCs (we do not care about IBCs)
  addEigenTagToMooseObjects(_nodal_bcs);
  // Scalar kernels
  addEigenTagToMooseObjects(_scalar_kernels);
  // IntegratedBCs
  addEigenTagToMooseObjects(_integrated_bcs);
}

template <typename T>
void
NonlinearEigenSystem::addEigenTagToMooseObjects(MooseObjectTagWarehouse<T> & warehouse)
{
  for (THREAD_ID tid = 0; tid < warehouse.numThreads(); tid++)
  {
    auto & objects = warehouse.getObjects(tid);

    for (auto & object : objects)
    {
      auto & vtags = object->getVectorTags();
      // If this is not an eigen kernel
      if (vtags.find(_Bx_tag) == vtags.end())
        object->useVectorTag(_Ax_tag);
      else // also associate eigen matrix tag if this is a eigen kernel
        object->useMatrixTag(_B_tag);

      auto & mtags = object->getMatrixTags();
      if (mtags.find(_B_tag) == mtags.end())
        object->useMatrixTag(_A_tag);
      else
        object->useVectorTag(_Bx_tag);
    }
  }
}

void
NonlinearEigenSystem::solve()
{
  // Clear the iteration counters
  _current_l_its.clear();
  _current_nl_its = 0;
  // Initialize the solution vector using a predictor and known values from nodal bcs
  setInitialSolution();

// In DEBUG mode, Libmesh will check the residual automatically. This may cause
// an error because B does not need to assembly by default.
#ifdef DEBUG
  if (_eigen_problem.isGeneralizedEigenvalueProblem())
    sys().matrix_B->close();
#endif
  // Solve the transient problem if we have a time integrator; the
  // steady problem if not.
  if (_time_integrator)
  {
    _time_integrator->solve();
    _time_integrator->postSolve();
  }
  else
    system().solve();

  // store eigenvalues
  unsigned int n_converged_eigenvalues = getNumConvergedEigenvalues();

  _n_eigen_pairs_required = _eigen_problem.getNEigenPairsRequired();

  if (_n_eigen_pairs_required < n_converged_eigenvalues)
    n_converged_eigenvalues = _n_eigen_pairs_required;

  _eigen_values.resize(n_converged_eigenvalues);
  for (unsigned int n = 0; n < n_converged_eigenvalues; n++)
    _eigen_values[n] = getNthConvergedEigenvalue(n);
}

void
NonlinearEigenSystem::stopSolve()
{
  mooseError("did not implement yet \n");
}

void
NonlinearEigenSystem::setupFiniteDifferencedPreconditioner()
{
  mooseError("did not implement yet \n");
}

bool
NonlinearEigenSystem::converged()
{
  return _transient_sys.get_n_converged();
}

unsigned int
NonlinearEigenSystem::getCurrentNonlinearIterationNumber()
{
  mooseError("did not implement yet \n");
  return 0;
}

NumericVector<Number> &
NonlinearEigenSystem::RHS()
{
  return _work_rhs_vector_BX;
}

NumericVector<Number> &
NonlinearEigenSystem::residualVectorAX()
{
  return _work_rhs_vector_AX;
}

NumericVector<Number> &
NonlinearEigenSystem::residualVectorBX()
{
  return _work_rhs_vector_BX;
}

NonlinearSolver<Number> *
NonlinearEigenSystem::nonlinearSolver()
{
  mooseError("did not implement yet \n");
  return NULL;
}

void
NonlinearEigenSystem::checkIntegrity()
{
  if (_nodal_bcs.hasActiveObjects())
  {
    const auto & nodal_bcs = _nodal_bcs.getActiveObjects();
    for (const auto & nodal_bc : nodal_bcs)
    {
      auto nbc = std::dynamic_pointer_cast<DirichletBC>(nodal_bc);
      auto eigen_nbc = std::dynamic_pointer_cast<EigenDirichletBC>(nodal_bc);
      if (nbc && nbc->getParamTempl<Real>("value"))
        mooseError(
            "Can't set an inhomogeneous Dirichlet boundary condition for eigenvalue problems.");
      else if (!nbc && !eigen_nbc)
        mooseError("Invalid NodalBC for eigenvalue problems, please use homogeneous Dirichlet.");
    }
  }
}

const std::pair<Real, Real>
NonlinearEigenSystem::getNthConvergedEigenvalue(dof_id_type n)
{
  unsigned int n_converged_eigenvalues = getNumConvergedEigenvalues();
  if (n >= n_converged_eigenvalues)
    mooseError(n, " not in [0, ", n_converged_eigenvalues, ")");
  return _transient_sys.get_eigenpair(n);
}

#else

NonlinearEigenSystem::NonlinearEigenSystem(EigenProblem & eigen_problem,
                                           const std::string & /*name*/)
  : libMesh::ParallelObject(eigen_problem)
{
  mooseError("Need to install SLEPc to solve eigenvalue problems, please reconfigure libMesh\n");
}

#endif /* LIBMESH_HAVE_SLEPC */
