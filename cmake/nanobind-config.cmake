include_guard(GLOBAL)

if (NOT TARGET Python::Module)
  message(FATAL_ERROR "You must invoke 'find_package(Python COMPONENTS Interpreter Development REQUIRED)' prior to including nanobind.")
endif()

# Determine the right suffix for ordinary and stable ABI extensions.

# We always need to know the extension
if(WIN32)
  set(NB_SUFFIX_EXT ".pyd")
else()
  set(NB_SUFFIX_EXT "${CMAKE_SHARED_MODULE_SUFFIX}")
endif()

# Check if FindPython/scikit-build-core defined a SOABI/SOSABI variable
if(DEFINED SKBUILD_SOABI)
  set(NB_SOABI "${SKBUILD_SOABI}")
elseif(DEFINED Python_SOABI)
  set(NB_SOABI "${Python_SOABI}")
endif()

if(DEFINED SKBUILD_SOSABI)
  set(NB_SOSABI "${SKBUILD_SOSABI}")
elseif(DEFINED Python_SOSABI)
  set(NB_SOSABI "${Python_SOSABI}")
endif()

# Error if scikit-build-core is trying to build Stable ABI < 3.12 wheels
if(DEFINED SKBUILD_SABI_VERSION AND SKBUILD_ABI_VERSION AND SKBUILD_SABI_VERSION VERSION_LESS "3.12")
  message(FATAL_ERROR "You must set tool.scikit-build.wheel.py-api to 'cp312' or later when "
                      "using scikit-build-core with nanobind, '${SKBUILD_SABI_VERSION}' is too old.")
endif()

# PyPy sets an invalid SOABI (platform missing), causing older FindPythons to
# report an incorrect value. Only use it if it looks correct (X-X-X form).
if(DEFINED NB_SOABI AND "${NB_SOABI}" MATCHES ".+-.+-.+")
  set(NB_SUFFIX ".${NB_SOABI}${NB_SUFFIX_EXT}")
endif()

if(DEFINED NB_SOSABI)
  if(NB_SOSABI STREQUAL "")
    set(NB_SUFFIX_S "${NB_SUFFIX_EXT}")
  else()
    set(NB_SUFFIX_S ".${NB_SOSABI}${NB_SUFFIX_EXT}")
  endif()
endif()

# Extract Python version and extensions (e.g. free-threaded build)
string(REGEX REPLACE "[^-]*-([^-]*)-.*" "\\1" NB_ABI "${NB_SOABI}")

# If either suffix is missing, call Python to compute it
if(NOT DEFINED NB_SUFFIX OR NOT DEFINED NB_SUFFIX_S)
  # Query Python directly to get the right suffix.
  execute_process(
    COMMAND "${Python_EXECUTABLE}" "-c"
      "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))"
    RESULT_VARIABLE NB_SUFFIX_RET
    OUTPUT_VARIABLE EXT_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NB_SUFFIX_RET AND NOT NB_SUFFIX_RET EQUAL 0)
    message(FATAL_ERROR "nanobind: Python sysconfig query to "
      "find 'EXT_SUFFIX' property failed!")
  endif()

  if(NOT DEFINED NB_SUFFIX)
    set(NB_SUFFIX "${EXT_SUFFIX}")
  endif()

  if(NOT DEFINED NB_SUFFIX_S)
    get_filename_component(NB_SUFFIX_EXT "${EXT_SUFFIX}" LAST_EXT)
    if(WIN32)
      set(NB_SUFFIX_S "${NB_SUFFIX_EXT}")
    else()
      set(NB_SUFFIX_S ".abi3${NB_SUFFIX_EXT}")
    endif()
  endif()
endif()

# Stash these for later use
set(NB_SUFFIX   ${NB_SUFFIX}   CACHE INTERNAL "")
set(NB_SUFFIX_S ${NB_SUFFIX_S} CACHE INTERNAL "")
set(NB_ABI      ${NB_ABI}      CACHE INTERNAL "")

