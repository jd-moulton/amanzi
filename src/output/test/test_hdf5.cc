#include "UnitTest++.h"
#include "../hdf5_mesh.hh"
#include "Mesh_simple.hh"
//#include "MeshFactory.hh"


TEST(HDF5) {

#ifdef HAVE_MPI
  Epetra_MpiComm *comm = new Epetra_MpiComm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm *comm = new Epetra_SerialComm();
#endif

  std::string hdf5_meshfile  = "new_mesh.h5";
  std::string hdf5_datafile1 = "new_data.h5";
  std::string hdf5_datafile2 = "new_restart.h5";
  
  Amanzi::AmanziMesh::Mesh_simple Mesh(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 4, 1, 1, comm);
  /* PREPING FOR MESHFACTORY - CURRENTLY ONLY AVAILABLE ON MESH_CHANGES BRANCH
  Amanzi::AmanziMesh::MeshFactory factory(*comm);
  Teuchos::RCP<Amanzi::AmanziMesh::Mesh> Mesh;
  Amanzi::AmanziMesh::FrameworkPreference prefs(factory.preference());
  prefs.clear();
  prefs.push_back(Amanzi::AmanziMesh::Simple);
  Mesh = factory(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 4, 1, 1);
  */

  unsigned int num_nodes = Mesh.count_entities(Amanzi::AmanziMesh::NODE, Amanzi::AmanziMesh::OWNED);
  unsigned int num_cells = Mesh.count_entities(Amanzi::AmanziMesh::CELL, Amanzi::AmanziMesh::OWNED);

  Teuchos::RCP<Epetra_Vector> node_quantity;
  Teuchos::RCP<Epetra_Vector> cell_quantity;
  Teuchos::RCP<Epetra_Vector> fake_pressure;

  // Setup node quantity
  int node_index_list[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  double node_values[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
  node_quantity = Teuchos::rcp(new Epetra_Vector(Mesh.node_map(false)));
  node_quantity->ReplaceGlobalValues(12, node_values, node_index_list);

  // Setup cell quantity
  int cell_index_list[] = {0, 1, 2, 3};
  double cell_values[] = {10, 20, 30, 40};
  cell_quantity = Teuchos::rcp(new Epetra_Vector(Mesh.cell_map(false)));
  cell_quantity->ReplaceGlobalValues(4, cell_values, cell_index_list);

  // Setup second cell quantity -- called fake pressure
  double fake_values[] = {9, 8, 7, 6};
  fake_pressure = Teuchos::rcp(new Epetra_Vector(Mesh.cell_map(false)));
  fake_pressure->ReplaceGlobalValues(4, fake_values, cell_index_list);

  // Write a file which contains both mesh and data.
  Amanzi::HDF5 viz_output;
  viz_output.setTrackXdmf(true);
  cout << "E>> creat viz mesh file" << endl;
  viz_output.createMeshFile(Mesh, hdf5_meshfile);
  cout << "E>> creat viz data file" << endl;
  viz_output.createDataFile(hdf5_datafile1);
  Amanzi::HDF5 restart_output;
  restart_output.setTrackXdmf(false);
  cout << "E>> creat restart data file" << endl;
  restart_output.createDataFile(hdf5_datafile2);

  double time = 0.0;
  for (int i = 0; i < 15; i++) {
    
    cell_quantity->ReplaceGlobalValues(4, cell_values, cell_index_list);
    fake_pressure->ReplaceGlobalValues(4, fake_values, cell_index_list);
    node_quantity->ReplaceGlobalValues(12, node_values, node_index_list);
    
    // write time step data
    cout << "E>> writing to viz:" << i << endl;
    viz_output.createTimestep(time, i);
    viz_output.writeCellData(*cell_quantity, "cell_quantity");
    viz_output.writeCellData(*fake_pressure, "pressure");
    viz_output.writeNodeData(*node_quantity, "node_quantity");

    // advance time and values
    time += 2.0;
    for (int j = 0; j < 4; j++) {
        cell_values[j] += 10.0;
        fake_values[j] += 1.0;
    }
    for (int j = 0; j < 12; j++) {
        node_values[j] += 10.0;
    }

    // close file
    viz_output.endTimestep();
  }
  
  cout << "E>> writing to restart" << endl;
  restart_output.writeCellData(*cell_quantity, "cell_quantity");
  restart_output.writeCellData(*fake_pressure, "pressure");
  restart_output.writeNodeData(*node_quantity, "node_quantity");
  
  cout << "E>> started with" << endl;
  cout << *cell_quantity;
  cout << "E>> read back restart" << endl;
  Teuchos::RCP<Epetra_Vector> read_quantity;
  read_quantity = Teuchos::rcp(new Epetra_Vector(Mesh.cell_map(false)));
  restart_output.readData(*read_quantity, "cell_quantity");
  cout << *read_quantity;

}

