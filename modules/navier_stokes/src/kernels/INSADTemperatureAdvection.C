//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "INSADTemperatureAdvection.h"

registerADMooseObject("NavierStokesApp", INSADTemperatureAdvection);

defineADLegacyParams(INSADTemperatureAdvection);

template <ComputeStage compute_stage>
InputParameters
INSADTemperatureAdvection<compute_stage>::validParams()
{
  InputParameters params = ADKernelValue<compute_stage>::validParams();
  params.addClassDescription("This class computes the residual and Jacobian contributions for "
                             "temperature advection for a divergence free velocity field.");
  params.addParam<MaterialPropertyName>("rho_name", "rho", "The name of the density");
  params.addParam<MaterialPropertyName>("cp_name", "cp", "The name of the specific heat capacity");
  params.addRequiredCoupledVar("velocity", "The velocity variable");
  return params;
}

template <ComputeStage compute_stage>
INSADTemperatureAdvection<compute_stage>::INSADTemperatureAdvection(
    const InputParameters & parameters)
  : ADKernelValue<compute_stage>(parameters),
    _rho(getADMaterialProperty<Real>("rho_name")),
    _cp(getADMaterialProperty<Real>("cp_name")),
    _U(adCoupledVectorValue("velocity"))
{
}

template <ComputeStage compute_stage>
ADReal
INSADTemperatureAdvection<compute_stage>::precomputeQpResidual()
{
  return _rho[_qp] * _cp[_qp] * _U[_qp] * _grad_u[_qp];
}

adBaseClass(INSADTemperatureAdvection);

registerADMooseObject("NavierStokesApp", INSADTemperatureAdvectionSUPG);

defineADValidParams(
    INSADTemperatureAdvectionSUPG,
    ADKernelSUPG,
    params.addClassDescription(
        "This class computes the residual and Jacobian contributions for "
        "SUPG stabilization of temperature advection for a divergence free velocity field.");
    params.addParam<MaterialPropertyName>("rho_name", "rho", "The name of the density");
    params.addParam<MaterialPropertyName>("cp_name",
                                          "cp",
                                          "The name of the specific heat capacity");
    params.addRequiredCoupledVar("velocity", "The velocity variable"););

template <ComputeStage compute_stage>
INSADTemperatureAdvectionSUPG<compute_stage>::INSADTemperatureAdvectionSUPG(
    const InputParameters & parameters)
  : ADKernelSUPG<compute_stage>(parameters),
    _rho(getADMaterialProperty<Real>("rho_name")),
    _cp(getADMaterialProperty<Real>("cp_name")),
    _U(adCoupledVectorValue("velocity"))
{
}

template <ComputeStage compute_stage>
ADReal
INSADTemperatureAdvectionSUPG<compute_stage>::precomputeQpStrongResidual()
{
  return _rho[_qp] * _cp[_qp] * _U[_qp] * _grad_u[_qp];
}

adBaseClass(INSADTemperatureAdvectionSUPG);
