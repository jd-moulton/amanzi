/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
#include "kinetic_rate.hh"

#include <cstdlib>

#include <sstream>
#include <iostream>
#include <iomanip>

#include "string_tokenizer.hh"
#include "chemistry_output.hh"
#include "chemistry_verbosity.hh"

namespace Amanzi {
namespace AmanziChemistry {

extern ChemistryOutput* chem_out;

KineticRate::KineticRate(void)
    : debug_(false),
      name_("KineticRate"),
      identifier_(0),
      reaction_rate_(0.0) {
  reactant_names.clear();
  reactant_stoichiometry.clear();
  reactant_ids.clear();
}  // end KineticRate constructor


KineticRate::~KineticRate(void) {
}  // end KineticRate destructor


void KineticRate::SetSpeciesIds(const SpeciesArray& species,
                                const std::string& species_type,
                                const std::vector<SpeciesName>& in_names,
                                const std::vector<double>& in_stoichiometry,
                                std::vector<SpeciesId>* out_ids,
                                std::vector<double>* out_stoichiometry) {
  /*
  ** loop through a list of input names. Compare the names to the
  ** names of the species. When a match is found, set the output id to
  ** match the identifier of the speciess and possibly set the
  ** corresponding _location_ in the output stoichiometry list to the
  ** correct value.
  */
  out_ids->clear();
  if (out_stoichiometry != NULL) {
    out_stoichiometry->clear();
    out_stoichiometry->resize(species.size(), 0.0);
  }
  for (unsigned int current = 0; current < in_names.size(); current++) {
    bool species_found = false;
    // check primary species
    SpeciesArray::const_iterator s;
    for (s = species.begin(); s != species.end(); s++) {
      if (in_names.at(current) == (*s).name()) {
        species_found = true;
        out_ids->push_back((*s).identifier());
        if (out_stoichiometry != NULL) {
          // geh          (*out_stoichiometry)[current] = in_stoichiometry.at(current);
          (*out_stoichiometry)[(*s).identifier()] = in_stoichiometry.at(current);
        }
        if (debug() == kDebugMineralKinetics) {
          std::cout << "    KineticRate::SetSpeciesIds: Found " << species_type
                    << " species " << (*s).name() << std::endl;
        }
      }
    }
    if (species_found == false && debug() == kDebugMineralKinetics) {
      // TODO(bandre): is this actually a runtime error?
      std::cout << "    KineticRate::SetSpeciesIds: Did not find species \'"
                << in_names.at(current) << "\' in " << species_type
                << " species list! " << std::endl;
    }
  }
}  // end SetSpeciesIds()

void KineticRate::DisplayReaction(void) const {
  std::stringstream message;
  message << "    Reaction: " << std::endl;
  message << "      ";

  message << name();
  message << " = ";
  for (unsigned int species = 0;
       species < this->reactant_names.size(); species++) {
    message << std::setprecision(2) << this->reactant_stoichiometry.at(species) << " "
              << this->reactant_names.at(species);
    if (species < this->reactant_names.size() - 1) {
      message << " + ";
    }
  }
  message << std::endl;
  chem_out->Write(kVerbose, message);
}  // end DisplayReaction

}  // namespace AmanziChemistry
}  // namespace Amanzi
