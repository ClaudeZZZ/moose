//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "INSADMassPSPG.h"

registerADMooseObject("NavierStokesApp", INSADMassPSPG);

defineADLegacyParams(INSADMassPSPG);

template <ComputeStage compute_stage>
InputParameters
INSADMassPSPG<compute_stage>::validParams()
{
  InputParameters params = ADKernelGrad<compute_stage>::validParams();
  params.addClassDescription(
      "This class adds PSPG stabilization to the mass equation, enabling use of "
      "equal order shape functions for pressure and velocity variables");
  params.addParam<MaterialPropertyName>("rho_name", "rho", "The name of the density");
  return params;
}

template <ComputeStage compute_stage>
INSADMassPSPG<compute_stage>::INSADMassPSPG(const InputParameters & parameters)
  : ADKernelGrad<compute_stage>(parameters),
    _rho(getADMaterialProperty<Real>("rho_name")),
    _tau(getADMaterialProperty<Real>("tau")),
    _momentum_strong_residual(getADMaterialProperty<RealVectorValue>("momentum_strong_residual"))
{
}

template <ComputeStage compute_stage>
ADRealVectorValue
INSADMassPSPG<compute_stage>::precomputeQpResidual()
{
  return -_tau[_qp] / _rho[_qp] * _momentum_strong_residual[_qp];
}
