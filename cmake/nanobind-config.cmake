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
# Create shared/static library targets for nanobind's non-templated core
# ---------------------------------------------------------------------------

function (nanobind_build_library TARGET_NAME TARGET_TYPE)
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
    ${NB_DIR}/include/nanobind/tensor.h
    ${NB_DIR}/include/nanobind/operators.h
    ${NB_DIR}/include/nanobind/stl/shared_ptr.h
    ${NB_DIR}/include/nanobind/stl/unique_ptr.h
    ${NB_DIR}/include/nanobind/stl/string.h
    ${NB_DIR}/include/nanobind/stl/string_view.h
    ${NB_DIR}/include/nanobind/stl/tuple.h
    ${NB_DIR}/include/nanobind/stl/pair.h
    ${NB_DIR}/include/nanobind/stl/function.h
    ${NB_DIR}/include/nanobind/stl/vector.h
    ${NB_DIR}/include/nanobind/stl/list.h
    ${NB_DIR}/include/nanobind/stl/optional.h
    ${NB_DIR}/include/nanobind/stl/variant.h

    ${NB_DIR}/src/buffer.h
    ${NB_DIR}/src/nb_internals.h
    ${NB_DIR}/src/nb_internals.cpp
    ${NB_DIR}/src/nb_func.cpp
    ${NB_DIR}/src/nb_type.cpp
    ${NB_DIR}/src/nb_enum.cpp
    ${NB_DIR}/src/common.cpp
    ${NB_DIR}/src/error.cpp
    ${NB_DIR}/src/tensor.cpp
    ${NB_DIR}/src/trampoline.cpp
    ${NB_DIR}/src/implicit.cpp
  )

  if (TARGET_TYPE STREQUAL "SHARED")
    if (APPLE)
      # target_link_options(${TARGET_NAME} PRIVATE -undefined suppress -flat_namespace)
    endif()

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
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX "${NB_SUFFIX}")
endfunction()

