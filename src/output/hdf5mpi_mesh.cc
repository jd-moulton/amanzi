#include "hdf5mpi_mesh.hh"

namespace Amanzi {
  

HDF5_MPI::HDF5_MPI(const Epetra_MpiComm &comm)
: viz_comm_(comm)
{
  viz_comm_ = comm;
  info_ = MPI_INFO_NULL;
  IOconfig_.numIOgroups = 1;
  IOconfig_.commIncoming = comm.Comm();
  parallelIO_IOgroup_init(&IOconfig_, &IOgroup_);
}

HDF5_MPI::~HDF5_MPI()
{
  parallelIO_IOgroup_cleanup(&IOgroup_);
}

//void HDF5_MPI::createMeshFile(Mesh_maps_base &mesh_maps, std::string filename)
void HDF5_MPI::createMeshFile(AmanziMesh::Mesh &mesh_maps, std::string filename)
{

  hid_t file, group, dataspace, dataset;
  herr_t status;
  hsize_t dimsf[2];
  std::string h5Filename, xmfFilename;
  int globaldims[2], localdims[2];
  int *ids;

  // build h5 filename
  h5Filename = filename + ".h5";

  // new parallel
  file = parallelIO_open_file(h5Filename.c_str(), &IOgroup_, FILE_CREATE);
  if (file < 0) {
    Errors::Message message("HDF5_MPI:: error creating mesh file");
    Exceptions::amanzi_throw(message);
  }
  
  // get num_nodes, num_cells
  const Epetra_Map &nmap = mesh_maps.node_map(false);
  int num_nodes = nmap.NumMyElements();
  int num_nodes_all = nmap.NumGlobalElements();
  const Epetra_Map &ngmap = mesh_maps.node_map(true);
  
  const Epetra_Map &cmap = mesh_maps.cell_map(false);
  int num_elems = cmap.NumMyElements();
  int num_elems_all = cmap.NumGlobalElements();

  // get coords
  double *nodes = new double[num_nodes*3];
  globaldims[0] = num_nodes_all;
  globaldims[1] = 3;
  localdims[0] = num_nodes;
  localdims[1] = 3;

  std::vector<double> xc(3);
  for (int i = 0; i < num_nodes; i++) {
    mesh_maps.node_to_coordinates(i, xc.begin(), xc.end());
    nodes[i*3] = xc[0];
    nodes[i*3+1] = xc[1];
    nodes[i*3+2] = xc[2];
  }

  // write out coords
  // TODO(barker): add error handling: can't create/write
  // new parallel
  parallelIO_write_dataset(nodes, PIO_DOUBLE, 2, globaldims, localdims, file,
                           "Mesh/Nodes", &IOgroup_,
                           NONUNIFORM_CONTIGUOUS_WRITE);
  delete nodes;
  
  // write out node map
  ids = new int[nmap.NumMyElements()];
  for (int i=0; i<num_nodes; i++) {
    ids[i] = nmap.GID(i);
  }
  globaldims[1] = 1;
  localdims[1] = 1;
  parallelIO_write_dataset(ids, PIO_INTEGER, 2, globaldims, localdims, file,
                           "Mesh/NodeMap", &IOgroup_,
                           NONUNIFORM_CONTIGUOUS_WRITE);
  
  
  // get connectivity
  // TODO(barker): remove HEX8 assumption here
  int elem_conn = 8;
  globaldims[0] = num_elems_all;
  globaldims[1] = elem_conn;
  localdims[0] = num_elems;
  localdims[1] = elem_conn;

  int nnodes(nmap.NumMyElements());
  std::vector<int> nnodesAll(viz_comm_.NumProc(),0);
  viz_comm_.GatherAll(&nnodes, &nnodesAll[0], 1);
  int start(0);
  std::vector<int> startAll(viz_comm_.NumProc(),0);
  for (int i = 0; i < viz_comm_.MyPID(); i++){
    start += nnodesAll[i];
  }
  viz_comm_.GatherAll(&start, &startAll[0],1);

  int *ielem = new int[num_elems*elem_conn];
  std::vector<unsigned int> xh(elem_conn);

  // testing
  std::vector<int> gid(num_nodes_all);
  std::vector<int> pid(num_nodes_all);
  std::vector<int> lid(num_nodes_all);
  for (int i=0; i<num_nodes_all; i++) {
    gid[i] = ngmap.GID(i);
  }
  nmap.RemoteIDList(num_nodes_all, &gid[0], &pid[0], &lid[0]);
  for (int i=0; i<num_nodes_all; i++) {
  }

  for (int i = 0; i < num_elems; i++) {
    mesh_maps.cell_to_nodes(i, xh.begin(), xh.end());
    for (int j = 0; j < elem_conn; j++) {
      if (nmap.MyLID(xh[j])) {
        ielem[i*elem_conn+j] = xh[j] + startAll[viz_comm_.MyPID()];
      } else {
	ielem[i*elem_conn+j] = lid[xh[j]] + startAll[pid[xh[j]]];
      }
    }
  }

  // write out connectivity
  // TODO(barker): add error handling: can't create/write  // new parallel
  parallelIO_write_dataset(ielem, PIO_INTEGER, 2, globaldims, localdims, file, 
                           "Mesh/Elements", &IOgroup_,
                           NONUNIFORM_CONTIGUOUS_WRITE);
  delete ielem;
  
  // write out cell map
  ids = new int[cmap.NumMyElements()];
  for (int i=0; i<num_elems; i++) {
    ids[i] = cmap.GID(i);
  }
  globaldims[1] = 1;
  localdims[1] = 1;
  parallelIO_write_dataset(ids, PIO_INTEGER, 2, globaldims, localdims, file, 
                           "Mesh/ElementMap", &IOgroup_,
                           NONUNIFORM_CONTIGUOUS_WRITE);
  delete ids;
  
  // close file
  parallelIO_close_file(file, &IOgroup_);

  // Store information
  setH5MeshFilename(h5Filename);
  setNumNodes(num_nodes_all);
  setNumElems(num_elems_all);

  // Create and write out accompanying Xdmf file
  if (TrackXdmf() && viz_comm_.MyPID() == 0) {
    xmfFilename = filename + ".xmf";
    createXdmfMesh_(xmfFilename);
  }

}

void HDF5_MPI::createDataFile(std::string soln_filename) {

  std::string h5filename, PVfilename, Vfilename;
  hid_t file;
  herr_t status;

  // ?? input mesh filename or grab global mesh filename
  // ->assumes global name exists!!
  // build h5 filename
  h5filename = soln_filename + ".h5";
  
  // new parallel
  file = parallelIO_open_file(h5filename.c_str(), &IOgroup_, FILE_CREATE);
  if (file < 0) {
    Errors::Message message("HDF5_MPI::createDataFile: error creating data file");
    Exceptions::amanzi_throw(message);
  }

  // close file
  parallelIO_close_file(file, &IOgroup_);

  // Store filenames
  setH5DataFilename(h5filename);
  if (TrackXdmf() && viz_comm_.MyPID() == 0) {
    setxdmfParaviewFilename(soln_filename + "PV.xmf");
    setxdmfVisitFilename(soln_filename + "V.xmf");

    // start xmf files xmlObjects stored inside functions
    createXdmfParaview_();
    createXdmfVisit_();
  }
}

void HDF5_MPI::createTimestep(const double time, const int iteration) {

  std::ofstream of;

  if (TrackXdmf() && viz_comm_.MyPID() == 0) {
    // create single step xdmf file
    Teuchos::XMLObject tmp("Xdmf");
    tmp.addChild(addXdmfHeaderLocal_(time));
    std::stringstream filename;
    filename << H5DataFilename() << "." << iteration << ".xmf";
    of.open(filename.str().c_str());
    of << tmp << endl;
    of.close();
    setxdmfStepFilename(filename.str());

    // update ParaView and VisIt xdmf files
    // TODO(barker): how to get to grid collection node, rather than root???
    writeXdmfParaviewGrid_(filename.str(), time, iteration);
    writeXdmfVisitGrid_(filename.str());
    // TODO(barker): where to write out depends on where the root node is
    // ?? how to terminate stream or switch to new file out??
    of.open(xdmfParaviewFilename().c_str());
    of << xmlParaview();
    of.close();
    of.open(xdmfVisitFilename().c_str());
    of << xmlVisit();
    of.close();

    // TODO(barker): where to add time & iteration information in h5 data file?
    // Store information
    xmlStep_ = tmp;
  }
  setIteration(iteration);
}

void HDF5_MPI::endTimestep() {
  if (TrackXdmf() && viz_comm_.MyPID() == 0) {
    std::ofstream of;
    std::stringstream filename;
    filename << H5DataFilename() << "." << Iteration() << ".xmf";
    of.open(filename.str().c_str());
    of << xmlStep();
    of.close();
  }
}

void HDF5_MPI::writeAttrString(const std::string value, const std::string attrname)
{
  
  hid_t file, fid, dataspace_id, attribute_id, atype;
  hsize_t dims[1]={1};
  herr_t status;
  
  /* Open the file */
  hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
  herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
  file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
    
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
    Exceptions::amanzi_throw(message);
  }
  
