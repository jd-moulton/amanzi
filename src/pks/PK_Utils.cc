/*
  Copyright 2010-202x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

/*
  Process Kernels

  Miscalleneous collection of simple non-member functions.
*/

#include "EvaluatorPrimary.hh"

#include "PK_Utils.hh"

namespace Amanzi {

/* ******************************************************************
* Deep copy of state fields.
****************************************************************** */
void
StateArchive::Add(std::vector<std::string>& fields, const Tag& tag)
{
  tag_ = tag;

  for (const auto& name : fields) {
    if (S_->HasEvaluator(name, tag_)) {
      if (S_->GetEvaluatorPtr(name, tag_)->get_type() == EvaluatorType::PRIMARY) {
        fields_.emplace(name, S_->Get<CompositeVector>(name, tag));
      }
    } else {
      fields_.emplace(name, S_->Get<CompositeVector>(name, tag));
    }
  }
}


/* ******************************************************************
* Deep copy of state fields.
****************************************************************** */
void
StateArchive::Restore(const std::string& passwd)
{
  for (auto it = fields_.begin(); it != fields_.end(); ++it) {
    S_->GetW<CompositeVector>(it->first, tag_, passwd) = it->second;

    if (vo_->getVerbLevel() > Teuchos::VERB_MEDIUM) {
      Teuchos::OSTab tab = vo_->getOSTab();
      *vo_->os() << "reverted field \"" << it->first << "\"" << std::endl;
    }

    if (S_->HasEvaluator(it->first, tag_)) {
      if (S_->GetEvaluatorPtr(it->first, tag_)->get_type() == EvaluatorType::PRIMARY) {
        Teuchos::rcp_dynamic_cast<EvaluatorPrimary<CompositeVector, CompositeVectorSpace>>(
          S_->GetEvaluatorPtr(it->first, tag_))
          ->SetChanged();

        if (vo_->getVerbLevel() > Teuchos::VERB_MEDIUM) {
          Teuchos::OSTab tab = vo_->getOSTab();
          *vo_->os() << "changed status of primary field \"" << it->first << "\"" << std::endl;
        }
      }
    }
  }
}


/* *******************************************************************
* Copy: Field (BASE) -> Field (prev_BASE)
******************************************************************* */
void
StateArchive::CopyFieldsToPrevFields(std::vector<std::string>& fields, const std::string& passwd)
{
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    auto name = Keys::splitKey(*it);
    std::string prev = Keys::getKey(name.first, "prev_" + name.second);
    if (S_->HasRecord(prev, tag_)) {
      fields_.emplace(prev, S_->Get<CompositeVector>(prev));
      S_->GetW<CompositeVector>(prev, tag_, passwd) = S_->Get<CompositeVector>(*it);
    }
  }
}


/* ******************************************************************
* Return a copy
****************************************************************** */
const CompositeVector&
StateArchive::get(const std::string& name)
{
  auto it = fields_.find(name);
  if (it != fields_.end()) return it->second;

  AMANZI_ASSERT(false);
}


/* ******************************************************************
* Average permeability tensor in horizontal direction.
****************************************************************** */
void
PKUtils_CalculatePermeabilityFactorInWell(const Teuchos::Ptr<State>& S,
                                          Teuchos::RCP<Epetra_Vector>& Kxy)
{
  if (!S->HasRecord("permeability", Tags::DEFAULT)) return;

  const auto& cv = S->Get<CompositeVector>("permeability", Tags::DEFAULT);
  cv.ScatterMasterToGhosted("cell");
  const auto& perm = *cv.ViewComponent("cell", true);

  int ncells_wghost = S->GetMesh()->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
  int dim = perm.NumVectors();

  Kxy = Teuchos::rcp(new Epetra_Vector(S->GetMesh()->cell_map(true)));

  for (int c = 0; c < ncells_wghost; c++) {
    (*Kxy)[c] = 0.0;
    int idim = std::max(1, dim - 1);
    for (int i = 0; i < idim; i++) (*Kxy)[c] += perm[i][c];
    (*Kxy)[c] /= idim;
  }
}


/* ******************************************************************
* Return coordinate of mesh entity (
****************************************************************** */
AmanziGeometry::Point
PKUtils_EntityCoordinates(int id, AmanziMesh::Entity_ID kind, const AmanziMesh::Mesh& mesh)
{
  if (kind == AmanziMesh::FACE) {
    return mesh.face_centroid(id);
  } else if (kind == AmanziMesh::CELL) {
    return mesh.cell_centroid(id);
  } else if (kind == AmanziMesh::NODE) {
    int d = mesh.space_dimension();
    AmanziGeometry::Point xn(d);
    mesh.node_get_coordinates(id, &xn);
    return xn;
  } else if (kind == AmanziMesh::EDGE) {
    return mesh.edge_centroid(id);
  }
  return AmanziGeometry::Point();
}

} // namespace Amanzi
