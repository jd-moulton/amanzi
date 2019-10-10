/*
  Copyright 2010-201x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:
      Konstantin Lipnikov (lipnikov@lanl.gov)
      Ethan Coon (ecoon@lanl.gov)  
*/


//! Operator represents a linear map, and typically encapsulates a discretization.

/*!
``Operator`` represents a map from linear space X to linear space Y.  Typically,
this map is a linear map, and encapsulates much of the discretization involved
in moving from continuous to discrete equations. The spaces X and Y are described 
by CompositeVectors (CV). A few maps X->Y are supported. 

An ``Operator`` provides an interface for applying both the forward and inverse
linear map (assuming the map is invertible).

Typically the ``Operator`` is never seen by the user; instead the user provides
input information for helper classes based on the continuous mathematical
operator and the desired discretization.  These helpers build the needed
``Operator``, which may include information from multiple helpers (i.e. in the
case of Jacobian Operators for a PDE).

However, one option may be provided by the user, which is related to dealing
with nearly singular operators:

* `"diagonal shift`" ``[double]`` **0.0** Adds a scalar shift to the diagonal
  of the ``Operator``, which can be useful if the ``Operator`` is singular or
  near-singular.
*/


/* 
Developer and implementation notes:

An ``Operator`` consists of, potentially, both a single, global, assembled 
matrix AND an un-ordered, additive collection of lower-rank (or equal) local
operators, here called ``Op``s. During its construction, an operator can grow
by assimilating more ``Op``s. An ``Operator`` knows how to Apply and Assemble
all of its local ``Op``s.

Typically the forward operator is applied using only local ``Op``s -- the 
inverse operator typically requires assembling a matrix, which may represent 
the entire operator or may be a Schur complement.  

In all ``Operator``s, ``Op``s, and matrices, a key concept is the schema.  
The retired schema includes, at the least, an enum representing the Degrees 
of Fredom (DoFs) associated with the Operator/matrix's domain (and implied 
equivalent range, X=Y).  A schema may also, in the case of ``Op``s, include 
information on the base entity on which the local matrix lives.
The new schema specifies dofs for the operator domain and range. It also 
includes location of the geometric base. 

For instance, the global matrix schema may be CELL, indicating that the domain
and range is defined as the space of cells on the mesh provided at
construction.  For example, a local Op might be defined with a scheme of FACE
base entities and CELL DoFs, indicating that there is one local operator on
each face, and it consists of small dense matrices with a domain and range of
all cells connected to that face (two for interior faces, one for boundary
faces).

Notes for application code developers:   
   
* We note that typically the Op object is not directly constructed; instead
  helper classes based on the discretization and continuous operator are used.
  For instance, OperatorDiffusion is a class that constructs local Ops that
  can be assembled into the global Operator to form a diffusion matrix.

* For simple operations (i.e. diffusion), Operators are not constructed
  directly.  Instead, a helper class that contains methods for creating and
  populating the Ops within the Operator is created. This helper class can
  create the appropriate Operator itself. More complex operations, i.e.
  advection-diffusion, can be generated by creating an Operator that is the
  union of all dof requirements, and then passing this Operator into the
  helper's constructor. When this is done, the helper simply checks to make
  sure the Operator contains the necessary dofs and adds local Ops to the
  Operator's list of Ops.

Note on implementation for discretization/framework developers:

* Ops work via a visitor pattern.  Assembly (resp Apply, BCs, and
  SymbolicAssembly) are implemented by the (base class) Operator calling a
  dispatch to the (base virtual class) Op, which then dispatches back to the
  (derived class) Operator so that type information of both the Operator
  (i.e. global matrix info) and the Op (i.e. local matrix info) are known.

  Adding new Ops is a fairly straightforward process of providing
  implementations of how to use the mesh to implement the forward action and
  how to assumble the local Op into the global Operator matrix.

* Ops can be shared by ``Operator``s. In combination with CopyShadowToMaster() 
  and Rescale(), the developer has a room for a variaty of optimized
  implementations.  The key variable is ``ops_properties_``. The key parameters
  are ``OPERATOR_PROPERTY_***`` and described in ``Operators_Defs.hh``.
*/ 

