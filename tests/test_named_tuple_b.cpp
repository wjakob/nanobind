/*
    Companion extension for the cross-module reuse test. It defines a
    second ``Point`` registration (identical struct) and a function that
    accepts a ``Point``. The Python test passes a ``Point`` produced by
    ``test_named_tuple_ext`` and verifies that this module's caster accepts
    it structurally.
*/

#include <nanobind/nanobind.h>
#include <nanobind/nb_named_tuple.h>

namespace nb = nanobind;

struct Point { int x; float y; };

NB_NAMED_TUPLE_CASTER(Point)

NB_MODULE(test_named_tuple_b_ext, m) {
    NB_NAMED_TUPLE(m, Point, x, y);

    m.def("consume_point", [](Point p) { return p.x + (int) p.y; });
    m.def("roundtrip_point", [](Point p) { return p; });
}
