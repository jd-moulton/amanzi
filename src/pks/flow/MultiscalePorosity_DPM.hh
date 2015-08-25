/*
  This is the flow component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_MULTISCALE_POROSITY_HH_
#define AMANZI_MULTISCALE_POROSITY_HH_

#include <string>
#include "MultiscalePorosity.hh"

namespace Amanzi {
namespace Flow {

class MultiscalePorosity_DPM : public MultiscalePorosity {
 public:
  MultiscalePorosity_DPM() {};
  ~MultiscalePorosity_DPM() {};

  void WaterContentMatrix();
};

}  // namespace Flow
}  // namespace Amanzi
  
#endif
  
