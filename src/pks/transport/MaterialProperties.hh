/*
  Transport PK 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_MATERIAL_PROPERTIES_HH_
#define AMANZI_MATERIAL_PROPERTIES_HH_

#include <vector>
#include <string>

#include "TransportDefs.hh"

namespace Amanzi {
namespace Transport {

class MaterialProperties {
 public:
  MaterialProperties() { tau.resize(TRANSPORT_NUMBER_PHASES, 0.0); }
  ~MaterialProperties(){};

 public:
  std::vector<double> tau;
  std::vector<std::string> regions;
};

} // namespace Transport
} // namespace Amanzi

#endif
