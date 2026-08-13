cmake_minimum_required(VERSION 3.15...3.30)
project(nanobind_backend LANGUAGES CXX)

if (NOT SKBUILD)
  message(WARNING "This project is normally built as a wheel via "
    "scikit-build-core, e.g. 'pip wheel .' from this directory.")
endif()

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)

# This project lives inside the nanobind repository and bundles the backend
# of the surrounding checkout
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/nanobind-config.cmake)

# The backend module automatically follows the free-threading flavor of the
# interpreter that the wheel targets
nanobind_add_backend(_nb_backend_v1)

install(TARGETS _nb_backend_v1 LIBRARY DESTINATION nanobind_backend)