get_filename_component(NB_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(NB_DIR "${NB_DIR}" PATH)

set(NB_DIR      ${NB_DIR} CACHE INTERNAL "")
set(NB_OPT      $<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>> CACHE INTERNAL "")
set(NB_OPT_SIZE $<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>> CACHE INTERNAL "")

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
    target_link_options(${name} PRIVATE "-Wl,@${NB_DIR}/cmake/${NB_LINKER_RESPONSE_FILE}")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Create shared/static library targets for nanobind's non-templated core
# ---------------------------------------------------------------------------

function (nanobind_build_library TARGET_NAME)
  cmake_parse_arguments(PARSE_ARGV 1 ARG
    "AS_SYSINCLUDE" "" "")

  if (TARGET ${TARGET_NAME})
    return()
  endif()

  if (TARGET_NAME MATCHES "-static")
    set (TARGET_TYPE STATIC)
  else()
    set (TARGET_TYPE SHARED)
  endif()

  if (${ARG_AS_SYSINCLUDE})
    set (AS_SYSINCLUDE SYSTEM)
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
    ${NB_DIR}/include/nanobind/typing.h
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
    ${NB_DIR}/src/hash.h
    ${NB_DIR}/src/nb_internals.h
    ${NB_DIR}/src/nb_internals.cpp
    ${NB_DIR}/src/nb_func.cpp
    ${NB_DIR}/src/nb_type.cpp
    ${NB_DIR}/src/nb_enum.cpp
    ${NB_DIR}/src/nb_ndarray.cpp
    ${NB_DIR}/src/nb_static_property.cpp
    ${NB_DIR}/src/nb_ft.h
    ${NB_DIR}/src/nb_ft.cpp
    ${NB_DIR}/src/common.cpp
    ${NB_DIR}/src/error.cpp
    ${NB_DIR}/src/trampoline.cpp
    ${NB_DIR}/src/implicit.cpp
  )

  if (TARGET_TYPE STREQUAL "SHARED")
    nanobind_link_options(${TARGET_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE -DNB_BUILD)
    target_compile_definitions(${TARGET_NAME} PUBLIC -DNB_SHARED)
    nanobind_lto(${TARGET_NAME})

    nanobind_strip(${TARGET_NAME})
  elseif(NOT WIN32 AND NOT APPLE)
    target_compile_options(${TARGET_NAME} PUBLIC $<${NB_OPT_SIZE}:-ffunction-sections -fdata-sections>)
    target_link_options(${TARGET_NAME} PUBLIC $<${NB_OPT_SIZE}:-Wl,--gc-sections>)
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

  if (${ARG_AS_SYSINCLUDE})
    set_target_properties(${TARGET_NAME} PROPERTIES
      CXX_CLANG_TIDY "")
  endif()

  if (MSVC)
    # Do not complain about vsnprintf
    target_compile_definitions(${TARGET_NAME} PRIVATE -D_CRT_SECURE_NO_WARNINGS)
  else()
    # Generally needed to handle type punning in Python code
    target_compile_options(${TARGET_NAME} PRIVATE -fno-strict-aliasing)
  endif()

  if (WIN32)
    if (${TARGET_NAME} MATCHES "-abi3")
      target_link_libraries(${TARGET_NAME} PUBLIC Python::SABIModule)
    else()
      target_link_libraries(${TARGET_NAME} PUBLIC Python::Module)
    endif()
  endif()

  if (TARGET_NAME MATCHES "-ft")
    target_compile_definitions(${TARGET_NAME} PUBLIC NB_FREE_THREADED)
  endif()

  # Nanobind performs many assertion checks -- detailed error messages aren't
  # included in Release/MinSizeRel/RelWithDebInfo modes
  target_compile_definitions(${TARGET_NAME} PRIVATE
    $<${NB_OPT_SIZE}:NB_COMPACT_ASSERTIONS>)

  # If nanobind was installed without submodule dependencies, then the
  # dependencies directory won't exist and we need to find them.
  # However, if the directory _does_ exist, then the user is free to choose
  # whether nanobind uses them (based on `NB_USE_SUBMODULE_DEPS`), with a
  # preference to choose them if `NB_USE_SUBMODULE_DEPS` is not defined
  if (NOT IS_DIRECTORY ${NB_DIR}/ext/robin_map/include OR
      (DEFINED NB_USE_SUBMODULE_DEPS AND NOT NB_USE_SUBMODULE_DEPS))
    include(CMakeFindDependencyMacro)
    find_dependency(tsl-robin-map)
    target_link_libraries(${TARGET_NAME} PRIVATE tsl::robin_map)
  else()
    target_include_directories(${TARGET_NAME} PRIVATE
      ${NB_DIR}/ext/robin_map/include)
  endif()

  target_include_directories(${TARGET_NAME} ${AS_SYSINCLUDE} PUBLIC
    ${Python_INCLUDE_DIRS}
    ${NB_DIR}/include)

  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
  nanobind_set_visibility(${TARGET_NAME})

  if (MSVC)
    # warning #1388-D: base class dllexport/dllimport specification differs from that of the derived class
    target_compile_options(${TARGET_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CUDA>:-Xcudafe --diag_suppress=1388>)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Define a convenience function for creating nanobind targets
# ---------------------------------------------------------------------------

function(nanobind_opt_size name)
  if (MSVC)
    target_compile_options(${name} PRIVATE $<${NB_OPT_SIZE}:$<$<COMPILE_LANGUAGE:CXX>:/Os>>)
  else()
    target_compile_options(${name} PRIVATE $<${NB_OPT_SIZE}:$<$<COMPILE_LANGUAGE:CXX>:-Os>>)
  endif()
endfunction()

function(nanobind_disable_stack_protector name)
  if (NOT MSVC)
    # The stack protector affects binding size negatively (+8% on Linux in my
    # benchmarks). Protecting from stack smashing in a Python VM seems in any
    # case futile, so let's get rid of it by default in optimized modes.
    target_compile_options(${name} PRIVATE $<${NB_OPT}:-fno-stack-protector>)
  endif()
endfunction()

function(nanobind_extension name)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX "${NB_SUFFIX}")
endfunction()

function(nanobind_extension_abi3 name)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX "${NB_SUFFIX_S}")
endfunction()

