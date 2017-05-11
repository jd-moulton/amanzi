/*
  Operators 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Collection of non-member functions f2 = Map(f1, f2) where 
  Map() connects fields living on different geometric objects.
*/

#include "Teuchos_RCP.hpp"

#include "CompositeVector.hh"
#include "Mesh.hh"
#include "dg.hh"

#include "RemapUtils.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* f2 = f2 * Map(f1)
****************************************************************** */
int CellToFace_Scale(Teuchos::RCP<CompositeVector>& f1,
                     Teuchos::RCP<CompositeVector>& f2)
{
  return 0;
}


/* ******************************************************************
* f2 = Map(f1, f2):
*   cell comp:  f2_cell = f2_cell / f1_cell
*   face comp:  f2_face = f2_face / FaceAverage(f1_cell)
****************************************************************** */
int CellToFace_ScaleInverse(Teuchos::RCP<const CompositeVector> f1,
                            Teuchos::RCP<CompositeVector>& f2)
{
  ASSERT(f1->HasComponent("cell"));
  ASSERT(f2->HasComponent("cell") && f2->HasComponent("face"));

  f1->ScatterMasterToGhosted("cell");

  const Epetra_MultiVector& f1c = *f1->ViewComponent("cell", true);
  Epetra_MultiVector& f2c = *f2->ViewComponent("cell", true);
  Epetra_MultiVector& f2f = *f2->ViewComponent("face", true);

  AmanziMesh::Entity_ID_List cells;
  Teuchos::RCP<const AmanziMesh::Mesh> mesh = f1->Map().Mesh();

  // cell-part of the map
  int ncells_wghost = mesh->num_entities(AmanziMesh::CELL, AmanziMesh::USED);
  for (int c = 0; c < ncells_wghost; ++c) {
    f2c[0][c] /= f1c[0][c]; 
  }

  // face-part of the map
  int nfaces_wghost = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::USED);
  for (int f = 0; f < nfaces_wghost; ++f) {
    mesh->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();

    double tmp(0.0);
    for (int n = 0; n < ncells; ++n) tmp += f1c[0][cells[n]];
    f2f[0][f] /= (tmp / ncells); 
  }

  // hack
  if (f2->HasComponent("grav")) {
    Epetra_MultiVector& f2f = *f2->ViewComponent("grav", true);

    for (int f = 0; f < nfaces_wghost; ++f) {
      mesh->face_get_cells(f, AmanziMesh::USED, &cells);
      int ncells = cells.size();

      double tmp(0.0);
      for (int n = 0; n < ncells; ++n) tmp += f1c[0][cells[n]];
      f2f[0][f] /= (tmp / ncells); 
    }
  }

  return 0;
}


/* ******************************************************************
* Calculate mesh velocity on faces for advection based remap.
* NOTE: new memory is allocated for u.
****************************************************************** */
int RemapVelocityFaces(int order,
                       Teuchos::RCP<const AmanziMesh::Mesh> mesh1,
                       Teuchos::RCP<AmanziMesh::Mesh> mesh2,
                       Teuchos::RCP<CompositeVector>& u)
{
  int nfaces = mesh1->num_entities(AmanziMesh::FACE, AmanziMesh::OWNED);

  // allocate new memory
  int nk((order + 1) * (order + 2) / 2);
  int pk = std::max(order, 1);

  CompositeVectorSpace cvs;
  cvs.SetMesh(mesh1)->SetGhosted(true)->AddComponent("face", AmanziMesh::FACE, 2 * nk);

  u = Teuchos::RCP<CompositeVector>(new CompositeVector(cvs));
  Epetra_MultiVector& uf = *u->ViewComponent("face", false);

  // populate velocities
  AmanziMesh::Entity_ID_List nodes;
  AmanziGeometry::Point x1, x2;
  std::vector<AmanziGeometry::Point> v;

  WhetStone::DG dg(mesh1);

  for (int f = 0; f < nfaces; ++f) {
    const AmanziGeometry::Point& xf1 = mesh1->face_centroid(f);
    const AmanziGeometry::Point& xf2 = mesh2->face_centroid(f);

    v.clear();
    v.push_back(xf2 - xf1);

    if (nk > 1) {  // 2D algorithm FIXME
      mesh1->face_get_nodes(f, &nodes);
      mesh1->node_get_coordinates(nodes[0], &x1);
      mesh2->node_get_coordinates(nodes[0], &x2);

      x1 -= xf1;
      x2 -= xf2;

      WhetStone::Tensor A(2, 2);
      AmanziGeometry::Point b(2);

      A(0, 0) = x1[0];
      A(0, 1) = A(1, 0) = x1[1];
      A(1, 1) = -x1[0];

      A.Inverse();
      b = A * (x2 - x1);

      v.push_back(AmanziGeometry::Point(b[0], -b[1]));
      v.push_back(AmanziGeometry::Point(b[1],  b[0]));
v[1] *= 0.0;
v[2] *= 0.0;
    } 

    int n(0);
    for (int k = 0; k < nk; ++k) {
      uf[n++][f] = v[k][0];
      uf[n++][f] = v[k][1];
    }
  }

  return 0;
}


