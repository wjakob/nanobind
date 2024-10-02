cmake_minimum_required(VERSION 3.15...3.27)

# ---------------------------------------------------------------------------
# Read the project version from nanobind.h
# ---------------------------------------------------------------------------

file(STRINGS "include/nanobind/nanobind.h" _nanobind_h_version REGEX "^#define NB_VERSION_.*$")
string(REGEX MATCH "#define NB_VERSION_MAJOR ([0-9]+)" _ "${_nanobind_h_version}")
set(_major ${CMAKE_MATCH_1})
string(REGEX MATCH "#define NB_VERSION_MINOR ([0-9]+)" _ "${_nanobind_h_version}")
set(_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "#define NB_VERSION_PATCH ([0-9]+)" _ "${_nanobind_h_version}")
set(_patch ${CMAKE_MATCH_1})

project(nanobind LANGUAGES NONE VERSION "${_major}.${_minor}.${_patch}")

# ---------------------------------------------------------------------------
# Only build tests by default if this is the top-level CMake project
# ---------------------------------------------------------------------------

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(NB_MASTER_PROJECT ON)
else()
  set(NB_MASTER_PROJECT OFF)
endif()

option(NB_CREATE_INSTALL_RULES "Create installation rules" ${NB_MASTER_PROJECT})
option(NB_USE_SUBMODULE_DEPS   "Use the nanobind dependencies shipped as a git submodule of this repository" ON)

option(NB_TEST               "Compile nanobind tests?" ${NB_MASTER_PROJECT})
option(NB_TEST_STABLE_ABI    "Test the stable ABI interface?" OFF)
option(NB_TEST_SHARED_BUILD  "Build a shared nanobind library for the test suite?" OFF)
option(NB_TEST_CUDA          "Force the use of the CUDA/NVCC compiler for testing purposes" OFF)
option(NB_TEST_FREE_THREADED "Build free-threaded extensions for the test suite?" ON)

if (NOT MSVC)
  option(NB_TEST_SANITIZERS_ASAN  "Build tests with the address sanitizer?" OFF)
  option(NB_TEST_SANITIZERS_UBSAN "Build tests with the address undefined behavior sanitizer?" OFF)
  option(NB_TEST_SANITIZERS_TSAN  "Build tests with the thread sanitizer?" OFF)
endif()

# ---------------------------------------------------------------------------
# Do a release build if nothing was specified
# ---------------------------------------------------------------------------

if (NB_MASTER_PROJECT AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "nanobind: setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

# ---------------------------------------------------------------------------
# Check whether all dependencies are present
# ---------------------------------------------------------------------------

if (NB_USE_SUBMODULE_DEPS AND NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/robin_map/include")
  message(FATAL_ERROR "The nanobind dependencies are missing! "
    "You probably did not clone the project with --recursive. It is possible to recover "
    "by invoking\n$ git submodule update --init --recursive")
endif()

# ---------------------------------------------------------------------------
# Installation rules
# ---------------------------------------------------------------------------
if(NB_CREATE_INSTALL_RULES AND NOT CMAKE_SKIP_INSTALL_RULES)
  # Silence warning in GNUInstallDirs due to no enabled languages
  set(CMAKE_INSTALL_LIBDIR "")
  include(GNUInstallDirs)
  set(NB_INSTALL_DATADIR "nanobind"
    CACHE PATH "Installation path for read-only architecture-independent nanobind data files")

  # Normally these would be configurable by the user, but we can't allow that
  # because the lookup paths are hard-coded in 'cmake/nanobind-config.cmake'
  set(NB_INSTALL_INC_DIR "${NB_INSTALL_DATADIR}/include")
  set(NB_INSTALL_SRC_DIR "${NB_INSTALL_DATADIR}/src")
  set(NB_INSTALL_EXT_DIR "${NB_INSTALL_DATADIR}/ext")
  set(NB_INSTALL_MODULE_DIR "${NB_INSTALL_DATADIR}")
  set(NB_INSTALL_CMAKE_DIR "${NB_INSTALL_DATADIR}/cmake")

  install(
    DIRECTORY include/
    DESTINATION "${NB_INSTALL_INC_DIR}"
  )

  install(
    DIRECTORY src/
    DESTINATION "${NB_INSTALL_SRC_DIR}"
    PATTERN "*.py" EXCLUDE
  )

  install(
    DIRECTORY src/
    DESTINATION "${NB_INSTALL_MODULE_DIR}"
    FILES_MATCHING PATTERN "*\.py"
  )

  if(NB_USE_SUBMODULE_DEPS)
    install(
      FILES ext/robin_map/include/tsl/robin_map.h
            ext/robin_map/include/tsl/robin_hash.h
            ext/robin_map/include/tsl/robin_growth_policy.h
      DESTINATION "${NB_INSTALL_EXT_DIR}/robin_map/include/tsl"
    )
  endif()

  install(
    FILES cmake/nanobind-config.cmake
          cmake/darwin-ld-cpython.sym
          cmake/darwin-ld-pypy.sym
    DESTINATION "${NB_INSTALL_CMAKE_DIR}"
  )

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    COMPATIBILITY SameMajorVersion
  )
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    DESTINATION "${NB_INSTALL_CMAKE_DIR}"
  )
endif()

# Return early to skip finding needless dependencies if the user only wishes to
# install nanobind
if (NB_MASTER_PROJECT AND NOT NB_TEST)
  return()
else()
  enable_language(CXX)
endif()

# ---------------------------------------------------------------------------
# Find the Python interpreter and development libraries
# ---------------------------------------------------------------------------

if (NOT TARGET Python::Module OR NOT TARGET Python::Interpreter)
  set(Python_FIND_FRAMEWORK LAST) # Prefer Brew/Conda to Apple framework python

  if (CMAKE_VERSION VERSION_LESS 3.18)
    set(NB_PYTHON_DEV_MODULE Development)
  else()
    set(NB_PYTHON_DEV_MODULE Development.Module)
  endif()

  find_package(Python 3.8
    REQUIRED COMPONENTS Interpreter ${NB_PYTHON_DEV_MODULE}
    OPTIONAL_COMPONENTS Development.SABIModule)
endif()

# ---------------------------------------------------------------------------
# Include nanobind cmake functionality
# ---------------------------------------------------------------------------
include(cmake/nanobind-config.cmake)

if (NB_TEST)
  add_subdirectory(tests)
endif()