  /* Create a dataset attribute. */
  dataspace_id = H5Screate(H5S_SCALAR);
  atype = H5Tcopy(H5T_C_S1);
  H5Tset_size(atype, strlen(value.c_str())+1);
  H5Tset_strpad(atype,H5T_STR_NULLTERM);
  H5Tset_order(atype,H5T_ORDER_NONE);
  attribute_id = H5Acreate (file, attrname.c_str(), atype, 
                            dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
  
  /* Write the attribute data. */
  status = H5Awrite(attribute_id, atype, value.c_str());
  
  /* Close everything. */
  status = H5Aclose(attribute_id);  
  status = H5Sclose(dataspace_id);  
  status = H5Tclose(atype);  
  status = H5Pclose(acc_tpl1);
  status = H5Fclose(file);
}
  
  
void HDF5_MPI::writeAttrReal(double value, const std::string attrname)
{
  
  hid_t file, fid, dataspace_id, attribute_id;
  hsize_t dims;
  herr_t status;
  
  /* Setup dataspace */
  dims = 1;
  dataspace_id = H5Screate_simple(1, &dims, NULL);
  
  /* Open the file */
  hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
  herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
  file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
    
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
    Exceptions::amanzi_throw(message);
  }
    
  /* Create a dataset attribute. */
  attribute_id = H5Acreate (file, attrname.c_str(), H5T_NATIVE_DOUBLE, 
                            dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
  
  /* Write the attribute data. */
  status = H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, &value);
  
