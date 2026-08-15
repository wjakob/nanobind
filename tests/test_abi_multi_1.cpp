#include "test_abi_multi.h"

namespace nb = nanobind;

NB_MODULE(NB_ABI_MULTI_NAME, m) {
    // Handshake parameters of this extension, used by the tests in test_abi.py
    m.attr("abi_major") = NB_BACKEND_ABI_MAJOR;
    m.attr("platform_abi_tag") = NB_PLATFORM_ABI_TAG;

    nb::class_<Point>(m, "Point")
        .def(nb::init<>())
        .def(nb::init<int, int>())
        .def_rw("x", &Point::x)
        .def_rw("y", &Point::y);

    m.def("make_point", [](int x, int y) { return Point(x, y); });

    m.attr("limited_api") = (unsigned int) Py_LIMITED_API;

    bind_part2(m);
}
