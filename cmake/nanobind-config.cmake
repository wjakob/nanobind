include_guard(GLOBAL)

if (NOT TARGET Python::Module)
  message(FATAL_ERROR "You must invoke 'find_package(Python COMPONENTS Interpreter Development REQUIRED)' prior to including nanobind.")
endif()

# Determine the Python extension suffix and stash in the CMake cache
execute_process(
  COMMAND "${Python_EXECUTABLE}" "-c"
    "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))"
  RESULT_VARIABLE NB_SUFFIX_RET
  OUTPUT_VARIABLE NB_SUFFIX
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NB_SUFFIX_RET AND NOT NB_SUFFIX_RET EQUAL 0)
  message(FATAL_ERROR "nanobind: Python sysconfig query to "
    "find 'EXT_SUFFIX' property failed!")
endif()

set(NB_SUFFIX ${NB_SUFFIX} CACHE INTERNAL "")

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
# Helper function to handle undefined CPython API symbols on macOS
# ---------------------------------------------------------------------------

function (nanobind_link_options name)
  if (APPLE)
    if (Python_INTERPRETER_ID STREQUAL "PyPy")
      set(NB_LINKER_RESPONSE_FILE darwin-ld-pypy.sym)
    else()
      set(NB_LINKER_RESPONSE_FILE darwin-ld-cpython.sym)
    endif()
    target_link_options(${name} PRIVATE "-Wl,-dead_strip" "-Wl,@${NB_DIR}/cmake/${NB_LINKER_RESPONSE_FILE}")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Create shared/static library targets for nanobind's non-templated core
# ---------------------------------------------------------------------------

function (nanobind_build_library TARGET_NAME)
  if (TARGET ${TARGET_NAME})
    return()
  endif()

  if (TARGET_NAME MATCHES "-static")
    set (TARGET_TYPE STATIC)
  else()
    set (TARGET_TYPE SHARED)
  endif()

  add_library(${TARGET_NAME} ${TARGET_TYPE}
    EXCLUDE_FROM_ALL
    ${NB_DIR}/include/nanobind/make_iterator.h
    ${NB_DIR}/include/nanobind/nanobind.h
    ${NB_DIR}/include/nanobind/nb_accessor.h
    ${NB_DIR}/include/nanobind/nb_attr.h
    ${NB_DIR}/include/nanobind/nb_call.h
    ${NB_DIR}/include/nanobind/nb_cast.h
    ${NB_DIR}/include/nanobind/nb_class.h
    ${NB_DIR}/include/nanobind/nb_defs.h
    ${NB_DIR}/include/nanobind/nb_descr.h
    ${NB_DIR}/include/nanobind/nb_enums.h
    ${NB_DIR}/include/nanobind/nb_error.h
    ${NB_DIR}/include/nanobind/nb_func.h
    ${NB_DIR}/include/nanobind/nb_lib.h
    ${NB_DIR}/include/nanobind/nb_misc.h
    ${NB_DIR}/include/nanobind/nb_python.h
    ${NB_DIR}/include/nanobind/nb_traits.h
    ${NB_DIR}/include/nanobind/nb_tuple.h
    ${NB_DIR}/include/nanobind/nb_types.h
    ${NB_DIR}/include/nanobind/ndarray.h
    ${NB_DIR}/include/nanobind/trampoline.h
    ${NB_DIR}/include/nanobind/operators.h
    ${NB_DIR}/include/nanobind/stl/array.h
    ${NB_DIR}/include/nanobind/stl/bind_map.h
    ${NB_DIR}/include/nanobind/stl/bind_vector.h
    ${NB_DIR}/include/nanobind/stl/detail
    ${NB_DIR}/include/nanobind/stl/detail/nb_array.h
    ${NB_DIR}/include/nanobind/stl/detail/nb_dict.h
    ${NB_DIR}/include/nanobind/stl/detail/nb_list.h
    ${NB_DIR}/include/nanobind/stl/detail/nb_set.h
    ${NB_DIR}/include/nanobind/stl/detail/traits.h
    ${NB_DIR}/include/nanobind/stl/filesystem.h
    ${NB_DIR}/include/nanobind/stl/function.h
    ${NB_DIR}/include/nanobind/stl/list.h
    ${NB_DIR}/include/nanobind/stl/map.h
    ${NB_DIR}/include/nanobind/stl/optional.h
    ${NB_DIR}/include/nanobind/stl/pair.h
    ${NB_DIR}/include/nanobind/stl/set.h
    ${NB_DIR}/include/nanobind/stl/shared_ptr.h
    ${NB_DIR}/include/nanobind/stl/string.h
    ${NB_DIR}/include/nanobind/stl/string_view.h
    ${NB_DIR}/include/nanobind/stl/tuple.h
    ${NB_DIR}/include/nanobind/stl/unique_ptr.h
    ${NB_DIR}/include/nanobind/stl/unordered_map.h
    ${NB_DIR}/include/nanobind/stl/unordered_set.h
    ${NB_DIR}/include/nanobind/stl/variant.h
    ${NB_DIR}/include/nanobind/stl/vector.h
    ${NB_DIR}/include/nanobind/eigen/dense.h
    ${NB_DIR}/include/nanobind/eigen/sparse.h

    ${NB_DIR}/src/buffer.h
    ${NB_DIR}/src/nb_internals.h
    ${NB_DIR}/src/nb_internals.cpp
    ${NB_DIR}/src/nb_func.cpp
    ${NB_DIR}/src/nb_type.cpp
    ${NB_DIR}/src/nb_enum.cpp
    ${NB_DIR}/src/nb_ndarray.cpp
    ${NB_DIR}/src/common.cpp
    ${NB_DIR}/src/error.cpp
    ${NB_DIR}/src/trampoline.cpp
    ${NB_DIR}/src/implicit.cpp
  )

  if (TARGET_TYPE STREQUAL "SHARED")
    nanobind_link_options(${TARGET_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE -DNB_BUILD)
    target_compile_definitions(${TARGET_NAME} PUBLIC -DNB_SHARED)
    nanobind_strip(${TARGET_NAME})
  endif()

  if ((TARGET_TYPE STREQUAL "SHARED") OR (TARGET_NAME MATCHES "-lto"))
    set_target_properties(${TARGET_NAME} PROPERTIES
      INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
      INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

  if (MSVC)
    # Do not complain about vsnprintf
    target_compile_definitions(${TARGET_NAME} PRIVATE -D_CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -fno-strict-aliasing)
  endif()

  if (WIN32)
    if (${TARGET_NAME} MATCHES "abi3")
      target_link_libraries(${TARGET_NAME} PUBLIC Python::SABIModule)
    else()
      target_link_libraries(${TARGET_NAME} PUBLIC Python::Module)
    endif()
  endif()

  target_include_directories(${TARGET_NAME} PRIVATE
    ${NB_DIR}/ext/robin_map/include)

  target_include_directories(${TARGET_NAME} PUBLIC
    ${Python_INCLUDE_DIRS}
    ${NB_DIR}/include)

  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
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
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX "${NB_SUFFIX}")
endfunction()

function(nanobind_extension_abi3 name)
  get_filename_component(ext "${NB_SUFFIX}" LAST_EXT)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX ".abi3${ext}")
