/*
  This is the flow component of the Amanzi code. 

  Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Daniil Svyatskiy (dasvyat@lanl.gov)
*/

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "UnitTest++.h"

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"

#include "Mesh.hh"
#include "MeshFactory.hh"
#include "GMVMesh.hh"

#include "State.hh"
#include "Richards_PK.hh"

#include "BDF1_TI.hh"


/* ******************************** */
TEST(NEWTON_RICHARD_STEADY) {
  using namespace Teuchos;
  using namespace Amanzi;
  using namespace Amanzi::AmanziMesh;
  using namespace Amanzi::AmanziGeometry;
  using namespace Amanzi::AmanziFlow;

  Epetra_MpiComm comm(MPI_COMM_WORLD);
  int MyPID = comm.MyPID();

  if (MyPID == 0) std::cout << "Test: orthogonal newton solver, 2-layer model" << std::endl;
  /* read parameter list */
  std::string xmlFileName = "test/flow_richards_newton_TPFA.xml";
  ParameterXMLFileReader xmlreader(xmlFileName);
  ParameterList plist = xmlreader.getParameters();

  /* create a mesh framework */
  ParameterList region_list = plist.get<Teuchos::ParameterList>("Regions");
  GeometricModelPtr gm = new GeometricModel(3, region_list, &comm);

  FrameworkPreference pref;
  pref.clear();
  pref.push_back(MSTK);

  MeshFactory factory(&comm);
  factory.preference(pref);
  ParameterList mesh_list = plist.get<ParameterList>("Mesh").get<ParameterList>("Unstructured");
  ParameterList factory_list = mesh_list.get<ParameterList>("Generate Mesh");
  Teuchos::RCP<const Mesh> mesh(factory(factory_list, gm));

  /* create a simple state and populate it */
  Amanzi::VerboseObject::hide_line_prefix = false;

  Teuchos::ParameterList state_list = plist.get<Teuchos::ParameterList>("State");
  RCP<State> S = rcp(new State(state_list));
  S->RegisterDomainMesh(rcp_const_cast<Mesh>(mesh));

  Richards_PK* RPK = new Richards_PK(plist, S);
  S->Setup();
  S->InitializeFields();
  S->InitializeEvaluators();
  RPK->InitializeFields();
  S->CheckAllFieldsInitialized();

  /* create Richards process kernel */
  RPK->InitPK();
  RPK->InitSteadyState(0.0, 1.0);

  /* solve the problem */
  RPK->AdvanceToSteadyState(0.0, 1e+7);
  RPK->CommitState(S);

  /* derive dependent variable */
  const Epetra_MultiVector& p = *S->GetFieldData("pressure")->ViewComponent("cell");
  const Epetra_MultiVector& ws = *S->GetFieldData("water_saturation")->ViewComponent("cell");

  GMV::open_data_file(*mesh, (std::string)"flow.gmv");
  GMV::start_data();
  GMV::write_cell_data(p, 0, "pressure");
  GMV::write_cell_data(ws, 0, "saturation");
  GMV::close_data_file();

  int ncells = mesh->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  for (int c = 0; c < ncells; c++) {
    // std::cout << (mesh->cell_centroid(c))[2] << " " << p[0][c] << std::endl;
    CHECK(p[0][c] > 4500.0 && p[0][c] < 101325.0);
  }
  
  delete RPK;
}

