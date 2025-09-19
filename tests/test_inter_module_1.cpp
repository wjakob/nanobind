#include <nanobind/nanobind.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/shared_ptr.h>
#include "inter_module.h"

namespace nb = nanobind;

NB_MODULE(test_inter_module_1_ext, m) {
    m.def("create_shared", &create_shared);
    m.def("create_shared_sp", &create_shared_sp);
    m.def("create_shared_up", &create_shared_up);
    m.def("create_enum", &create_enum);
    m.def("throw_shared", &throw_shared);
    m.def("export_all", []() { nb::interoperate_by_default(true, false); });
    m.def("import_all", []() { nb::interoperate_by_default(false, true); });
}
