/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
#include "sorption_isotherm_freundlich.hh"

#include <cmath>
#include <iostream>
#include <iomanip>

namespace amanzi {
namespace chemistry {

SorptionIsothermFreundlich::SorptionIsothermFreundlich()
    : SorptionIsotherm("freundlich"),
      KD_(0.), 
      one_over_n_(0.) {
}  // end SorptionIsothermLangmuir() constructor

SorptionIsothermFreundlich::SorptionIsothermFreundlich(const double KD, 
                                                       const double one_over_n)
    : SorptionIsotherm("freundlich"),
      KD_(KD), 
      one_over_n_(one_over_n) {
}  // end SorptionIsothermLangmuir() constructor

SorptionIsothermFreundlich::~SorptionIsothermFreundlich() {
}  // end SorptionIsothermLangmuir() destructor

void SorptionIsothermFreundlich::Init(const double KD, const double n) {
  set_KD(KD);
  set_n(n);
}

double SorptionIsothermFreundlich::Evaluate(const Species& primarySpecies) {
  // Csorb = KD * activity^(1/n)
  // Units: The units don't make a whole lot of sense.
  return KD() * std::pow(primarySpecies.activity(), one_over_n());
}  // end Evaluate()

std::vector<double> SorptionIsothermFreundlich::GetParameters(void) const {
  std::vector<double> params;
  params.resize(2, 0.0);
  params.at(0) = KD();
  params.at(1) = n();
  return params;
}  // end GetParameters()

void SorptionIsothermFreundlich::SetParameters(const std::vector<double>& params) {
  set_KD(params.at(0));
  set_n(params.at(1));
}  // end SetParameters()

double SorptionIsothermFreundlich::EvaluateDerivative(
    const Species& primarySpecies) {
  // Csorb = KD * activity^(1/n)
  // dCsorb/dCaq = KD * 1/n * activity^(1/n-1) * activity_coef
  // Units:
  //  KD [kg water/m^3 bulk]
  double C_sorb = KD() * std::pow(primarySpecies.activity(), one_over_n());
  return C_sorb / primarySpecies.molality() * one_over_n();
}  // end EvaluateDerivative()

void SorptionIsothermFreundlich::Display(void) const {
  std::cout << std::setw(5) << "KD:"
            << std::scientific << std::setprecision(5)
            << std::setw(15) << KD() 
            << std::setw(5) << "1/n:"
            << std::scientific << std::setprecision(5)
            << std::setw(15) << one_over_n() 
            << std::endl;
}  // end Display()

}  // namespace chemistry
}  // namespace amanzi
