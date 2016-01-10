/*
  Operators 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  A face-based field is defined by volume averaging of cell-centered data.
*/

#ifndef AMANZI_ARITHMETIC_AVERAGE_HH_
#define AMANZI_ARITHMETIC_AVERAGE_HH_

#include <string>
#include <vector>

// TPLs
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

// Amanzi
#include "CompositeVector.hh"
#include "Mesh.hh"
#include "Point.hh"

// Operators
#include "Upwind.hh"

namespace Amanzi {
namespace Operators {

template<class Model>
class UpwindArithmeticAverage : public Upwind<Model> {
 public:
  UpwindArithmeticAverage(Teuchos::RCP<const AmanziMesh::Mesh> mesh,
                          Teuchos::RCP<const Model> model)
      : Upwind<Model>(mesh, model) {};
  ~UpwindArithmeticAverage() {};

  // main methods
  void Init(Teuchos::ParameterList& plist);

  void Compute(const CompositeVector& flux, const CompositeVector& solution,
               const std::vector<int>& bc_model, const std::vector<double>& bc_value,
               const CompositeVector& field, CompositeVector& field_upwind,
               double (Model::*Value)(int, double) const);

 private:
  using Upwind<Model>::mesh_;
  using Upwind<Model>::model_;
  using Upwind<Model>::face_comp_;

 private:
  int method_, order_;
  double tolerance_;
};


/* ******************************************************************
* Public init method. It is not yet used.
****************************************************************** */
template<class Model>
void UpwindArithmeticAverage<Model>::Init(Teuchos::ParameterList& plist)
{
  method_ = OPERATOR_UPWIND_ARITHMETIC_AVERAGE;
  tolerance_ = plist.get<double>("tolerance", OPERATOR_UPWIND_RELATIVE_TOLERANCE);
  order_ = plist.get<int>("polynomial order", 1);
}


/* ******************************************************************
* Upwind field using flux and place the result in field_upwind.
* Upwinded field must be calculated on all faces of the owned cells.
****************************************************************** */
template<class Model>
void UpwindArithmeticAverage<Model>::Compute(
    const CompositeVector& flux, const CompositeVector& solution,
    const std::vector<int>& bc_model, const std::vector<double>& bc_value,
    const CompositeVector& field, CompositeVector& field_upwind,
    double (Model::*Value)(int, double) const)
{
  ASSERT(field.HasComponent("cell"));
  ASSERT(field_upwind.HasComponent(face_comp_));

  field.ScatterMasterToGhosted("cell");

  const Epetra_MultiVector& fld_cell = *field.ViewComponent("cell", true);
  Epetra_MultiVector& upw_face = *field_upwind.ViewComponent(face_comp_, true);

  int nfaces_wghost = mesh_->num_entities(AmanziMesh::FACE, AmanziMesh::USED);
  AmanziMesh::Entity_ID_List cells;

  int c1, c2, dir;
  double kc1, kc2;
  for (int f = 0; f < nfaces_wghost; ++f) {
    mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();

    c1 = cells[0];
    kc1 = fld_cell[0][c1];

    if (ncells == 2) { 
      c2 = cells[1];
      kc2 = fld_cell[0][c2];

      double v1 = mesh_->cell_volume(c1);
      double v2 = mesh_->cell_volume(c2);

      double tmp = v2 / (v1 + v2);
      upw_face[0][f] = kc1 * tmp + kc2 * (1.0 - tmp); 

    } else {
      upw_face[0][f] = kc1;
    }
  }
}

}  // namespace Operators
}  // namespace Amanzi

#endif