#ifndef AMANZI_OPERATOR_HH_
#define AMANZI_OPERATOR_HH_

#include <vector>

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Epetra_CrsMatrix.h"
#include "EpetraExt_RowMatrixOut.h"

#include "Mesh.hh"
#include "CompositeVectorSpace.hh"
#include "CompositeVector.hh"
#include "DenseVector.hh"
#include "OperatorDefs.hh"
#include "Schema.hh"

namespace Amanzi {

namespace AmanziPreconditioners { class Preconditioner; }
class CompositeVector;

namespace Operators {

class BCs;
class SuperMap;
class MatrixFE;
class GraphFE;
class Op;
class Op_Cell_FaceCell;
class Op_Cell_Face;
class Op_Cell_Cell;
class Op_Cell_Node;
class Op_Cell_Edge;
class Op_Cell_Schema;
class Op_Diagonal;
class Op_Face_Cell;
class Op_Face_CellBndFace;
class Op_Face_Schema;
class Op_Node_Node;
class Op_Edge_Edge;
class Op_SurfaceCell_SurfaceCell;
class Op_SurfaceFace_SurfaceCell;


class Operator {
 public:
  // constructors
  // At the moment CVS is the domain and range of the operator
  Operator() { apply_calls_ = 0; }

  // deprecated but yet supported
  Operator(const Teuchos::RCP<const CompositeVectorSpace>& cvs,
           Teuchos::ParameterList& plist,
           int schema);

  // general operator (domain may differ from range)
  Operator(const Teuchos::RCP<const CompositeVectorSpace>& cvs_row,
           const Teuchos::RCP<const CompositeVectorSpace>& cvs_col,
           Teuchos::ParameterList& plist,
           const Schema& schema_row,
           const Schema& schema_col);

  // bijective operator (domain = range)
  Operator(const Teuchos::RCP<const CompositeVectorSpace>& cvs,
           Teuchos::ParameterList& plist,
           const Schema& schema) :
      Operator(cvs, cvs, plist, schema, schema) {};

  virtual ~Operator() = default;
  
  void Init();

  // main members
  // -- virtual methods potentially altered by the schema
  virtual int Apply(const CompositeVector& X, CompositeVector& Y, double scalar) const;
  virtual int Apply(const CompositeVector& X, CompositeVector& Y) const {
    return Apply(X, Y, 0.0);
  }
  virtual int ApplyTranspose(const CompositeVector& X, CompositeVector& Y, double scalar = 0.0) const;
  virtual int ApplyAssembled(const CompositeVector& X, CompositeVector& Y, double scalar = 0.0) const;
  virtual int ApplyInverse(const CompositeVector& X, CompositeVector& Y) const;

  // symbolic assembly:
  // -- wrapper
  virtual void SymbolicAssembleMatrix();
  // -- first dispatch
  virtual void SymbolicAssembleMatrix(const SuperMap& map,
        GraphFE& graph, int my_block_row, int my_block_col) const;
  
  // actual assembly:
  // -- wrapper
  virtual void AssembleMatrix();
  // -- first dispatch
  virtual void AssembleMatrix(const SuperMap& map,
          MatrixFE& matrix, int my_block_row, int my_block_col) const;

  // modifiers
  // -- add a vector to operator's rhs vector  
  virtual void UpdateRHS(const CompositeVector& source, bool volume_included = true);
  // -- rescale elemental matrices
  virtual void Rescale(double scaling);
  virtual void Rescale(const CompositeVector& scaling);
  virtual void Rescale(const CompositeVector& scaling, int iops);

  // -- default functionality
  const CompositeVectorSpace& DomainMap() const { return *cvs_col_; }
  const CompositeVectorSpace& RangeMap() const { return *cvs_row_; }