function (nanobind_lto name)
  set_target_properties(${name} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
endfunction()

function (nanobind_compile_options name)
  if (MSVC)
    target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/bigobj /MP>)
  endif()
endfunction()

function (nanobind_strip name)
  if (APPLE)
    target_link_options(${name} PRIVATE $<${NB_OPT}:-Wl,-dead_strip -Wl,-x -Wl,-S>)
  elseif (NOT WIN32)
    target_link_options(${name} PRIVATE $<${NB_OPT}:-Wl,-s>)
  endif()
endfunction()

function (nanobind_set_visibility name)
  set_target_properties(${name} PROPERTIES CXX_VISIBILITY_PRESET hidden)
endfunction()

function (nanobind_musl_static_libcpp name)
  if ("$ENV{AUDITWHEEL_PLAT}" MATCHES "musllinux")
    target_link_options(${name} PRIVATE -static-libstdc++ -static-libgcc)
  endif()
endfunction()

function(nanobind_add_module name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG
    "STABLE_ABI;FREE_THREADED;NB_STATIC;NB_SHARED;PROTECT_STACK;LTO;NOMINSIZE;NOSTRIP;MUSL_DYNAMIC_LIBCPP;NB_SUPPRESS_WARNINGS"
    "NB_DOMAIN" "")

  add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})

  nanobind_compile_options(${name})
  nanobind_link_options(${name})
  set_target_properties(${name} PROPERTIES LINKER_LANGUAGE CXX)

  if (ARG_NB_SHARED AND ARG_NB_STATIC)
    message(FATAL_ERROR "NB_SHARED and NB_STATIC cannot be specified at the same time!")
  elseif (NOT ARG_NB_SHARED)
    set(ARG_NB_STATIC TRUE)
  endif()

  # Stable ABI builds require CPython >= 3.12 and Python::SABIModule
  if ((Python_VERSION VERSION_LESS 3.12) OR
      (NOT Python_INTERPRETER_ID STREQUAL "Python") OR
      (NOT TARGET Python::SABIModule))
    set(ARG_STABLE_ABI FALSE)
  endif()

  if (NB_ABI MATCHES "t")
    # Free-threaded Python interpreters don't support building a nanobind
    # module that uses the stable ABI.
    set(ARG_STABLE_ABI FALSE)
  else()
    # A free-threaded Python interpreter is required to build a free-threaded
    # nanobind module.
    set(ARG_FREE_THREADED FALSE)
  endif()

  set(libname "nanobind")
  if (ARG_NB_STATIC)
    set(libname "${libname}-static")
  endif()

  if (ARG_STABLE_ABI)
    set(libname "${libname}-abi3")
  endif()

  if (ARG_FREE_THREADED)
    set(libname "${libname}-ft")
  endif()

  if (ARG_NB_DOMAIN AND ARG_NB_SHARED)
    set(libname ${libname}-${ARG_NB_DOMAIN})
  endif()

  if (ARG_NB_SUPPRESS_WARNINGS)
    set(EXTRA_LIBRARY_PARAMS AS_SYSINCLUDE)
  endif()

  nanobind_build_library(${libname} ${EXTRA_LIBRARY_PARAMS})

  if (ARG_NB_DOMAIN)
    target_compile_definitions(${name} PRIVATE NB_DOMAIN=${ARG_NB_DOMAIN})
  endif()

  if (ARG_STABLE_ABI)
    target_compile_definitions(${libname} PUBLIC -DPy_LIMITED_API=0x030C0000)
    nanobind_extension_abi3(${name})
  else()
    nanobind_extension(${name})
  endif()

  if (ARG_FREE_THREADED)
    target_compile_definitions(${name} PRIVATE NB_FREE_THREADED)
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

  if (ARG_NB_STATIC AND NOT ARG_MUSL_DYNAMIC_LIBCPP)
    nanobind_musl_static_libcpp(${name})
  endif()

  nanobind_set_visibility(${name})
