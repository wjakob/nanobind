#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct Base {
    int value;
};

struct Other {
    int data;
};

struct SubModType {
    int info;
};

struct SubSubModType {
    int deep;
};

struct Child : Base {
    int get_doubled() const { return value * 2; }
};

NB_MODULE(test_dep_child_ext, m) {
    nb::class_<Child, Base>(m, "Child")
        .def(nb::init<>())
        .def("get_doubled", &Child::get_doubled);
    m.def("make_other", []() { return Other{7}; });
    m.def("read_other", [](const Other &o) { return o.data; });
    m.def("make_subtype", []() { return SubModType{42}; });
    m.def("make_deep", []() { return SubSubModType{99}; });
}
