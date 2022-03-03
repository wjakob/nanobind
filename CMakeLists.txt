cmake_minimum_required(VERSION 3.17...3.22)
project(nanobind)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(NB_TEST_DEFAULT ON)
else()
  set(NB_TEST_DEFAULT OFF)
endif()

option(NB_SHARED "Should nanobind produce a shared core library?" ON)
option(NB_TEST   "Compile nanobind tests?" ${NB_TEST_DEFAULT})

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
# Compile with a few more compiler warnings turned on
# ---------------------------------------------------------------------------

if (MSVC)
  if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    add_compile_options(/W4)
  endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wall -Wextra -Wno-unused-local-typedefs)
endif()

# ---------------------------------------------------------------------------
# Find the Python interpreter and development libraries
# ---------------------------------------------------------------------------

set(Python_FIND_FRAMEWORK LAST) # Prefer Brew/Conda to Apple framework python
find_package(Python COMPONENTS Interpreter Development REQUIRED)
if ((Python_VERSION_MAJOR LESS 3) OR ((Python_VERSION_MAJOR EQUAL 3) AND (Python_VERSION_MINOR LESS 8)))
  message(FATAL_ERROR "nanobind requires Python >= 3.8")
endif()

# ---------------------------------------------------------------------------
# Build a static library containing the non-templated core of nanobind
# ---------------------------------------------------------------------------

if (NB_SHARED)
  set(NB_LIB_TYPE SHARED)
else()
  set(NB_LIB_TYPE STATIC)
endif()

add_library(nanobind ${NB_LIB_TYPE}
  include/nanobind/nanobind.h
  include/nanobind/nb_attr.h
  include/nanobind/nb_cast.h
  include/nanobind/nb_descr.h
  include/nanobind/nb_error.h
  include/nanobind/nb_lib.h
  include/nanobind/nb_python.h
  include/nanobind/nb_tuple.h
  include/nanobind/nb_accessor.h
  include/nanobind/nb_call.h
  include/nanobind/nb_class.h
  include/nanobind/nb_defs.h
  include/nanobind/nb_enums.h
  include/nanobind/nb_func.h
  include/nanobind/nb_misc.h
  include/nanobind/nb_traits.h
  include/nanobind/nb_types.h
  include/nanobind/trampoline.h
  include/nanobind/stl/string.h

  src/internals.h
  src/buffer.h
  src/internals.cpp
  src/common.cpp
  src/nb_func.cpp
  src/nb_type.cpp
  src/nb_enum.cpp
  src/error.cpp
  src/trampoline.cpp
  src/implicit.cpp
)

if (NB_SHARED)
  if (APPLE)
    target_link_options(nanobind PRIVATE -undefined dynamic_lookup)
  endif()

  target_compile_definitions(nanobind PRIVATE -DNB_BUILD)
  target_compile_definitions(nanobind PUBLIC -DNB_SHARED)
endif()

if (MSVC)
  # Properly report the C++ version, just like other compilers do.
  target_compile_options(nanobind PUBLIC /Zc:__cplusplus)
endif()

if (MSVC)
  # C++20 needed for designated initializers on MSVC..
  target_compile_features(nanobind PRIVATE cxx_std_20)
else()
  target_compile_features(nanobind PRIVATE cxx_std_17)
  target_compile_options(nanobind PRIVATE -fno-strict-aliasing)
endif()

if (WIN32)
    target_link_libraries(nanobind PUBLIC Python::Module)
endif()

set_target_properties(nanobind PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
  INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
)

target_include_directories(nanobind PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/ext/robin_map/include
  ${Python_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Define a convenience function for creating nanobind targets
# ---------------------------------------------------------------------------

# Strip unnecessary sections of the binary on Linux/macOS
function(nanobind_strip name)
  if (CMAKE_STRIP AND NOT MSVC AND NOT CMAKE_BUILD_TYPE MATCHES Debug|RelWithDebInfo)
    if(APPLE)
      set(NB_STRIP_OPT -x)
    endif()

    add_custom_command(
      TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_STRIP} ${NB_STRIP_OPT} $<TARGET_FILE:${name}>)
  endif()
endfunction()

function(nanobind_opt_size name)
  if (MSVC)
    set(NB_OPT_SIZE /Os)
  else()
    set(NB_OPT_SIZE -Os)
  endif()

  target_compile_options(${name} PRIVATE
      $<$<CONFIG:Release>:${NB_OPT_SIZE}>
      $<$<CONFIG:MinSizeRel>:${NB_OPT_SIZE}>
      $<$<CONFIG:RelWithDebInfo>:${NB_OPT_SIZE}>)
endfunction()

function(nanobind_disable_stack_protector name)
  if (NOT MSVC)
    # The stack protector affects binding size negatively (+8% on Linux in my
    # benchmarks). Protecting from stack smashing in a Python VM seems in any
    # case futile, so let's get rid of it by default in optimized modes.
    target_compile_options(${name} PRIVATE
        $<$<CONFIG:Release>:-fno-stack-protector>
        $<$<CONFIG:MinSizeRel>:-fno-stack-protector>
        $<$<CONFIG:RelWithDebInfo>:-fno-stack-protector>)
  endif()
endfunction()

function(nanobind_extension name)
  if (WIN32)
    set(NB_SUFFIX ".pyd")
  else()
    set(NB_SUFFIX ".so")
  endif()

  set_target_properties(${name} PROPERTIES
    PREFIX "" SUFFIX ".${Python_SOABI}${NB_SUFFIX}")
endfunction()

function (nanobind_cpp17 name)
  target_compile_features(${name} PRIVATE cxx_std_17)
  set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

function (nanobind_msvc)
  if (MSVC)
    target_compile_options(${name} PRIVATE /bigobj /MP)
  endif()
endfunction()

function (nanobind_lto name)
  set_target_properties(${name} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
endfunction()

set(NB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "")
function (nanobind_headers name)
  target_include_directories(${name} PRIVATE ${NB_INCLUDE_DIR})
endfunction()

function (nanobind_lib name)
  target_link_libraries(${name} PRIVATE nanobind)
endfunction()

function(nanobind_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NOMINSIZE;NOSTRIP;PROTECT_STACK;LTO" "" "")

  Python_add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})

  nanobind_cpp17(${name})
  nanobind_extension(${name})
  nanobind_msvc(${name})
  nanobind_headers(${name})
  nanobind_lib(${name})

  if (NOT ARG_PROTECT_STACK)
    nanobind_disable_stack_protector(${name})
  endif()

  if (NOT ARG_NOMINSIZE)
    nanobind_opt_size(${name})
  endif()

  if (NOT ARG_NOSTRIP)
    nanobind_strip(${name})
  endif()

  if (ARG_LTO)
    nanobind_lto(${name})
  endif()
endfunction()

nanobind_strip(nanobind)

if (NB_TEST)
  add_subdirectory(tests)
endif()