  int ComputeResidual(const CompositeVector& u, CompositeVector& r, bool zero = true);
  int ComputeNegativeResidual(const CompositeVector& u, CompositeVector& r, bool zero = true);

  // preconditioner 
  void InitPreconditioner(const std::string& prec_name, const Teuchos::ParameterList& plist);
  void InitPreconditioner(Teuchos::ParameterList& plist);
  void InitializePreconditioner(Teuchos::ParameterList& plist);
  void UpdatePreconditioner();

  void CreateCheckPoint();
  void RestoreCheckPoint();

  // supporting members
  int CopyShadowToMaster(int iops);

  // access
  int schema() const { return schema_col_.OldSchema(); }
  const Schema& schema_col() const { return schema_col_; }
  const Schema& schema_row() const { return schema_row_; }

  const std::string& schema_string() const { return schema_string_; }
  void set_schema_string(const std::string& schema_string) { schema_string_ = schema_string; }

  Teuchos::RCP<const AmanziMesh::Mesh> Mesh() const { return mesh_; }
  Teuchos::RCP<SuperMap> smap() const { return smap_; }

  Teuchos::RCP<Epetra_CrsMatrix> A() { return A_; }
  Teuchos::RCP<const Epetra_CrsMatrix> A() const { return A_; }
  Teuchos::RCP<CompositeVector> rhs() { return rhs_; }
  Teuchos::RCP<const CompositeVector> rhs() const { return rhs_; }
  void set_rhs(const Teuchos::RCP<CompositeVector>& rhs) { rhs_ = rhs; }

  int apply_calls() { return apply_calls_; }

  // block access
  typedef std::vector<Teuchos::RCP<Op> >::const_iterator const_op_iterator;
  const_op_iterator OpBegin() const { return ops_.begin(); }
  const_op_iterator OpEnd() const { return ops_.end(); }
  const_op_iterator FindMatrixOp(int schema_dofs, int matching_rule, bool action) const;

  typedef std::vector<Teuchos::RCP<Op> >::iterator op_iterator;
  op_iterator OpBegin() { return ops_.begin(); }
  op_iterator OpEnd() { return ops_.end(); }
  int OpSize() { return ops_.size(); }
  op_iterator FindMatrixOp(int schema_dofs, int matching_rule, bool action);

  // block mutate
  void OpPushBack(const Teuchos::RCP<Op>& block, int properties = 0);
  void OpExtend(op_iterator begin, op_iterator end);
  void OpReplace(const Teuchos::RCP<Op>& op, int index) { ops_[index] = op; }

 public:
  // visit methods for Apply
  virtual int ApplyMatrixFreeOp(const Op_Cell_FaceCell& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Cell_Face& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Cell_Node& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Cell_Edge& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Cell_Cell& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Cell_Schema& op,
      const CompositeVector& X, CompositeVector& Y) const;

  virtual int ApplyMatrixFreeOp(const Op_Face_Cell& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Face_CellBndFace& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Face_Schema& op,
      const CompositeVector& X, CompositeVector& Y) const;

  virtual int ApplyMatrixFreeOp(const Op_Edge_Edge& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_Node_Node& op,
      const CompositeVector& X, CompositeVector& Y) const;