function(nanobind_extension_abi3 name)
  get_filename_component(ext "${NB_SUFFIX}" LAST_EXT)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX ".abi3${ext}")
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
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NOMINSIZE;STABLE_ABI;NOSTRIP;NB_STATIC;NB_SHARED;PROTECT_STACK;LTO" "" "")

  if (APPLE)
    add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})
    target_include_directories(${name} PUBLIC ${Python_INCLUDE_DIRS})
    # target_link_options(${name} PRIVATE -undefined suppress -flat_namespace)
    set(SYMBOLS
        PyBool_Type
        PyBuffer_Release
        PyBytes_AsString
        PyBytes_FromObject
        PyBytes_FromString
        PyBytes_FromStringAndSize
        PyBytes_Size
        PyCMethod_New
        PyCallable_Check
        PyCapsule_GetContext
        PyCapsule_GetName
        PyCapsule_GetPointer
        PyCapsule_New
        PyCapsule_SetContext
        PyCapsule_SetDestructor
        PyCapsule_SetName
        PyCapsule_Type
        PyDict_GetItem
        PyDict_GetItemString
        PyDict_Items
        PyDict_New
        PyDict_Next
        PyDict_SetItem
        PyDict_SetItemString
        PyErr_Clear
        PyErr_Fetch
        PyErr_Format
        PyErr_NewException
        PyErr_NoMemory
        PyErr_NormalizeException
        PyErr_Occurred
        PyErr_Restore
        PyErr_SetString
        PyErr_WarnFormat
        PyEval_EvalCode
        PyEval_GetGlobals
        PyEval_RestoreThread
        PyEval_SaveThread
        PyExc_AttributeError
        PyExc_BufferError
        PyExc_Exception
        PyExc_ImportError
        PyExc_IndexError
        PyExc_KeyError
        PyExc_MemoryError
        PyExc_OverflowError
        PyExc_RuntimeError
        PyExc_RuntimeWarning
        PyExc_StopIteration
        PyExc_SystemError
        PyExc_TypeError
        PyExc_ValueError
        PyException_SetTraceback
        PyFile_WriteObject
        PyFile_WriteString
        PyFloat_AsDouble
        PyFloat_FromDouble
        PyFloat_Type
        PyFrame_GetBack
        PyFrame_GetCode
        PyFrame_GetLineNumber
        PyGILState_Check
        PyGILState_Ensure
        PyGILState_Release
        PyImport_AddModuleObject
        PyImport_ImportModule
        PyInterpreterState_Get
        PyInterpreterState_GetDict
        PyIter_Check
        PyIter_Next
        PyList_Append
        PyList_New
        PyList_Type
        PyLong_AsLong
        PyLong_AsUnsignedLong
        PyLong_FromLong
        PyLong_FromLongLong
        PyLong_FromUnsignedLong
        PyLong_FromUnsignedLongLong
        PyMem_Free
        PyMem_Malloc
        PyModule_AddObject
        PyModule_Create2
        PyModule_GetNameObject
        PyModule_NewObject
        PyModule_Type
        PyNumber_Absolute
        PyNumber_Add
        PyNumber_And
        PyNumber_FloorDivide
        PyNumber_Invert
        PyNumber_Lshift
        PyNumber_Long
        PyNumber_Multiply
        PyNumber_Negative
        PyNumber_Or
        PyNumber_Rshift
        PyNumber_Subtract
        PyNumber_Xor
        PyObject_CallMethod
        PyObject_Free
        PyObject_GC_Del
        PyObject_GC_UnTrack
        PyObject_GetAttr
        PyObject_GetAttrString
        PyObject_GetBuffer
        PyObject_GetItem
        PyObject_GetIter
        PyObject_HasAttrString
        PyObject_Init
        PyObject_Malloc
        PyObject_Realloc
        PyObject_Repr
        PyObject_RichCompare
        PyObject_RichCompareBool
        PyObject_SetAttr
        PyObject_SetAttrString
        PyObject_SetItem
        PyObject_Size
        PyObject_Str
        PyObject_Vectorcall
        PyObject_VectorcallMethod
        PyProperty_Type
        PySequence_Check
        PySequence_Fast
        PySequence_GetItem
        PySequence_SetItem
        PySet_Add
        PySet_New
        PySlice_New
        PySlice_Type
        PySys_GetObject
        PyTuple_New
        PyTuple_Type
        PyType_FromMetaclass
        PyType_FromSpec
        PyType_GenericAlloc
        PyType_GenericNew
        PyType_GetSlot
        PyType_IsSubtype
        PyType_Type
        PyUnicode_AsUTF8AndSize
        PyUnicode_FromFormat
        PyUnicode_FromString
        PyUnicode_FromStringAndSize
        PyUnicode_InternFromString
        PyVectorcall_Call
        PyWeakref_NewRef
        Py_AtExit
        Py_CompileStringExFlags
        _PyObject_GC_New
        _PyWeakref_RefType
        _Py_Dealloc
        _Py_EllipsisObject
        _Py_FalseStruct
        _Py_NegativeRefcount
        _Py_NotImplementedStruct
        _Py_NoneStruct
        _Py_RefTotal
        _Py_TrueStruct)

    set(SYMBOL_STR "-Wl")
    foreach (SYMBOL ${SYMBOLS}) 
      set(SYMBOL_STR "${SYMBOL_STR},-U,_${SYMBOL}")
    endforeach()
    message(STATUS ${SYMBOL_STR})

    target_link_options(${name} PRIVATE ${SYMBOL_STR})

  else()
    Python_add_library(${name} MODULE ${ARG_UNPARSED_ARGUMENTS})
  endif()

  nanobind_cpp17(${name})
  nanobind_msvc(${name})
  nanobind_headers(${name})

  # Limited API interface only supported in Python >= 3.12
  if ((Python_VERSION_MAJOR EQUAL 3) AND (Python_VERSION_MINOR LESS 12))
    set(ARG_STABLE_ABI OFF)
  endif()

  if (ARG_STABLE_ABI)
    if (ARG_NB_STATIC)
      nanobind_build_library(nanobind-static-abi3 STATIC)
      set(libname nanobind-static-abi3)
    else()
      nanobind_build_library(nanobind-abi3 SHARED)
      set(libname nanobind-abi3)
    endif()

    target_compile_definitions(${libname} PUBLIC -DPy_LIMITED_API=0x030C0000)
    nanobind_extension_abi3(${name})
  else()
    if (ARG_NB_STATIC)
      if (ARG_LTO)
        nanobind_build_library(nanobind-static-lto STATIC)
        set(libname nanobind-static-lto)
      else()
        nanobind_build_library(nanobind-static STATIC)
        set(libname nanobind-static)
      endif()
    else()
      nanobind_build_library(nanobind SHARED)
      set(libname nanobind)
    endif()

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
