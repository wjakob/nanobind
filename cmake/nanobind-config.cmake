include_guard(GLOBAL)

if (NOT TARGET Python::Module)
  message(FATAL_ERROR "You must invoke 'find_package(Python COMPONENTS Interpreter Development REQUIRED)' prior to including nanobind.")
endif()

get_filename_component(NB_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(NB_DIR "${NB_DIR}" PATH)
set(NB_DIR ${NB_DIR} CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# Helper function to strip unnecessary sections from binaries on Linux/macOS
# ---------------------------------------------------------------------------
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


# ---------------------------------------------------------------------------
# Create shared/static library targets for nanobind's non-templated core
# ---------------------------------------------------------------------------

function (nanobuild_build_library TARGET_NAME TARGET_TYPE)
  if (TARGET ${TARGET_NAME})
    return()
  endif()

  add_library(${TARGET_NAME} ${TARGET_TYPE}
    EXCLUDE_FROM_ALL
    ${NB_DIR}/include/nanobind/nanobind.h
    ${NB_DIR}/include/nanobind/nb_attr.h
    ${NB_DIR}/include/nanobind/nb_cast.h
    ${NB_DIR}/include/nanobind/nb_descr.h
    ${NB_DIR}/include/nanobind/nb_error.h
    ${NB_DIR}/include/nanobind/nb_lib.h
    ${NB_DIR}/include/nanobind/nb_python.h
    ${NB_DIR}/include/nanobind/nb_tuple.h
    ${NB_DIR}/include/nanobind/nb_accessor.h
    ${NB_DIR}/include/nanobind/nb_call.h
    ${NB_DIR}/include/nanobind/nb_class.h
    ${NB_DIR}/include/nanobind/nb_defs.h
    ${NB_DIR}/include/nanobind/nb_enums.h
    ${NB_DIR}/include/nanobind/nb_func.h
    ${NB_DIR}/include/nanobind/nb_misc.h
    ${NB_DIR}/include/nanobind/nb_traits.h
    ${NB_DIR}/include/nanobind/nb_types.h
    ${NB_DIR}/include/nanobind/trampoline.h
    ${NB_DIR}/include/nanobind/stl/string.h

    ${NB_DIR}/src/internals.h
    ${NB_DIR}/src/buffer.h
    ${NB_DIR}/src/internals.cpp
    ${NB_DIR}/src/common.cpp
    ${NB_DIR}/src/nb_func.cpp
    ${NB_DIR}/src/nb_type.cpp
    ${NB_DIR}/src/nb_enum.cpp
    ${NB_DIR}/src/error.cpp
    ${NB_DIR}/src/trampoline.cpp
    ${NB_DIR}/src/implicit.cpp
  )

  if (TARGET_TYPE STREQUAL "SHARED")
    if (APPLE)
      target_link_options(${TARGET_NAME} PRIVATE -undefined dynamic_lookup)
    endif()

    target_compile_definitions(${TARGET_NAME} PRIVATE -DNB_BUILD)
    target_compile_definitions(${TARGET_NAME} PUBLIC -DNB_SHARED)
    nanobind_strip(${TARGET_NAME})
  endif()

  if (MSVC)
    # C++20 needed for designated initializers on MSVC..
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    # Do not complain about vsnprintf
    target_compile_definitions(${TARGET_NAME} PRIVATE -D_CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
    target_compile_options(${TARGET_NAME} PRIVATE -fno-strict-aliasing)
  endif()

  if (WIN32)
    target_link_libraries(${TARGET_NAME} PUBLIC Python::Module)
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
  )

  target_include_directories(${TARGET_NAME} PRIVATE
    ${NB_DIR}/include
    ${NB_DIR}/ext/robin_map/include
    ${Python_INCLUDE_DIRS})
endfunction()

# ---------------------------------------------------------------------------
# Define a convenience function for creating nanobind targets
# ---------------------------------------------------------------------------

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

function (nanobind_headers name)
  target_include_directories(${name} PRIVATE ${NB_DIR}/include)
endfunction()

function(nanobind_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NOMINSIZE;NOSTRIP;NB_STATIC;NB_SHARED;PROTECT_STACK;LTO" "" "")

  Python_add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})

  nanobind_cpp17(${name})
  nanobind_extension(${name})
  nanobind_msvc(${name})
  nanobind_headers(${name})

  if (ARG_NB_STATIC)
    nanobuild_build_library(nanobind-static STATIC)
    target_link_libraries(${name} PRIVATE nanobind-static)
  else()
    nanobuild_build_library(nanobind SHARED)
    target_link_libraries(${name} PRIVATE nanobind)
  endif()

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
