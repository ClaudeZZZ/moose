//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ADGrainGrowth.h"

registerADMooseObject("PhaseFieldApp", ADGrainGrowth);

defineADLegacyParams(ADGrainGrowth);

template <ComputeStage compute_stage>
InputParameters
ADGrainGrowth<compute_stage>::validParams()
{
  InputParameters params = ADGrainGrowthBase<compute_stage>::validParams();
  params.addClassDescription("Grain-Boundary model poly-crystalline interface Allen-Cahn Kernel");
  return params;
}

template <ComputeStage compute_stage>
ADGrainGrowth<compute_stage>::ADGrainGrowth(const InputParameters & parameters)
  : ADGrainGrowthBase<compute_stage>(parameters), _gamma(getADMaterialProperty<Real>("gamma_asymm"))
{
}

template <ComputeStage compute_stage>
ADReal
ADGrainGrowth<compute_stage>::computeDFDOP()
{
  // Sum all other order parameters
  ADReal SumEtaj = 0.0;
  for (unsigned int i = 0; i < _op_num; ++i)
    SumEtaj += (*_vals[i])[_qp] * (*_vals[i])[_qp];

  return _mu[_qp] * (_u[_qp] * _u[_qp] * _u[_qp] - _u[_qp] + 2.0 * _gamma[_qp] * _u[_qp] * SumEtaj);
}

adBaseClass(ADGrainGrowth);