endfunction()

function (nanobind_lto name)
  set_target_properties(${name} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
endfunction()

function (nanobind_compile_options)
  if (MSVC)
    target_compile_options(${name} PRIVATE /bigobj /MP)
  endif()
endfunction()

function(nanobind_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NOMINSIZE;STABLE_ABI;NOSTRIP;NB_STATIC;NB_SHARED;PROTECT_STACK;LTO" "" "")

  add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})

  nanobind_compile_options(${name})
  nanobind_link_options(${name})
  set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)

  if (ARG_NB_SHARED AND ARG_NB_STATIC)
    message(FATAL_ERROR "NB_SHARED and NB_STATIC cannot be specified at the same time!")
  elseif (NOT ARG_NB_SHARED)
    set(ARG_NB_STATIC TRUE)
  endif()

  # Stable ABI interface requires Python >= 3.12
  if (ARG_STABLE_ABI AND (Python_VERSION_MAJOR EQUAL 3) AND (Python_VERSION_MINOR LESS 12))
    set(ARG_STABLE_ABI OFF)
  endif()

  # Stable API interface requires CPython (PyPy isn't supported)
  if (ARG_STABLE_ABI AND NOT (Python_INTERPRETER_ID STREQUAL "Python"))
    set(ARG_STABLE_ABI OFF)
  endif()

  # On Windows, use of the stable ABI requires a very recent CMake version
  if (ARG_STABLE_API AND WIN32 AND NOT TARGET Python::SABIModule)
    if (CMAKE_VERSION VERSION_LESS 3.26)
      message(WARNING "To build stable ABI packages on Windows, you must use CMake version 3.26 or newer!")
    else()
      message(WARNING "To build stable ABI packages on Windows, you must invoke 'find_package(Python COMPONENTS Interpreter Development.Module Development.SABIModule REQUIRED)' prior to including nanobind.")
    endif()
    set(ARG_STABLE_ABI OFF)
  endif()

  set(libname "nanobind")
  if (ARG_NB_STATIC)
    set(libname "${libname}-static")
  endif()

  if (ARG_STABLE_ABI)
    set(libname "${libname}-abi3")
  endif()

  # Shared builds always use LTO for release builds of the library component
  if (ARG_LTO AND NOT ARG_NB_STATIC)
    set(libname "${libname}-lto")
  endif()

  nanobind_build_library(${libname})

  if (ARG_STABLE_ABI)
    target_compile_definitions(${libname} PUBLIC -DPy_LIMITED_API=0x030C0000)
    nanobind_extension_abi3(${name})
  else()
    nanobind_extension(${name})
  endif()

  target_link_libraries(${name} PRIVATE ${libname})

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

  set_target_properties(${name} PROPERTIES CXX_VISIBILITY_PRESET hidden)
endfunction()