  /* Close everything. */
  status = H5Aclose(attribute_id);  
  status = H5Sclose(dataspace_id);  
  status = H5Pclose(acc_tpl1);
  status = H5Fclose(file);
}
  
void HDF5_MPI::writeAttrInt(int value, const std::string attrname)
{
  
  hid_t file, dataspace_id, attribute_id;
  hsize_t dims;
  herr_t status;

  /* Setup dataspace */
  dims = 1;
  dataspace_id = H5Screate_simple(1, &dims, NULL);
  
  /* Open the file */
  //file = parallelIO_open_file(H5DataFilename_.c_str(), &IOgroup_,
  //                            FILE_READWRITE);
  //iofile_t *curr_file = IOgroup_.file[file];
  hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
  herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
  file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
    
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
    Exceptions::amanzi_throw(message);
  }
    
  /* Create/Write a dataset attribute. */
  attribute_id = H5Acreate (file, attrname.c_str(), H5T_NATIVE_INT,
                            dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Awrite(attribute_id, H5T_NATIVE_INT, &value);
  
  /* Set the attribute data. */
  //H5LTset_attribute_int(curr_file->fid, "/", attrname.c_str(), &value, 1);
  //H5LTset_attribute_int(file, "/", attrname.c_str(), &value, 1);
  
  /* Close everything. */
  status = H5Aclose(attribute_id);  
  status = H5Sclose(dataspace_id); 
  status = H5Pclose(acc_tpl1);
  status = H5Fclose(file);
  //parallelIO_close_file(file, &IOgroup_);
  
}