/* ******************************************************************
* Calculate mesh velocity in cells for advection based remap.
* NOTE: new memory is allocated for u.
****************************************************************** */
int RemapVelocityCells(int order,
                       Teuchos::RCP<const AmanziMesh::Mesh> mesh1,
                       Teuchos::RCP<AmanziMesh::Mesh> mesh2,
                       Teuchos::RCP<CompositeVector>& u)
{
  int d = mesh1->space_dimension();
  int ncells = mesh1->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);

  // allocate new memory
  int nk((order + 1) * (order + 2) / 2);
  CompositeVectorSpace cvs;
  cvs.SetMesh(mesh1)->SetGhosted(true)->AddComponent("cell", AmanziMesh::CELL, 2 * nk);

  u = Teuchos::RCP<CompositeVector>(new CompositeVector(cvs));
  Epetra_MultiVector& uc = *u->ViewComponent("cell", false);

  // populate velocities
  std::vector<int> dirs;
  AmanziMesh::Entity_ID_List faces;

  for (int c = 0; c < ncells; ++c) {
    double volume = mesh1->cell_volume(c);
    const AmanziGeometry::Point& xc = mesh1->cell_centroid(c);

    mesh1->cell_get_faces_and_dirs(c, &faces, &dirs);
    int nfaces = faces.size();

    double b0(0.0);
    AmanziGeometry::Point a0(d), a1(d), a2(d), b1(d), cc(d);

    for (int i = 0; i < nfaces; ++i) {
      int f = faces[i];
      double area = mesh1->face_area(f);
      const AmanziGeometry::Point& normal = mesh1->face_normal(f);

      const AmanziGeometry::Point& xf1 = mesh1->face_centroid(f);
      const AmanziGeometry::Point& xf2 = mesh2->face_centroid(f);

      cc = xf2 - xf1;
      a0 += area * cc;
      a1 += (dirs[i] * normal[0] / volume) * cc;
      a2 += (dirs[i] * normal[1] / volume) * cc;

      b0 += area; 
      b1 += area * (xf1 - xc);
    }

    a0 = (a0 - a1 * b1[0] - a2 * b1[1]) / b0;

std::cout << std::endl;
std::cout << "b1=" << b1 << std::endl;
std::cout << "a0=" << a0 << std::endl;
std::cout << "a1=" << a1 << std::endl;
std::cout << "a2=" << a2 << std::endl;
std::cout << (mesh2->cell_centroid(c) - xc) << " " << a0 << std::endl;
// a0 = mesh2->cell_centroid(c) - xc;
// a1 *= 0.0;
// a2 *= 0.0;

    std::vector<AmanziGeometry::Point> v;
    v.push_back(a0);
    v.push_back(a1);
    v.push_back(a2);

    // calculate velocity F(X) - X
    int n(0);
    for (int k = 0; k < nk; ++k) {
      uc[n++][c] = v[k][0];
      uc[n++][c] = v[k][1];
    }
  }

  return 0;
}

}  // namespace Operators
}  // namespace Amanzi