endfunction()

# ---------------------------------------------------------------------------
# Detect if a list of targets uses sanitizers (ASAN/UBSAN/TSAN). If so, compute
# a shared library preload directive so that these sanitizers can be safely
# together with a Python binary that will in general not import them.
# ---------------------------------------------------------------------------

function(nanobind_sanitizer_preload_env env_var)
  set(detected_san "")

  # Process each target
  foreach(target ${ARGN})
    if (NOT TARGET ${target})
      continue()
    endif()

    # Check for sanitizer flags in various compile and link options
    set(san_flags "")
    set(san_options_to_search
      COMPILE_OPTIONS LINK_OPTIONS
      INTERFACE_LINK_OPTIONS INTERFACE_COMPILE_OPTIONS
    )
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
      set(san_options_to_search
        ${san_options_to_search}
        TRANSITIVE_LINK_PROPERTIES
        TRANSITIVE_COMPILE_PROPERTIES
      )
    endif()

    # create a list of all dependent targets and scan those for dependencies on sanitizers
    set(all_deps "${target}")
    get_target_property(deps ${target} LINK_LIBRARIES)
    if(deps AND NOT deps STREQUAL "deps-NOTFOUND")
        foreach(dep ${deps})
            if(NOT "${dep}" IN_LIST all_deps AND TARGET "${dep}")
                list(APPEND all_deps "${dep}")
            endif()
        endforeach()
    endif()

    foreach(tgt ${all_deps})
      # Check target type
      get_target_property(target_type ${tgt} TYPE)

      foreach(prop ${san_options_to_search})
        # Skip non-interface properties for INTERFACE_LIBRARY targets
        if(target_type STREQUAL "INTERFACE_LIBRARY")
          if(NOT prop MATCHES "^INTERFACE_")
            continue()
          endif()
        endif()

        get_target_property(options ${tgt} ${prop})
        if(options)
          foreach(opt ${options})
            if(opt MATCHES "-fsanitize=([^ ]+)")
              list(APPEND san_flags "${CMAKE_MATCH_1}")
            endif()
          endforeach()
        endif()
      endforeach()
    endforeach()

    # Parse sanitizer flags
    foreach(flag ${san_flags})
      string(REPLACE "\"" "" flag "${flag}")
      string(REPLACE "," ";" san_list "${flag}")
      foreach(san ${san_list})
        if(san MATCHES "^(address|asan)$")
          list(APPEND detected_san "asan")
        elseif(san MATCHES "^(thread|tsan)$")
          list(APPEND detected_san "tsan")
        elseif(san MATCHES "^(undefined|ubsan)$")
          list(APPEND detected_san "ubsan")
        endif()
      endforeach()
    endforeach()
  endforeach()

  if (detected_san)
    list(REMOVE_DUPLICATES detected_san)
    set(libs "")

    foreach(san ${detected_san})
      set(san_libname "")

      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(APPLE)
          set(san_libname "libclang_rt.${san}_osx_dynamic.dylib")
        else()
          set(san_libname "libclang_rt.${san}.so")
        endif()
      else()
        set(san_libname "lib${san}.so")
      endif()

      # Get the full path using a file name query
      execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${san_libname}
        RESULT_VARIABLE san_success
        OUTPUT_VARIABLE san_libpath
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )

      if(NOT san_success EQUAL 0)
        message(FATAL_ERROR "Error querying ${san_libname}: ${san_success}")
      endif()

      # Check if a real path was returned (and not just echoing back the input)
      if(NOT san_libpath OR (san_libpath STREQUAL san_libname))
        continue()
      endif()

      # Read the file content and turn into a single-line string
      file(READ "${san_libpath}" san_libdata LIMIT 1024)
      string(REPLACE "\n" " " san_libdata "${san_libdata}")

      if(san_libdata MATCHES "INPUT[ \t]*\\([ \t]*([^ \t)]+)")
        # If this is a linker script with INPUT directive, extract the path
        list(APPEND libs "${CMAKE_MATCH_1}")
      else()
        # Use the original library path
        list(APPEND libs "${san_libpath}")
      endif()
    endforeach()

    # Set platform-specific environment variable
    string(REPLACE ";" ":" libs_str "${libs}")
    if(APPLE)
      set(${env_var} "DYLD_INSERT_LIBRARIES=${libs_str}" PARENT_SCOPE)
    else()
      set(${env_var} "LD_PRELOAD=${libs_str}" PARENT_SCOPE)
    endif()
  else()
    set(${env_var} "" PARENT_SCOPE)
  endif()
