cmake_minimum_required(VERSION 3.15...3.30)
project(nanobind_wasm_smoke LANGUAGES CXX)

# Builds against the surrounding nanobind checkout. See tests/wasm/README.md.
set(NB_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/nanobind-config.cmake)
if (NOT EXISTS ${NB_CONFIG})
  message(FATAL_ERROR "tests/wasm builds only from a full nanobind "
    "repository checkout.")
endif()

find_package(Python 3.10
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

include(${NB_CONFIG})

nanobind_add_module(smoke_ext smoke.cpp STABLE_ABI)

install(TARGETS smoke_ext LIBRARY DESTINATION .)
