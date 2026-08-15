#include "test_abi_multi.h"

namespace nb = nanobind;

void bind_part2(nb::module_ &m) {
    nb::class_<Box>(m, "Box")
        .def(nb::init<>())
        .def_rw("min", &Box::min)
        .def_rw("max", &Box::max);

    m.def("box_from_points", [](const Point &a, const Point &b) {
        return Box{ a, b };
    });

    m.def("box_width", [](const Box &b) { return b.max.x - b.min.x; });
}