endfunction()

# On macOS, it's quite tricky to get the actual path of the Python executable
# which is often hidden behind several layers of shims. We need this path to
# inject sanitizers.
function(nanobind_resolve_python_path)
  if(NOT DEFINED NB_PY_PATH)
    if (APPLE)
      execute_process(
        COMMAND ${Python_EXECUTABLE} "${NB_DIR}/cmake/darwin-python-path.py"
        RESULT_VARIABLE rv
        OUTPUT_VARIABLE NB_PY_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(NOT rv EQUAL 0)
        message(FATAL_ERROR "Could not query Python binary path")
      endif()
    else()
      set(NB_PY_PATH "${Python_EXECUTABLE}")
    endif()
    set(NB_PY_PATH ${NB_PY_PATH} CACHE STRING "" FORCE)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Convenient Cmake frontent for nanobind's stub generator
# ---------------------------------------------------------------------------

function (nanobind_add_stub name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "VERBOSE;INCLUDE_PRIVATE;EXCLUDE_DOCSTRINGS;INSTALL_TIME;RECURSIVE;EXCLUDE_FROM_ALL" "MODULE;COMPONENT;PATTERN_FILE;OUTPUT_PATH" "PYTHON_PATH;LIB_PATH;DEPENDS;MARKER_FILE;OUTPUT")

  if (EXISTS ${NB_DIR}/src/stubgen.py)
    set(NB_STUBGEN "${NB_DIR}/src/stubgen.py")
  elseif (EXISTS ${NB_DIR}/stubgen.py)
    set(NB_STUBGEN "${NB_DIR}/stubgen.py")
  else()
    message(FATAL_ERROR "nanobind_add_stub(): could not locate 'stubgen.py'!")
  endif()

  if (NOT ARG_VERBOSE)
    list(APPEND NB_STUBGEN_ARGS -q)
  else()
    set(NB_STUBGEN_EXTRA USES_TERMINAL)
  endif()

  if (ARG_INCLUDE_PRIVATE)
    list(APPEND NB_STUBGEN_ARGS -P)
  endif()

  if (ARG_EXCLUDE_DOCSTRINGS)
    list(APPEND NB_STUBGEN_ARGS -D)
  endif()

  if (ARG_RECURSIVE)
    list(APPEND NB_STUBGEN_ARGS -r)
  endif()

  foreach (PYTHON_PATH IN LISTS ARG_PYTHON_PATH)
    list(APPEND NB_STUBGEN_ARGS -i "${PYTHON_PATH}")
  endforeach()

  foreach (LIB_PATH IN LISTS ARG_LIB_PATH)
    list(APPEND NB_STUBGEN_ARGS -L "${LIB_PATH}")
  endforeach()

  if (ARG_PATTERN_FILE)
    list(APPEND NB_STUBGEN_ARGS -p "${ARG_PATTERN_FILE}")
  endif()

  if (ARG_MARKER_FILE)
    foreach (MARKER_FILE IN LISTS ARG_MARKER_FILE)
      list(APPEND NB_STUBGEN_ARGS -M "${MARKER_FILE}")
      list(APPEND NB_STUBGEN_OUTPUTS "${MARKER_FILE}")
    endforeach()
  endif()

  if (NOT ARG_MODULE)
    message(FATAL_ERROR "nanobind_add_stub(): a 'MODULE' argument must be specified!")
  else()
    list(APPEND NB_STUBGEN_ARGS -m "${ARG_MODULE}")
  endif()

  list(LENGTH ARG_OUTPUT OUTPUT_LEN)

  # Some sanity hecks
  if (ARG_RECURSIVE)
    if (NOT ARG_INSTALL_TIME)
      if ((OUTPUT_LEN EQUAL 0) AND NOT ARG_OUTPUT_PATH)
        message(FATAL_ERROR "nanobind_add_stub(): either 'OUTPUT' or 'OUTPUT_PATH' must be specified when 'RECURSIVE' is set!")
      endif()
    endif()
  else()
    if ((OUTPUT_LEN EQUAL 0) AND NOT ARG_INSTALL_TIME)
      message(FATAL_ERROR "nanobind_add_stub(): an 'OUTPUT' argument must be specified.")
    endif()
    if ((OUTPUT_LEN GREATER 0) AND ARG_OUTPUT_PATH)
      message(FATAL_ERROR "nanobind_add_stub(): 'OUTPUT' and 'OUTPUT_PATH' can only be specified together when 'RECURSIVE' is set!")
    endif()
    if (OUTPUT_LEN GREATER 1)
      message(FATAL_ERROR "nanobind_add_stub(): specifying more than one 'OUTPUT' requires that 'RECURSIVE' is set!")
    endif()
  endif()

  if (ARG_OUTPUT_PATH)
    list(APPEND NB_STUBGEN_ARGS -O "${ARG_OUTPUT_PATH}")
  endif()

  foreach (OUTPUT IN LISTS ARG_OUTPUT)
    if (NOT ARG_RECURSIVE)
      list(APPEND NB_STUBGEN_ARGS -o "${OUTPUT}")
    endif()
    list(APPEND NB_STUBGEN_OUTPUTS "${OUTPUT}")
  endforeach()

  file(TO_CMAKE_PATH ${Python_EXECUTABLE} NB_Python_EXECUTABLE)

  set(NB_STUBGEN_CMD "${NB_Python_EXECUTABLE}" "${NB_STUBGEN}" ${NB_STUBGEN_ARGS})

  if (NOT WIN32)
    # Pass sanitizer flags to nanobind if needed
    nanobind_sanitizer_preload_env(NB_STUBGEN_ENV ${ARG_DEPENDS})
    if (NB_STUBGEN_ENV)
      nanobind_resolve_python_path()
      if (NB_STUBGEN_ENV MATCHES asan)
        list(APPEND NB_STUBGEN_ENV "ASAN_OPTIONS=detect_leaks=0")
      endif()
      set(NB_STUBGEN_CMD ${CMAKE_COMMAND} -E env "${NB_STUBGEN_ENV}" "${NB_PY_PATH}" "${NB_STUBGEN}" ${NB_STUBGEN_ARGS})
    endif()
  endif()

  if (NOT ARG_INSTALL_TIME)
    add_custom_command(
      OUTPUT ${NB_STUBGEN_OUTPUTS}
      COMMAND ${NB_STUBGEN_CMD}
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS ${ARG_DEPENDS} "${NB_STUBGEN}" "${ARG_PATTERN_FILE}"
      ${NB_STUBGEN_EXTRA}
    )
    add_custom_target(${name} ALL DEPENDS ${NB_STUBGEN_OUTPUTS})
  else()
    set(NB_STUBGEN_EXTRA "")
    if (ARG_COMPONENT)
      list(APPEND NB_STUBGEN_EXTRA COMPONENT ${ARG_COMPONENT})
    endif()
    if (ARG_EXCLUDE_FROM_ALL)
      list(APPEND NB_STUBGEN_EXTRA EXCLUDE_FROM_ALL)
    endif()
    # \${CMAKE_INSTALL_PREFIX} has same effect as $<INSTALL_PREFIX>
    # This is for compatibility with CMake < 3.27.
    # For more info: https://github.com/wjakob/nanobind/issues/420#issuecomment-1971353531
    install(CODE "set(CMD \"${NB_STUBGEN_CMD}\")\nexecute_process(\n COMMAND \$\{CMD\}\n WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\"\n)" ${NB_STUBGEN_EXTRA})
  endif()
endfunction()
