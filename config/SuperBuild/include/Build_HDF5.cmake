#  -*- mode: cmake -*-

#
# Build TPL:  HDF5 
#    
# --- Define all the directories and common external project flags
define_external_project_args(HDF5
                             TARGET hdf5
                             DEPENDS ZLIB)

# add HDF5 version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX HDF5
  VERSION ${HDF5_VERSION_MAJOR} ${HDF5_VERSION_MINOR} ${HDF5_VERSION_PATCH})

# --- Patch the original code
set(HDF5_patch_file hdf5-1.10.6-rpath.patch)
set(HDF5_sh_patch ${HDF5_prefix_dir}/hdf5-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/hdf5-patch-step.sh.in
               ${HDF5_sh_patch}
               @ONLY)
# configure the CMake patch step
set(HDF5_cmake_patch ${HDF5_prefix_dir}/hdf5-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/hdf5-patch-step.cmake.in
               ${HDF5_cmake_patch}
               @ONLY)
# set the patch command
set(HDF5_PATCH_COMMAND ${CMAKE_COMMAND} -P ${HDF5_cmake_patch})     

# --- Define configure parameters
set(HDF5_CMAKE_CACHE_ARGS "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}")
if ( (${AMANZI_ARCH_NERSC} OR ${AMANZI_ARCH_CHICOMA}) AND (NOT BUILD_SHARED_LIBS))
   list(APPEND HDF5_CMAKE_CACHE_ARGS "-DBUILD_STATIC_EXECS:BOOL=ON")
endif()

list(APPEND HDF5_CMAKE_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DHDF5_ENABLE_PARALLEL:BOOL=TRUE")
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DMPI_HOME:PATH=${MPI_PREFIX}")

# Fortran buindings are needed for PFLoTran, although they should be 
# optional of reaction library only
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DHDF5_BUILD_FORTRAN:BOOL=TRUE")

list(APPEND HDF5_CMAKE_CACHE_ARGS "-DHDF5_BUILD_CPP_LIB:BOOL=FALSE")
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DHDF5_ENABLE_Z_LIB_SUPPORT:BOOL=TRUE")
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DHDF5_BUILD_HL_LIB:BOOL=TRUE")
list(APPEND HDF5_CMAKE_CACHE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${TPL_INSTALL_PREFIX}")
#-D ZLIB_INCLUDE_DIR=${ZLIB_install_dir}/include
#-D ZLIB_LIBRARY=${ZLIB_install_dir}/lib/libz.a

# --- Add external project build and tie to the HDF5 build target
ExternalProject_Add(${HDF5_BUILD_TARGET}
                    DEPENDS   ${HDF5_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${HDF5_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${HDF5_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}              # Download directory
                    URL          ${HDF5_URL}                      # URL may be a web site OR a local file
                    URL_MD5      ${HDF5_MD5_SUM}                  # md5sum of the archive file
                    # -- Patch 
                    PATCH_COMMAND ${HDF5_PATCH_COMMAND}
                    # -- Configure
                    SOURCE_DIR    ${HDF5_source_dir} 
                    CMAKE_ARGS    ${AMANZI_CMAKE_CACHE_ARGS}      # Ensure unifom build
                                  ${HDF5_CMAKE_CACHE_ARGS}
                                  -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                  -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                  -DCMAKE_CXX_FLAGS:STRING=${Amanzi_COMMON_CXXFLAGS}
                                  -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
                    # -- Build
                    BINARY_DIR        ${HDF5_build_dir}           # Build directory 
                    BUILD_COMMAND     $(MAKE)                     # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE   ${HDF5_BUILD_IN_SOURCE}     # Flag for in source builds
                    # -- Install
                    INSTALL_DIR       ${TPL_INSTALL_PREFIX}       # Install directory
                    # -- Output control
                    ${HDF5_logging_args})

# --- Useful variables for packages that depend on HDF5 (NetCDF)
include(BuildLibraryName)

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
  build_library_name(hdf5_debug HDF5_C_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
  build_library_name(hdf5_hl_debug HDF5_HL_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
else()
  build_library_name(hdf5 HDF5_C_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
  build_library_name(hdf5_hl HDF5_HL_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
endif()
  
set(HDF5_LIBRARIES ${HDF5_HL_LIBRARY} ${HDF5_C_LIBRARY} ${ZLIB_LIBRARIES} m dl)
set(HDF5_INCLUDE_DIRS ${TPL_INSTALL_PREFIX}/include ${ZLIB_INCLUDE_DIRS})
list(REMOVE_DUPLICATES HDF5_INCLUDE_DIRS)

message(STATUS "\t HDF5:  HDF5_HL_LIBRARY = ${HDF5_HL_LIBRARY}")
message(STATUS "\t        HDF5_C_LIBRARY  = ${HDF5_C_LIBRARY}")
message(STATUS "\t        ZLIB_LIBRARIES  = ${ZLIB_LIBRARIES}")

set(HDF5_DIR ${TPL_INSTALL_PREFIX})