void HDF5_MPI::readAttrString(std::string &value, const std::string attrname)
{
  
  hid_t file, fid, attribute_id, atype, root;
  herr_t status;
  
  /* Open the file */
  hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
  herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
  file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
  root = H5Gopen(file, "/", H5P_DEFAULT);
    
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
    Exceptions::amanzi_throw(message);
  }
  
  atype = H5Tcopy(H5T_C_S1);
  attribute_id = H5Aopen(root, attrname.c_str(), H5P_DEFAULT);
  hid_t ftype = H5Aget_type(attribute_id);
  size_t size = H5Tget_size(ftype);
  H5Tset_size(atype, size);
  
  /* Write the attribute data. */
  char string_out[size];
  status = H5Aread(attribute_id, atype, string_out);
  value = string_out;

  /* Close everything. */ 
  status = H5Tclose(atype);    
  status = H5Tclose(ftype);  
  status = H5Pclose(acc_tpl1);
  status = H5Aclose(attribute_id); 
  status = H5Gclose(root);
  status = H5Fclose(file);
}
  
void HDF5_MPI::readAttrReal(double &value, const std::string attrname)
{
    
  hid_t file, fid, attribute_id;
  herr_t status;
    
  /* Open the file */
  hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
  herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
  file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
    
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
    Exceptions::amanzi_throw(message);
  }
    
  /* Create a dataset attribute. */
  attribute_id = H5Aopen(file, attrname.c_str(), H5P_DEFAULT);
    
  /* Write the attribute data. */
  status = H5Aread(attribute_id, H5T_NATIVE_DOUBLE, &value);
    
  /* Close everything. */
  status = H5Aclose(attribute_id);  
  status = H5Pclose(acc_tpl1);
  status = H5Fclose(file);
  
}
  
  void HDF5_MPI::readAttrInt(int &value, const std::string attrname)
  {
    
    hid_t file, fid, attribute_id;
    herr_t status;
    
    /* Open the file */
    hid_t acc_tpl1 = H5Pcreate (H5P_FILE_ACCESS);
    herr_t ret = H5Pset_fapl_mpio(acc_tpl1, viz_comm_.Comm(), info_);
    file = H5Fopen(H5DataFilename_.c_str(), H5F_ACC_RDWR, acc_tpl1);
    
    if (file < 0) {
      Errors::Message message("HDF5_MPI::writeAttrReal error opening data file to write attribute");
      Exceptions::amanzi_throw(message);
    }
    
    /* Create a dataset attribute. */
    attribute_id = H5Aopen(file, attrname.c_str(), H5P_DEFAULT);
    
    /* Write the attribute data. */
    status = H5Aread(attribute_id, H5T_NATIVE_INT, &value);
    
    /* Close everything. */
    status = H5Aclose(attribute_id);  
    status = H5Pclose(acc_tpl1);
    status = H5Fclose(file);
  }
  
void HDF5_MPI::writeDataReal(const Epetra_Vector &x, const std::string varname) 
{
  writeFieldData_(x, varname, PIO_DOUBLE, "NONE");
}

void HDF5_MPI::writeDataInt(const Epetra_Vector &x, const std::string varname)
{
  writeFieldData_(x, varname, PIO_INTEGER, "NONE");
}
  
void HDF5_MPI::writeCellDataReal(const Epetra_Vector &x,
                                 const std::string varname)
{
  writeFieldData_(x, varname, PIO_DOUBLE, "Cell");
}

void HDF5_MPI::writeCellDataInt(const Epetra_Vector &x,
                                const std::string varname)
{
  writeFieldData_(x, varname, PIO_INTEGER, "Cell");
}

void HDF5_MPI::writeNodeDataReal(const Epetra_Vector &x, 
                                 const std::string varname)
{
  writeFieldData_(x, varname, PIO_DOUBLE, "Node");
}

