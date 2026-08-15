cmake_minimum_required(VERSION 3.15...3.30)
project(nanobind_backend LANGUAGES CXX)

# This project lives inside the nanobind repository and bundles the backend of
# the surrounding checkout. It is distributed as binary wheels only.
set(NB_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/nanobind-config.cmake)
if (NOT EXISTS ${NB_CONFIG})
  message(FATAL_ERROR "nanobind-backend builds only from a full nanobind "
    "repository checkout and is distributed as binary wheels; there is no "
    "source distribution. See the split-mode documentation for building a "
    "custom backend module on an unsupported platform.")
endif()

if (NOT SKBUILD)
  message(WARNING "This project is normally built as a wheel via "
    "scikit-build-core, e.g. 'pip wheel .' from this directory.")
endif()

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)

include(${NB_CONFIG})

# The version in pyproject.toml mirrors the macros in nb_backend.h (both are
# written by 'src/version.py -b'); refuse to build a mislabeled wheel
file(STRINGS ${NB_DIR}/include/nanobind/nb_backend.h lines
     REGEX "^#define NB_BACKEND_(ABI_MAJOR|ABI_MINOR|REVISION) ")
string(REGEX MATCH "ABI_MAJOR ([0-9]+)" _ "${lines}")
set(version "${CMAKE_MATCH_1}")
string(REGEX MATCH "ABI_MINOR ([0-9]+)" _ "${lines}")
set(version "${version}.${CMAKE_MATCH_1}")
string(REGEX MATCH "REVISION ([0-9]+)" _ "${lines}")
set(version "${version}.${CMAKE_MATCH_1}")
string(REPLACE "." "\\." pattern "${version}")
if (SKBUILD AND NOT SKBUILD_PROJECT_VERSION MATCHES "^${pattern}(\\.dev[0-9]+)?$")
  message(FATAL_ERROR "nanobind-backend/pyproject.toml declares version "
    "${SKBUILD_PROJECT_VERSION}, but nb_backend.h declares ${version}. Run "
    "'python src/version.py -b <version>' to synchronize them.")
endif()

# The backend module automatically follows the free-threading flavor of the
# interpreter that the wheel targets
nanobind_add_backend(_nb_backend_v1)

install(TARGETS _nb_backend_v1 LIBRARY DESTINATION nanobind_backend)
