#ifndef OPERATOR_MARSHAK_TESTCLASS_HH_
#define OPERATOR_MARSHAK_TESTCLASS_HH_

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "Teuchos_RCP.hpp"

namespace Amanzi {

class HeatConduction {
 public:
  HeatConduction(Teuchos::RCP<const AmanziMesh::Mesh> mesh, double T0)
    : mesh_(mesh), TemperatureFloor(T0), TemperatureSource(100.0)
  {
    CompositeVectorSpace cvs;
    cvs.SetMesh(mesh_)
      ->SetGhosted(true)
      ->AddComponent("cell", AmanziMesh::CELL, 1)
      ->AddComponent("face", AmanziMesh::FACE, 1)
      ->AddComponent("dirichlet_faces", AmanziMesh::BOUNDARY_FACE, 1);

    values_ = Teuchos::RCP<CompositeVector>(new CompositeVector(cvs, true));
    derivatives_ =
      Teuchos::RCP<CompositeVector>(new CompositeVector(cvs, true));
  }
  ~HeatConduction(){};

  // main members
  void UpdateValues(const CompositeVector& u, const std::vector<int>& bc_model,
                    const std::vector<double>& bc_value)
  {
    const Epetra_MultiVector& uc = *u.ViewComponent("cell", true);
    const Epetra_MultiVector& values_c = *values_->ViewComponent("cell", true);

    int ncells =
      mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
    for (int c = 0; c < ncells; c++) {
      values_c[0][c] = std::pow(uc[0][c], 3.0);
    }

    // add boundary face component
    Epetra_MultiVector& vbf = *values_->ViewComponent("dirichlet_faces", true);
    const Epetra_Map& ext_face_map = mesh_->exterior_face_map(true);
    const Epetra_Map& face_map = mesh_->face_map(true);
    for (int f = 0; f != face_map.NumMyElements(); ++f) {
      if (bc_model[f] == Operators::OPERATOR_BC_DIRICHLET) {
        AmanziMesh::Entity_ID_List cells;
        mesh_->face_get_cells(f, AmanziMesh::Parallel_type::ALL, &cells);
        AMANZI_ASSERT(cells.size() == 1);
        int bf = ext_face_map.LID(face_map.GID(f));
        vbf[0][bf] = std::pow(bc_value[f], 3.0);
      }
    }

    derivatives_->putScalar(1.0);
  }

  double Conduction(int c, double T) const { return T * T * T; }

  Teuchos::RCP<CompositeVector> values() { return values_; }
  Teuchos::RCP<CompositeVector> derivatives() { return derivatives_; }

  double exact(double t, const Amanzi::AmanziGeometry::Point& p)
  {
    double x = p[0], c = 0.4;
    double xi = c * t - x;
    return (xi > 0.0) ? std::pow(3 * c * (c * t - x), 1.0 / 3) :
                        TemperatureFloor;
  }

 public:
  double TemperatureSource;
  double TemperatureFloor;

 private:
  Teuchos::RCP<const AmanziMesh::Mesh> mesh_;
  Teuchos::RCP<CompositeVector> values_, derivatives_;
};

typedef double (HeatConduction::*ModelUpwindFn)(int c, double T) const;

} // namespace Amanzi

#endif
