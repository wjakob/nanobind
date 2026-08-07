#include <nanobind/nanobind.h>

extern "C" int lib_path_value();

namespace nb = nanobind;

NB_MODULE(test_lib_path_ext, m) {
    m.def("value", &lib_path_value);
}
