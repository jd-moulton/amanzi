#
# Build TPL:  SuperLU
#   
# --- Define all the directories and common external project flags
define_external_project_args(SuperLU
                             TARGET superlu)

# Add version version to the autogenerated tpl_versions.h file
include(${SuperBuild_SOURCE_DIR}/TPLVersions.cmake)
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
  PREFIX SuperLU
  VERSION ${SuperLU_VERSION_MAJOR} ${SuperLU_VERSION_MINOR} ${SuperLU_VERSION_PATCH})
  
# --- Patch the original code
set(SuperLU_patch_file superlu-osx-shared.patch)
set(SuperLU_sh_patch ${SuperLU_prefix_dir}/superlu-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/superlu-patch-step.sh.in
               ${SuperLU_sh_patch}
               @ONLY)
# configure the CMake patch step
set(SuperLU_cmake_patch ${SuperLU_prefix_dir}/superlu-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/superlu-patch-step.cmake.in
               ${SuperLU_cmake_patch}
               @ONLY)
# set the patch command
set(SuperLU_PATCH_COMMAND ${CMAKE_COMMAND} -P ${SuperLU_cmake_patch})

# --- Define the arguments passed to CMake.
set(SuperLU_CMAKE_ARGS 
      "-DCMAKE_INSTALL_PREFIX:FILEPATH=${TPL_INSTALL_PREFIX}"
      "-DCMAKE_INSTALL_LIBDIR:FILEPATH=${TPL_INSTALL_PREFIX}/lib"
      "-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}"
      "-Denable_blaslib:BOOL=FALSE")

# --- Add external project build and tie to the SuperLU build target
ExternalProject_Add(${SuperLU_BUILD_TARGET}
                    DEPENDS   ${SuperLU_PACKAGE_DEPENDS}       # Package dependency target
                    TMP_DIR   ${SuperLU_tmp_dir}               # Temporary files directory
                    STAMP_DIR ${SuperLU_stamp_dir}             # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}           # Download directory
                    URL          ${SuperLU_URL}                # URL may be a web site OR a local file
                    URL_MD5      ${SuperLU_MD5_SUM}            # md5sum of the archive file
                    PATCH_COMMAND ${SuperLU_PATCH_COMMAND}     # Mods to source
                    # -- Configure
                    SOURCE_DIR    ${SuperLU_source_dir}        # Source directory
                    CMAKE_ARGS    ${SuperLU_CMAKE_ARGS}        # CMAKE_CACHE_ARGS or CMAKE_ARGS => CMake configure
                                  -DCMAKE_C_FLAGS:STRING=${Amanzi_COMMON_CFLAGS}  # Ensure uniform build
                                  -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                                  -DCMAKE_Fortran_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}

                    # -- Build
                    BINARY_DIR      ${SuperLU_build_dir}       # Build directory 
                    BUILD_COMMAND   $(MAKE)
                    # -- Install
                    INSTALL_DIR     ${TPL_INSTALL_PREFIX}      # Install directory
                    INSTALL_COMMAND $(MAKE) install
                    # -- Output control
                    ${SuperLU_logging_args})

include(BuildLibraryName)
build_library_name(superlu SuperLU_LIB APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)

# --- set cache (global) variables
global_set(SuperLU_LIBRARY "${SuperLU_LIB}")
