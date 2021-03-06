//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "AnisoHeatConduction.h"
#include "MooseMesh.h"

registerMooseObject("HeatConductionApp", AnisoHeatConduction);

defineLegacyParams(AnisoHeatConduction);

InputParameters
AnisoHeatConduction::validParams()
{
  InputParameters params = Kernel::validParams();
  params.set<bool>("use_displaced_mesh") = true;
  return params;
}

AnisoHeatConduction::AnisoHeatConduction(const InputParameters & parameters)
  : Kernel(parameters), _dim(_subproblem.mesh().dimension())
{
  _k_i[0] = _k_i[1] = _k_i[2] = NULL;
  _k_i_dT[0] = _k_i_dT[1] = _k_i_dT[2] = NULL;

  if (hasMaterialProperty<Real>("thermal_conductivity_x"))
    _k_i[0] = &getMaterialProperty<Real>("thermal_conductivity_x");

  if (hasMaterialProperty<Real>("thermal_conductivity_x_dT"))
    _k_i_dT[0] = &getMaterialProperty<Real>("thermal_conductivity_x_dT");

  if (hasMaterialProperty<Real>("thermal_conductivity_y"))
    _k_i[1] = &getMaterialProperty<Real>("thermal_conductivity_y");

  if (hasMaterialProperty<Real>("thermal_conductivity_y_dT"))
    _k_i_dT[1] = &getMaterialProperty<Real>("thermal_conductivity_y_dT");

  if (hasMaterialProperty<Real>("thermal_conductivity_z"))
    _k_i[2] = &getMaterialProperty<Real>("thermal_conductivity_z");

  if (hasMaterialProperty<Real>("thermal_conductivity_z_dT"))
    _k_i_dT[2] = &getMaterialProperty<Real>("thermal_conductivity_z_dT");

  if (!_k_i[0])
    mooseError("No thermal conductivity was defined");

  if (!_k_i[0] && _k_i[1])
    mooseError("Cannot define y conductivity but not x");

  if (_k_i[2] && (!_k_i[0] || !_k_i[1]))
    mooseError("Cannot define z conductivty but not x and y");

  if (_dim == 2 && !_k_i[1])
    mooseError("Must define x and y thermal conductivities for 2D");

  if (_dim == 3 && (!_k_i[1] || !_k_i[2]))
    mooseError("Must define x, y, and z thermal conductivities for 3D");

  if (_dim == 2 && !(_k_i_dT[0] && _k_i_dT[1]))
    mooseError("Must define k_x_dT and k_y_dT for 2D");

  if (_dim == 3 && !(_k_i_dT[0] && _k_i_dT[1] && _k_i_dT[2]))
    mooseError("Must define k_x_dT, k_y_dT, and k_z_dT for 3D");
}

Real
AnisoHeatConduction::computeQpResidual()
{
  Real r(0);
  //   r = _k[_qp]*Diffusion::computeQpResidual();
  //   if (!libmesh_isnan(r))
  //   {
  //   }
  //   else
  //   {
  //     Moose::err << "NaN found at " << __LINE__ << " in " << __FILE__ << "!\n"
  //               << "Processor: " << libMesh::processor_id() << "\n"
  //               << "_k[_qp]: " << _k[_qp] << "\n"
  //               << "Diffusion resid: " << Diffusion::computeQpResidual() << "\n"
  //               << "Elem: " << _current_elem->id() << "\n"
  //               << "Qp: " << _qp << "\n"
  //               << "Qpoint: " << _q_point[_qp] << "\n"
  //               << std::endl;
  //   }
  //   return r;
  for (unsigned i(0); i < _dim; ++i)
  {
    r += _grad_test[_i][_qp](i) * (*_k_i[i])[_qp] * _grad_u[_qp](i);
  }
  return r;
}

Real
AnisoHeatConduction::computeQpJacobian()
{
  Real jac(0);
  for (unsigned i(0); i < _dim; ++i)
  {
    jac += _grad_test[_i][_qp](i) * (*_k_i[i])[_qp] * _grad_phi[_j][_qp](i);
    if (_k_i_dT[i])
    {
      jac += (*_k_i_dT[i])[_qp] * _phi[_j][_qp] *
             (_grad_test[_i][_qp](i) * (*_k_i[i])[_qp] * _grad_u[_qp](i));
    }
  }
  return jac;
}
