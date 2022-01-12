/*
  MultiPhase PK

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)

  Calculates liquid mole fraction from partial gas pressure.
*/

#include "ModelMeshPartition.hh"
#include "MultiphaseDefs.hh"
#include "TccLiquid.hh"

namespace Amanzi {
namespace Multiphase {

/* ******************************************************************
* Simple constructor
****************************************************************** */
TccLiquid::TccLiquid(Teuchos::ParameterList& plist)
  : MultiphaseBaseEvaluator(plist)
{
  if (my_keys_.size() == 0) {
    my_keys_.push_back(std::make_pair(plist_.get<std::string>("my key"), Tags::DEFAULT));
  }
  tcc_gas_key_ = plist_.get<std::string>("tcc gas key");
  dependencies_.insert(std::make_pair(tcc_gas_key_, Tags::DEFAULT));
}


/* ******************************************************************
* Copy constructor.
****************************************************************** */
Teuchos::RCP<Evaluator> TccLiquid::Clone() const {
  return Teuchos::rcp(new TccLiquid(*this));
}


/* ******************************************************************
* Required member function.
****************************************************************** */
void TccLiquid::Evaluate_(
    const State& S, const std::vector<CompositeVector*>& results)
{
  const auto& tcc = *S.Get<CompositeVector>(tcc_gas_key_).ViewComponent("cell");
  auto& result_c = *results[0]->ViewComponent("cell");
  int ncells = result_c.MyLength();

  for (int c = 0; c != ncells; ++c) {
    result_c[0][c] = tcc[0][c] * kH_;
  }
}


/* ******************************************************************
* Required member function.
****************************************************************** */
void TccLiquid::EvaluatePartialDerivative_(
    const State& S, const Key& wrt_key, const Tag& wrt_tag,
    const std::vector<CompositeVector*>& results)
{
  auto& result_c = *results[0]->ViewComponent("cell");
  int ncells = result_c.MyLength();

  if (wrt_key == tcc_gas_key_) {
    for (int c = 0; c != ncells; ++c) {
      result_c[0][c] = kH_;
    }
  }
}

}  // namespace Multiphase
}  // namespace Amanzi

