nanobind_add_module(test_functions_ext test_functions.cpp)
nanobind_add_module(test_classes_ext test_classes.cpp)
nanobind_add_module(test_holders_ext test_holders.cpp)
nanobind_add_module(test_stl_ext test_stl.cpp)
nanobind_add_module(test_enum_ext test_enum.cpp)

set(TEST_FILES
  test_functions.py
  test_classes.py
  test_holders.py
  test_stl.py
  test_enum.py
)

if (NOT (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR))
  foreach(TEST_FILE IN LISTS TEST_FILES)
    set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE})
    set(OUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TEST_FILE})
    set(TEST_FILES_OUT ${TEST_FILES_OUT} ${OUT_FILE})
    add_custom_command(
      DEPENDS ${IN_FILE} OUTPUT ${OUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IN_FILE} ${CMAKE_CURRENT_BINARY_DIR})
  endforeach()
  add_custom_target(copy-tests ALL DEPENDS ${TEST_FILES_OUT})
endif()
