#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct Base {
    int value;
};

struct SubModType {
    int info;
};

struct SubSubModType {
    int deep;
};

NB_MODULE(test_dep_base_ext, m) {
    nb::class_<Base>(m, "Base")
        .def(nb::init<>())
        .def_rw("value", &Base::value);

    auto sub = m.def_submodule("sub");
    nb::class_<SubModType>(sub, "SubModType")
        .def(nb::init<>())
        .def_rw("info", &SubModType::info);

    auto subsub = sub.def_submodule("inner");
    nb::class_<SubSubModType>(subsub, "SubSubModType")
        .def(nb::init<>())
        .def_rw("deep", &SubSubModType::deep);
}