  virtual int ApplyMatrixFreeOp(const Op_SurfaceFace_SurfaceCell& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyMatrixFreeOp(const Op_SurfaceCell_SurfaceCell& op,
      const CompositeVector& X, CompositeVector& Y) const;

  virtual int ApplyMatrixFreeOp(const Op_Diagonal& op,
      const CompositeVector& X, CompositeVector& Y) const;

  // visit methods for ApplyTranspose 
  virtual int ApplyTransposeMatrixFreeOp(const Op_Cell_Schema& op,
      const CompositeVector& X, CompositeVector& Y) const;
  virtual int ApplyTransposeMatrixFreeOp(const Op_Face_Schema& op,
      const CompositeVector& X, CompositeVector& Y) const;

  // visit methods for symbolic assemble
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_FaceCell& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_Face& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_Node& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_Edge& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_Cell& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Cell_Schema& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;

  virtual void SymbolicAssembleMatrixOp(const Op_Face_Cell& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Face_CellBndFace& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Face_Schema& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;

  virtual void SymbolicAssembleMatrixOp(const Op_Edge_Edge& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_Node_Node& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;

  virtual void SymbolicAssembleMatrixOp(const Op_SurfaceFace_SurfaceCell& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  virtual void SymbolicAssembleMatrixOp(const Op_SurfaceCell_SurfaceCell& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  
  virtual void SymbolicAssembleMatrixOp(const Op_Diagonal& op,
          const SuperMap& map, GraphFE& graph,
          int my_block_row, int my_block_col) const;
  
  // visit methods for assemble
  virtual void AssembleMatrixOp(const Op_Cell_FaceCell& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Cell_Face& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Cell_Node& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Cell_Edge& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Cell_Cell& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Cell_Schema& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;

  virtual void AssembleMatrixOp(const Op_Face_Cell& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Face_CellBndFace& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Face_Schema& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;

  virtual void AssembleMatrixOp(const Op_Edge_Edge& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_Node_Node& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;

  virtual void AssembleMatrixOp(const Op_SurfaceCell_SurfaceCell& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;
  virtual void AssembleMatrixOp(const Op_SurfaceFace_SurfaceCell& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;

  virtual void AssembleMatrixOp(const Op_Diagonal& op,
          const SuperMap& map, MatrixFE& mat,
          int my_block_row, int my_block_col) const;

  // local <-> global communications
  virtual void ExtractVectorCellOp(int c, const Schema& schema,
          WhetStone::DenseVector& v, const CompositeVector& X) const;
  virtual void AssembleVectorCellOp(int c, const Schema& schema,
          const WhetStone::DenseVector& v, CompositeVector& X) const;

  virtual void ExtractVectorFaceOp(int c, const Schema& schema,
          WhetStone::DenseVector& v, const CompositeVector& X) const;
  virtual void AssembleVectorFaceOp(int c, const Schema& schema,
          const WhetStone::DenseVector& v, CompositeVector& X) const;

  // deep copy for building interfaces to TPLs, mainly to solvers
  // -- composite vectors
  void CopyVectorToSuperVector(const CompositeVector& cv, Epetra_Vector& sv) const;
  void CopySuperVectorToVector(const Epetra_Vector& sv, CompositeVector& cv) const;

  // diagnostics
  std::string PrintDiagnostics() const;

 protected:
  int SchemaMismatch_(const std::string& schema1, const std::string& schema2) const;

 protected:
  Teuchos::RCP<const AmanziMesh::Mesh> mesh_;
  Teuchos::RCP<const CompositeVectorSpace> cvs_row_;
  Teuchos::RCP<const CompositeVectorSpace> cvs_col_;

  mutable std::vector<Teuchos::RCP<Op> > ops_;
  mutable std::vector<int> ops_properties_;
  Teuchos::RCP<CompositeVector> rhs_, rhs_checkpoint_;

  int ncells_owned, nfaces_owned, nnodes_owned, nedges_owned;
  int ncells_wghost, nfaces_wghost, nnodes_wghost, nedges_wghost;
 
  Teuchos::RCP<Epetra_CrsMatrix> A_;
  Teuchos::RCP<MatrixFE> Amat_;
  Teuchos::RCP<SuperMap> smap_;

  Teuchos::RCP<AmanziPreconditioners::Preconditioner> preconditioner_;

  Teuchos::RCP<VerboseObject> vo_;

  int schema_old_;
  Schema schema_row_, schema_col_;
  std::string schema_string_;
  double shift_;

  mutable int apply_calls_;

 private:
  Operator(const Operator& op);
  Operator& operator=(const Operator& op);
};

}  // namespace Operators
}  // namespace Amanzi


#endif

