cmake_minimum_required(VERSION 3.17...3.22)
project(nanobind)

# ---------------------------------------------------------------------------
# Do a release build if nothing was specified
# ---------------------------------------------------------------------------

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "${PN}: setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

# ---------------------------------------------------------------------------
# Find the Python interpreter and development libraries
# ---------------------------------------------------------------------------

find_package(Python COMPONENTS Interpreter Development)

# ---------------------------------------------------------------------------
# Build a static library containing the non-templated core of nanobind
# ---------------------------------------------------------------------------

add_library(nanobind-core STATIC
  src/common.cpp
  src/func.cpp
)

target_compile_features(nanobind-core PRIVATE cxx_std_17)

set_target_properties(nanobind-core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON)

target_include_directories(nanobind-core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Python_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Define a convenient function for creating nanobind targets
# ---------------------------------------------------------------------------

function(nanobind_add_module name)
  if (MSVC)
    set(NB_OPT_SIZE /Os)
  else()
    set(NB_OPT_SIZE -Os)
  endif()

  set(NB_SUFFIX ".so")
  set(NB_COMPILE_OPTIONS
    $<$<CONFIG:Release>:${NB_OPT_SIZE}>
    $<$<CONFIG:MinSizeRel>:${NB_OPT_SIZE}>
    $<$<CONFIG:RelWithDebInfo>:${NB_OPT_SIZE}>)

  if(MSVC)
    set(NB_COMPILE_OPTIONS ${NB_COMPILE_OPTIONS} /bigobj /MP)
    set(NB_SUFFIX ".pyd")
  endif()

  add_library(${name} MODULE ${ARGV1})
  set_target_properties(${name} PROPERTIES CXX_VISIBILITY_PRESET hidden)
  target_compile_features(${name} PRIVATE cxx_std_17)
  target_compile_options(${name} PRIVATE ${NB_COMPILE_OPTIONS})
  target_link_libraries(${name} nanobind-core)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX ".${Python_SOABI}${NB_SUFFIX}")
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Python_INCLUDE_DIRS})

  if (APPLE)
    target_link_options(${name} PRIVATE -undefined dynamic_lookup)
  endif()
endfunction()

nanobind_add_module(
  nbtest
  nbtest.cpp
)
