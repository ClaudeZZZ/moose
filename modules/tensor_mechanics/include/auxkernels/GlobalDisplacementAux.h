//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "AuxKernel.h"

// Forward Declarations
class GlobalDisplacementAux;
class GlobalStrainUserObjectInterface;
template <typename>
class RankTwoTensorTempl;
typedef RankTwoTensorTempl<Real> RankTwoTensor;

template <>
InputParameters validParams<GlobalDisplacementAux>();

class GlobalDisplacementAux : public AuxKernel
{
public:
  static InputParameters validParams();

  GlobalDisplacementAux(const InputParameters & parameters);

  virtual Real computeValue() override;

protected:
  const VariableValue & _scalar_global_strain;

  /// Component of the displacement vector
  const unsigned int _component;

  bool _output_global_disp;

  const GlobalStrainUserObjectInterface & _pst;
  const VectorValue<bool> & _periodic_dir;
  const Point _ref_point;

  const unsigned int _dim;

  /// Number of displacement variables
  const unsigned int _ndisp;

  /// Displacement variables
  std::vector<const VariableValue *> _disp;
};