void HDF5_MPI::writeNodeDataInt(const Epetra_Vector &x, 
                                const std::string varname)
{
  writeFieldData_(x, varname, PIO_INTEGER, "Node");
}

  
void HDF5_MPI::writeFieldData_(const Epetra_Vector &x, std::string varname,
                               datatype_t type, std::string loc) {
  // write field data
  double *data;
  int err = x.ExtractView(&data);
  hid_t file, group, dataspace, dataset;
  herr_t status;

  int globaldims[2], localdims[2];
  globaldims[0] = x.GlobalLength();
  globaldims[1] = 1;
  localdims[0] = x.MyLength();
  localdims[1] = 1;

  // TODO(barker): how to build path name?? probably still need iteration number
  std::stringstream h5path;
  h5path << varname;

  // TODO(barker): add error handling: can't write/create

  file = parallelIO_open_file(H5DataFilename_.c_str(), &IOgroup_,
                              FILE_READWRITE);
  if (file < 0) {
    Errors::Message message("HDF5_MPI::writeFieldData_ error opening data file to write field data");
    Exceptions::amanzi_throw(message);
  }

  if (TrackXdmf()){
    h5path << "/" << Iteration();
  }
  
  char *tmp;
  tmp = new char [h5path.str().size()+1];
  strcpy(tmp,h5path.str().c_str());
  parallelIO_write_dataset(data, type, 2, globaldims, localdims, file, tmp,
                           &IOgroup_, NONUNIFORM_CONTIGUOUS_WRITE);    
  parallelIO_close_file(file, &IOgroup_);

  // TODO(barker): add error handling: can't write
  if (TrackXdmf() && viz_comm_.MyPID() == 0) {
    // TODO(barker): get grid node, node.addChild(addXdmfAttribute)
    Teuchos::XMLObject node = findMeshNode_(xmlStep());
    node.addChild(addXdmfAttribute_(varname, loc, globaldims[0], h5path.str()));
  }
}
  
void HDF5_MPI::readData(Epetra_Vector &x, const std::string varname)
{
  readFieldData_(x, varname, PIO_DOUBLE);
}
  
void HDF5_MPI::readFieldData_(Epetra_Vector &x, std::string varname,
                               datatype_t type) {
  
  char *h5path = new char [varname.size()+1];
  strcpy(h5path,varname.c_str());
  int ndims;
  hid_t file = parallelIO_open_file(H5DataFilename_.c_str(), &IOgroup_,
                                    FILE_READONLY);
  parallelIO_get_dataset_ndims(&ndims, file, h5path, &IOgroup_);
  int  globaldims[ndims], localdims[ndims];
  parallelIO_get_dataset_dims(globaldims, file, h5path, &IOgroup_);
  localdims[0] = x.MyLength();
  localdims[1] = globaldims[1];
  std::vector<int> myidx(localdims[0],0);
  int start = 0;
  for (int i=0; i<localdims[0]; i++) myidx[i] = i+start;
  
  double *data = new double[localdims[0]*localdims[1]];
  parallelIO_read_dataset(data, type, ndims, globaldims, localdims,
                          file, h5path, &IOgroup_, NONUNIFORM_CONTIGUOUS_READ);
  x.ReplaceMyValues(localdims[0], &data[0], &myidx[0]);
  
  parallelIO_close_file(file, &IOgroup_); 
  delete data;
  
}
  
void HDF5_MPI::createXdmfMesh_(const std::string xmfFilename) {
  // TODO(barker): add error handling: can't open/write
  Teuchos::XMLObject mesh("Xdmf");

  // build xml object
  mesh.addChild(addXdmfHeaderLocal_(0.0));

  // write xmf
  std::ofstream of(xmfFilename.c_str());
  of << HDF5_MPI::xdmfHeader_ << mesh << endl;
  of.close();
}

