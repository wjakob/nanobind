#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct Other {
    int data;
};

NB_MODULE(test_dep_other_ext, m) {
    nb::class_<Other>(m, "Other")
        .def(nb::init<>())
        .def_rw("data", &Other::data);
}
