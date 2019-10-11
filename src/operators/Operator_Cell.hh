/*
  Copyright 2010-201x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:
      Konstantin Lipnikov (lipnikov@lanl.gov)
      Ethan Coon (coonet@ornl.gov)
*/


//! <MISSING_ONELINE_DOCSTRING>

#ifndef AMANZI_OPERATOR_WITH_CELL_HH_
#define AMANZI_OPERATOR_WITH_CELL_HH_

#include "DenseMatrix.hh"
#include "Operator.hh"

namespace Amanzi {
namespace Operators {

class Operator_Cell : public Operator {
 public:
  // main constructor
  //   The CVS is the domain and range of the operator
  Operator_Cell(const Teuchos::RCP<const CompositeVectorSpace>& cvs,
                Teuchos::ParameterList& plist, int schema)
    : Operator(cvs, plist, schema)
  {
    set_schema_string("CELL");
    cell_max_faces = mesh_->cell_get_max_faces();
  }

  // rhs update which multiplies by cell
  virtual void UpdateRHS(const CompositeVector& source, bool volume_included);

  // visit methods for Apply
  virtual int
  ApplyMatrixFreeOp(const Op_Cell_Cell& op, const CompositeVector& X,
                    CompositeVector& Y) const;
  virtual int
  ApplyMatrixFreeOp(const Op_Face_Cell& op, const CompositeVector& X,
                    CompositeVector& Y) const;

  // visit methods for symbolic assemble
  virtual void
  SymbolicAssembleMatrixOp(const Op_Cell_Cell& op, const SuperMap& map,
                           GraphFE& graph, int my_block_row,
                           int my_block_col) const;
  virtual void
  SymbolicAssembleMatrixOp(const Op_Face_Cell& op, const SuperMap& map,
                           GraphFE& graph, int my_block_row,
                           int my_block_col) const;

  // visit methods for assemble
  virtual void
  AssembleMatrixOp(const Op_Cell_Cell& op, const SuperMap& map, MatrixFE& mat,
                   int my_block_row, int my_block_col) const;
  virtual void
  AssembleMatrixOp(const Op_Face_Cell& op, const SuperMap& map, MatrixFE& mat,
                   int my_block_row, int my_block_col) const;

 protected:
  int cell_max_faces;
};

} // namespace Operators
} // namespace Amanzi

#endif