void HDF5_MPI::createXdmfParaview_() {
  // TODO(barker): add error handling: can't open/write
  Teuchos::XMLObject xmf("Xdmf");
  xmf.addAttribute("xmlns:xi", "http://www.w3.org/2001/XInclude");
  xmf.addAttribute("Version", "2.0");

  // build xml object
  Teuchos::XMLObject header;
  header = addXdmfHeaderGlobal_();
  xmf.addChild(header);
  Teuchos::XMLObject node;
  node = findGridNode_(xmf);
  node.addChild(addXdmfTopo_());
  node.addChild(addXdmfGeo_());

  // write xmf
  std::ofstream of(xdmfParaviewFilename().c_str());
  of << HDF5_MPI::xdmfHeader_ << xmf << endl;
  of.close();

  // Store ParaView XMLObject
  xmlParaview_ = xmf;
}

void HDF5_MPI::createXdmfVisit_() {
  // TODO(barker): add error handling: can't open/write
  Teuchos::XMLObject xmf("Xdmf");
  xmf.addAttribute("xmlns:xi", "http://www.w3.org/2001/XInclude");
  xmf.addAttribute("Version", "2.0");

  // build xml object
  xmf.addChild(addXdmfHeaderGlobal_());

  // write xmf
  std::ofstream of(xdmfVisitFilename().c_str());
  of << HDF5_MPI::xdmfHeader_ << xmf << endl;
  of.close();

  // Store VisIt XMLObject
  xmlVisit_ = xmf;
}

Teuchos::XMLObject HDF5_MPI::addXdmfHeaderGlobal_() {

  Teuchos::XMLObject domain("Domain");

  Teuchos::XMLObject grid("Grid");
  grid.addAttribute("GridType", "Collection");
  grid.addAttribute("CollectionType", "Temporal");
  domain.addChild(grid);

  return domain;
}

Teuchos::XMLObject HDF5_MPI::addXdmfHeaderLocal_(const double value) {
  Teuchos::XMLObject domain("Domain");

  Teuchos::XMLObject grid("Grid");
  grid.addAttribute("Name", "Mesh");
  domain.addChild(grid);
  grid.addChild(addXdmfTopo_());
  grid.addChild(addXdmfGeo_());

  Teuchos::XMLObject time("Time");
  time.addDouble("Value", value);
  grid.addChild(time);

  return domain;
}

Teuchos::XMLObject HDF5_MPI::addXdmfTopo_() {
  std::stringstream tmp, tmp1;

  Teuchos::XMLObject topo("Topology");
  // TODO(barker): remove hex assumption
  topo.addAttribute("Type", "Hexahedron");
  topo.addInt("Dimensions", NumElems());
  topo.addAttribute("Name", "topo");

  Teuchos::XMLObject DataItem("DataItem");
  DataItem.addAttribute("DataType", "Int");
  // TODO(barker): remove hex assumption
  tmp << NumElems() << " 8";
  DataItem.addAttribute("Dimensions", tmp.str());
  DataItem.addAttribute("Format", "HDF");

  tmp1 << H5MeshFilename() << ":/Mesh/Elements";
  DataItem.addContent(tmp1.str());

  topo.addChild(DataItem);

  return topo;
}

Teuchos::XMLObject HDF5_MPI::addXdmfGeo_() {
  std::string tmp;
  std::stringstream tmp1;

  Teuchos::XMLObject geo("Geometry");
  geo.addAttribute("Name", "geo");
  geo.addAttribute("Type", "XYZ");

  Teuchos::XMLObject DataItem("DataItem");
  DataItem.addAttribute("DataType", "Float");
  tmp1 << NumNodes() << " 3";
  DataItem.addAttribute("Dimensions", tmp1.str());
  DataItem.addAttribute("Format", "HDF");
  tmp = H5MeshFilename() + ":/Mesh/Nodes";
  DataItem.addContent(tmp);
  geo.addChild(DataItem);

  return geo;
}

