# If leaks are found, abort() during interpreter shutdown to catch this in the CI
add_definitions(-DNB_ABORT_ON_LEAK)

if (NB_TEST_STABLE_ABI)
  set(NB_EXTRA_ARGS ${NB_EXTRA_ARGS} STABLE_ABI)
endif()

if (NB_TEST_FREE_THREADED)
  set(NB_EXTRA_ARGS ${NB_EXTRA_ARGS} FREE_THREADED)
endif()

if (NB_TEST_SHARED_BUILD)
  set(NB_EXTRA_ARGS ${NB_EXTRA_ARGS} NB_SHARED)
endif()

# ---------------------------------------------------------------------------
# Compile with a few more compiler warnings turned on
# ---------------------------------------------------------------------------

if (MSVC)
  if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  elseif (NOT NB_TEST_CUDA)
    add_compile_options(/W4)
  endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wno-unused-local-typedefs)
endif()

if (UNIX AND (CMAKE_SIZEOF_VOID_P EQUAL 4) AND (CMAKE_SYSTEM_PROCESSOR STREQUAL i686))
  # Don't use the legacy 387 math coprocessor in 32 bit builds, this causes tests to fail
  add_compile_options(-mfpmath=sse -msse2)
endif()

# Enforce the use of the CUDA NVCC compiler if requested
if (NB_TEST_CUDA)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

set(NB_TEST_SANITIZER "")

if (NB_TEST_SANITIZERS_ASAN)
  list(APPEND NB_TEST_SANITIZERS address)
endif()

if (NB_TEST_SANITIZERS_UBSAN)
  list(APPEND NB_TEST_SANITIZERS undefined)
endif()

if (NB_TEST_SANITIZERS_TSAN)
  list(APPEND NB_TEST_SANITIZERS thread)
endif()


if (NB_TEST_SANITIZERS)
  string(REPLACE ";" "," NB_TEST_SANITIZERS "${NB_TEST_SANITIZERS}")
  add_compile_options(-fsanitize=${NB_TEST_SANITIZERS})
  add_link_options(-fsanitize=${NB_TEST_SANITIZERS})
endif()

set(TEST_NAMES
  functions
  classes
  holders
  stl
  stl_bind_map
  stl_bind_vector
  chrono
  enum
  eval
  ndarray
  exception
  make_iterator
  typing
  issue
  intrusive
  thread
)

foreach (NAME ${TEST_NAMES})
  nanobind_add_module(test_${NAME}_ext test_${NAME}.cpp ${NB_EXTRA_ARGS})

  if (NB_TEST_CUDA)
    set_property(SOURCE test_${NAME}.cpp PROPERTY LANGUAGE CUDA)
  endif()
endforeach()

target_sources(test_intrusive_ext PRIVATE test_intrusive_impl.cpp)

foreach (NAME functions classes ndarray stl enum typing make_iterator)
  if (NAME STREQUAL typing)
    set(EXTRA
      MARKER_FILE py.typed
      PATTERN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/pattern_file.nb"
    )
  else()
    set(EXTRA "")
  endif()

  if (CMAKE_CONFIGURATION_TYPES)
    # On multi-config generators like Visual Studio, put stubs in the 'Debug' / 'Release' /.. folders
    set(PYI_PREFIX $<CONFIG>/)
  endif()

  nanobind_add_stub(
    ${NAME}_ext_stub
    MODULE test_${NAME}_ext
    OUTPUT ${PYI_PREFIX}test_${NAME}_ext.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:test_${NAME}_ext>
    DEPENDS test_${NAME}_ext
    ${EXTRA})
endforeach()

find_package (Eigen3 3.3.1 NO_MODULE)
if (TARGET Eigen3::Eigen)
  nanobind_add_module(test_eigen_ext test_eigen.cpp ${NB_EXTRA_ARGS})
  target_link_libraries(test_eigen_ext PRIVATE Eigen3::Eigen)
endif()

add_library(
  inter_module
  SHARED
  inter_module.h
  inter_module.cpp
)

target_compile_definitions(inter_module PRIVATE -DSHARED_BUILD)
target_compile_features(inter_module PRIVATE cxx_std_17)
target_include_directories(inter_module PRIVATE ${NB_DIR}/include)

nanobind_add_module(test_inter_module_1_ext NB_DOMAIN mydomain test_inter_module_1.cpp ${NB_EXTRA_ARGS})
nanobind_add_module(test_inter_module_2_ext NB_DOMAIN mydomain test_inter_module_2.cpp ${NB_EXTRA_ARGS})
target_link_libraries(test_inter_module_1_ext PRIVATE inter_module)
target_link_libraries(test_inter_module_2_ext PRIVATE inter_module)

set(TEST_FILES
  common.py
  test_classes.py
  test_eigen.py
  test_enum.py
  test_eval.py
  test_exception.py
  test_functions.py
  test_holders.py
  test_inter_module.py
  test_intrusive.py
  test_make_iterator.py
  test_stl.py
  test_stl_bind_map.py
  test_stl_bind_vector.py
  test_chrono.py
  test_ndarray.py
  test_stubs.py
  test_typing.py
  test_thread.py

  # Stub reference files
  test_classes_ext.pyi.ref
  test_functions_ext.pyi.ref
  test_make_iterator_ext.pyi.ref
  test_ndarray_ext.pyi.ref
  test_stl_ext.pyi.ref
  test_enum_ext.pyi.ref
  test_typing_ext.pyi.ref
  py_stub_test.py
  py_stub_test.pyi.ref
)

set (PY_STUB_TEST py_stub_test.py)
if (NOT (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR) OR MSVC)
  if (CMAKE_CONFIGURATION_TYPES)
    set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
  else()
    set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  foreach(TEST_FILE IN LISTS TEST_FILES)
    set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE})
    set(OUT_FILE ${OUT_DIR}/${TEST_FILE})
    set(TEST_FILES_OUT ${TEST_FILES_OUT} ${OUT_FILE})
    add_custom_command(
      DEPENDS ${IN_FILE} OUTPUT ${OUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IN_FILE} ${OUT_DIR})
  endforeach()

  add_custom_target(copy-tests ALL DEPENDS ${TEST_FILES_OUT})
  set(PY_STUB_TEST ${OUT_DIR}/py_stub_test.py)
endif()

nanobind_add_stub(
  py_stub
  MODULE py_stub_test
  OUTPUT ${PYI_PREFIX}py_stub_test.pyi
  PYTHON_PATH $<TARGET_FILE_DIR:test_stl_ext>
  DEPENDS ${PY_STUB_TEST}
)
