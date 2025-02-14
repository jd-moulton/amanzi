/*
  Copyright 2010-202x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

/*
  Operators

*/

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>

// TPLs
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "UnitTest++.h"

// Amanzi
#include "GMVMesh.hh"
#include "MeshExtractedManifold.hh"
#include "MeshFactory.hh"
#include "Mesh_Algorithms.hh"
#include "VerboseObject.hh"

// Operators
#include "OperatorDefs.hh"
#include "OperatorUtils.hh"
#include "UpwindDivK.hh"
#include "UpwindGravity.hh"
#include "UpwindFlux.hh"
#include "UpwindFluxAndGravity.hh"

namespace Amanzi {

double
Value(const AmanziGeometry::Point& xyz)
{
  return 1e-5 + xyz[0];
}

} // namespace Amanzi


/* *****************************************************************
* Test one upwind model.
* **************************************************************** */
using namespace Amanzi;
using namespace Amanzi::AmanziMesh;
using namespace Amanzi::AmanziGeometry;
using namespace Amanzi::Operators;

template <class UpwindClass>
void
RunTestUpwind(const std::string& method)
{
  auto comm = Amanzi::getDefaultComm();
  int MyPID = comm->MyPID();

  if (MyPID == 0) std::cout << "\nTest: 1st-order convergence for upwind \"" << method << "\"\n";

  // read parameter list
  std::string xmlFileName = "test/operator_upwind.xml";
  Teuchos::RCP<Teuchos::ParameterList> plist = Teuchos::getParametersFromXmlFile(xmlFileName);

  // create an SIMPLE mesh framework
  Teuchos::ParameterList region_list = plist->sublist("regions");
  Teuchos::RCP<GeometricModel> gm = Teuchos::rcp(new GeometricModel(3, region_list, *comm));

  MeshFactory meshfactory(comm, gm);
  meshfactory.set_preference(Preference({ Framework::MSTK, Framework::STK }));

  for (int n = 4; n < 17; n *= 2) {
    Teuchos::RCP<const Mesh> mesh = meshfactory.create(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, n, n, n);

    int ncells_wghost = mesh->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
    int nfaces_owned = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::Parallel_type::OWNED);
    int nfaces_wghost = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::Parallel_type::ALL);

    // create boundary data
    std::vector<int> bc_model(nfaces_wghost, OPERATOR_BC_NONE);
    std::vector<double> bc_value(nfaces_wghost);
    for (int f = 0; f < nfaces_wghost; f++) {
      const Point& xf = mesh->face_centroid(f);
      if (fabs(xf[0]) < 1e-6 || fabs(xf[0] - 1.0) < 1e-6 || fabs(xf[1]) < 1e-6 ||
          fabs(xf[1] - 1.0) < 1e-6 || fabs(xf[2]) < 1e-6 || fabs(xf[2] - 1.0) < 1e-6)

        bc_model[f] = OPERATOR_BC_DIRICHLET;
      bc_value[f] = Value(xf);
    }

    // create and initialize cell-based field
    Teuchos::RCP<CompositeVectorSpace> cvs = Teuchos::rcp(new CompositeVectorSpace());
    cvs->SetMesh(mesh)
      ->SetGhosted(true)
      ->AddComponent("cell", AmanziMesh::CELL, 1)
      ->AddComponent("face", AmanziMesh::FACE, 1);

    CompositeVector field(*cvs);
    Epetra_MultiVector& fcells = *field.ViewComponent("cell", true);
    Epetra_MultiVector& ffaces = *field.ViewComponent("face", true);

    for (int c = 0; c < ncells_wghost; c++) {
      const AmanziGeometry::Point& xc = mesh->cell_centroid(c);
      fcells[0][c] = Value(xc);
    }

    // add boundary face component
    for (int f = 0; f != bc_model.size(); ++f) {
      if (bc_model[f] == OPERATOR_BC_DIRICHLET) { ffaces[0][f] = bc_value[f]; }
    }

    // create and initialize face-based flux field
    cvs = CreateCompositeVectorSpace(mesh, "face", AmanziMesh::FACE, 1, true);

    CompositeVector flux(*cvs), solution(*cvs);
    Epetra_MultiVector& u = *flux.ViewComponent("face", true);

    Point vel(1.0, 2.0, 3.0);
    for (int f = 0; f < nfaces_wghost; f++) {
      const Point& normal = mesh->face_normal(f);
      u[0][f] = vel * normal;
    }

    // Create two upwind models
    Teuchos::ParameterList& ulist = plist->sublist("upwind");
    UpwindClass upwind(mesh);
    upwind.Init(ulist);

    upwind.Compute(flux, bc_model, field);

    // calculate errors
    double error(0.0);
    for (int f = 0; f < nfaces_owned; f++) {
      const Point& xf = mesh->face_centroid(f);
      double exact = Value(xf);

      error += pow(exact - ffaces[0][f], 2.0);

      CHECK(ffaces[0][f] >= 0.0);
    }
#ifdef HAVE_MPI
    double tmp = error;
    mesh->get_comm()->SumAll(&tmp, &error, 1);
    int itmp = nfaces_owned;
    mesh->get_comm()->SumAll(&itmp, &nfaces_owned, 1);
#endif
    error = sqrt(error / nfaces_owned);

    if (comm->MyPID() == 0) printf("n=%2d %s=%8.4f\n", n, method.c_str(), error);
  }
}

TEST(UPWIND_FLUX)
{
  RunTestUpwind<UpwindFlux>("flux");
}

TEST(UPWIND_DIVK)
{
  RunTestUpwind<UpwindDivK>("divk");
}

TEST(UPWIND_GRAVITY)
{
  RunTestUpwind<UpwindGravity>("gravity");
}

// TEST(UPWIND_FLUX_GRAVITY) {
//  RunTestUpwind<UpwindFluxAndGravity>("flux_gravity");
// }