void HDF5_MPI::writeXdmfParaviewGrid_(std::string filename, const double time,
                                const int iteration) {
  // Create xmlObject grid

  Teuchos::XMLObject grid("Grid");
  grid.addInt("Name", iteration);
  grid.addAttribute("GridType", "Uniform");

  // TODO(barker): update topo/geo name if mesh is evolving
  Teuchos::XMLObject topo("Topology");
  topo.addAttribute("Reference", "//Topology[@Name='topo']");
  grid.addChild(topo);
  Teuchos::XMLObject geo("Geometry");
  geo.addAttribute("Reference", "//Geometry[@Name='geo']");
  grid.addChild(geo);

  Teuchos::XMLObject timeValue("Time");
  timeValue.addDouble("Value", time);
  grid.addChild(timeValue);

  Teuchos::XMLObject xi_include("xi:include");
  xi_include.addAttribute("href", filename);
  xi_include.addAttribute("xpointer", "xpointer(//Xdmf/Domain/Grid/Attribute)");
  grid.addChild(xi_include);

  // Step through paraview xmlobject to find /domain/grid
  Teuchos::XMLObject node;
  node = findGridNode_(xmlParaview_);

  // Add new grid to xmlobject paraview
  node.addChild(grid);
}

void HDF5_MPI::writeXdmfVisitGrid_(std::string filename) {

  // Create xmlObject grid
  Teuchos::XMLObject xi_include("xi:include");
  xi_include.addAttribute("href", filename);
  xi_include.addAttribute("xpointer", "xpointer(//Xdmf/Domain/Grid)");

  // Step through xmlobject visit to find /domain/grid
  Teuchos::XMLObject node;
  node = findGridNode_(xmlVisit_);

  // Add new grid to xmlobject visit
  node.addChild(xi_include);
}

Teuchos::XMLObject HDF5_MPI::findGridNode_(Teuchos::XMLObject xmlobject) {

  Teuchos::XMLObject node, tmp;

  // Step down to child tag==Domain
  for (int i = 0; i < xmlobject.numChildren(); i++) {
    if (xmlobject.getChild(i).getTag() == "Domain") {
      node = xmlobject.getChild(i);
    }
  }

  // Step down to child tag==Grid and Attribute(GridType==Collection)
  for (int i = 0; i < node.numChildren(); i++) {
    tmp = node.getChild(i);
    if (tmp.getTag() == "Grid" && tmp.hasAttribute("GridType")) {
      if (tmp.getAttribute("GridType") == "Collection") {
        return tmp;
      }
    }
  }
  
  // TODO(barker): return some error indicator
  return node;
}

Teuchos::XMLObject HDF5_MPI::findMeshNode_(Teuchos::XMLObject xmlobject) {
  
  Teuchos::XMLObject node, tmp;
  
  // Step down to child tag==Domain
  for (int i = 0; i < xmlobject.numChildren(); i++) {
    if (xmlobject.getChild(i).getTag() == "Domain") {
      node = xmlobject.getChild(i);
    }
  }

  // Step down to child tag==Grid and Attribute(Name==Mesh)
  for (int i = 0; i < node.numChildren(); i++) {
    tmp = node.getChild(i);
    if (tmp.getTag() == "Grid" && tmp.hasAttribute("Name")) {
      if (tmp.getAttribute("Name") == "Mesh") {
        return tmp;
      }
    }
  }

  // TODO(barker): return some error indicator
  return node;
}

Teuchos::XMLObject HDF5_MPI::addXdmfAttribute_(std::string varname,
                                           std::string location,
                                           int length,
                                           std::string h5path) {
  Teuchos::XMLObject attribute("Attribute");
  attribute.addAttribute("Name", varname);
  attribute.addAttribute("Type", "Scalar");
  attribute.addAttribute("Center", location);

  Teuchos::XMLObject DataItem("DataItem");
  DataItem.addAttribute("Format", "HDF");
  DataItem.addInt("Dimensions", length);
  DataItem.addAttribute("DataType", "Float");
  std::stringstream tmp;
  tmp << H5DataFilename() << ":" << h5path;
  DataItem.addContent(tmp.str());
  attribute.addChild(DataItem);

  return attribute;
}


std::string HDF5_MPI::xdmfHeader_ =
         "<?xml version=\"1.0\" ?>\n<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []>\n";
  
} // close namespace Amanzi

