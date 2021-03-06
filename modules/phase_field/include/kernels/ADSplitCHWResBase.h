//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ADKernelGrad.h"
#include "DerivativeMaterialInterface.h"

#define usingSplitCHWResBase(T)                                                                    \
  usingKernelGradMembers;                                                                          \
  using ADSplitCHWResBase<compute_stage, T>::_mob_name;                                            \
  using ADSplitCHWResBase<compute_stage, T>::_mob

// Forward declarations
template <ComputeStage compute_stage, typename T = void>
class ADSplitCHWResBase;

declareADValidParams(ADSplitCHWResBase);

/**
 * ADSplitCHWResBase implements the residual for the chemical potential in the
 * split form of the Cahn-Hilliard equation in a general way that can be templated
 * to a scalar or tensor mobility.
 */
template <ComputeStage compute_stage, typename T>
class ADSplitCHWResBase : public ADKernelGrad<compute_stage>
{
public:
  static InputParameters validParams();

  ADSplitCHWResBase(const InputParameters & parameters);

protected:
  virtual ADRealVectorValue precomputeQpResidual();

  const MaterialPropertyName _mob_name;
  const ADMaterialProperty(T) & _mob;

  usingKernelGradMembers;
};

template <ComputeStage compute_stage, typename T>
ADSplitCHWResBase<compute_stage, T>::ADSplitCHWResBase(const InputParameters & parameters)
  : ADKernelGrad<compute_stage>(parameters),
    _mob_name(getParam<MaterialPropertyName>("mob_name")),
    _mob(getADMaterialProperty<T>("mob_name"))
{
}

template <ComputeStage compute_stage, typename T>
ADRealVectorValue
ADSplitCHWResBase<compute_stage, T>::precomputeQpResidual()
{
  return _mob[_qp] * _grad_u[_qp];
}

template <ComputeStage compute_stage, typename T>
InputParameters
ADSplitCHWResBase<compute_stage, T>::validParams()
{
  InputParameters params = ADKernelGrad<compute_stage>::validParams();
  params.addClassDescription(
      "Split formulation Cahn-Hilliard Kernel for the chemical potential variable");
  params.addParam<MaterialPropertyName>("mob_name", "mobtemp", "The mobility used with the kernel");
  return params;
}
