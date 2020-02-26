/*
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Ethan Coon (ecoon@lanl.gov)
*/

//! Op on all FACES with matrices for CELL-adjacencies.

#ifndef AMANZI_OP_FACE_CELL_HH_
#define AMANZI_OP_FACE_CELL_HH_

#include <vector>
#include "DenseMatrix.hh"
#include "Operator.hh"
#include "Op.hh"

namespace Amanzi {
namespace Operators {

class Op_Face_Cell : public Op {
 public:
  Op_Face_Cell(const std::string& name,
               const Teuchos::RCP<const AmanziMesh::Mesh> mesh) :
      Op(OPERATOR_SCHEMA_BASE_FACE |
         OPERATOR_SCHEMA_DOFS_CELL, name, mesh) {
    int nfaces_owned = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::Parallel_type::OWNED);

    // CSR version 
    // 1. Compute size 
    int entries_size = 0; 
    for (int f=0; f!=nfaces_owned; ++f) {
      AmanziMesh::Entity_ID_View cells;
      mesh->face_get_cells(f, AmanziMesh::Parallel_type::ALL, cells);   
      int ncells = cells.extent(0);   
      entries_size += ncells*ncells; 
    }    

    csr = CSR_Matrix(nfaces_owned,entries_size); 
    // 2. Feed csr
    //Kokkos::resize(csr.row_map_,nfaces_owned+1);
    //Kokkos::resize(csr.entries_,entries_size);
    //Kokkos::resize(csr.sizes_,nfaces_owned,2);

    for (int f=0; f!=nfaces_owned; ++f) {
      AmanziMesh::Entity_ID_View cells;
      mesh->face_get_cells(f, AmanziMesh::Parallel_type::ALL, cells);      // This perform the prefix_sum
      int ncells = cells.extent(0); 
      csr.row_map_(f) = ncells*ncells;
      csr.sizes_(f,0) = ncells;
      csr.sizes_(f,1) = ncells; 
    }
    csr.prefix_sum(); 
  }

  virtual void
  GetLocalDiagCopy(CompositeVector& X) const
  {
    auto Xv = X.ViewComponent("cell", true);

    AmanziMesh::Mesh const * mesh_ = mesh.get();
    Kokkos::parallel_for(
        "Op_Face_Cell::GetLocalDiagCopy",
        csr.size(),
        KOKKOS_LAMBDA(const int f) {
          // Extract matrix 
          WhetStone::DenseMatrix lm(
            csr.at(f),csr.size(f,0),csr.size(f,1)); 

          AmanziMesh::Entity_ID_View cells;
          mesh_->face_get_cells(f, AmanziMesh::Parallel_type::ALL, cells);
          Kokkos::atomic_add(&Xv(cells(0), 0), lm(0,0));
          if (cells.extent(0) > 1) {
            Kokkos::atomic_add(&Xv(cells(1),0), lm(1,1));
          }
        });
  }

  
  virtual void ApplyMatrixFreeOp(const Operator* assembler,
          const CompositeVector& X, CompositeVector& Y) const {
    assembler->ApplyMatrixFreeOp(*this, X, Y);
  }

  //virtual void SymbolicAssembleMatrixOp(const Operator* assembler,
  //        const SuperMap& map, GraphFE& graph,
  //       int my_block_row, int my_block_col) const {
  //  assembler->SymbolicAssembleMatrixOp(*this, map, graph, my_block_row, my_block_col);
  //}

  //virtual void AssembleMatrixOp(const Operator* assembler,
  //        const SuperMap& map, MatrixFE& mat,
  //        int my_block_row, int my_block_col) const {
  //  assembler->AssembleMatrixOp(*this, map, mat, my_block_row, my_block_col);
  //}
  
  virtual void Rescale(const CompositeVector& scaling) {
    if (scaling.HasComponent("cell")) {
      const auto s_c = scaling.ViewComponent("cell",true);
      const Amanzi::AmanziMesh::Mesh* m = mesh.get(); 
      Kokkos::parallel_for(
          "Op_Face_Cell::Rescale",
          csr.size(), 
          KOKKOS_LAMBDA(const int& f) {
            AmanziMesh::Entity_ID_View cells;
            m->face_get_cells(f, AmanziMesh::Parallel_type::ALL, cells);
            WhetStone::DenseMatrix lm(
              csr.at(f),csr.size(f,0),csr.size(f,1)); 
            lm(0,0) *= s_c(0, cells(0));
            if (cells.size() > 1) {
              lm(0,1) *= s_c(0, cells(1));          
              lm(1,0) *= s_c(0, cells(0));          
              lm(1,1) *= s_c(0, cells(1));
            }          
          });
    }
  }
};

}  // namespace Operators
}  // namespace Amanzi


#endif

