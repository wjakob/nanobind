#include <nanobind/nanobind.h>
#include <nanobind/nb_named_tuple.h>

namespace nb = nanobind;

struct Point { int x; float y; };
struct Color { int r; int g; };

NB_NAMED_TUPLE_CASTER(Point)
NB_NAMED_TUPLE_CASTER(Color)

NB_MODULE(test_named_tuple_ext, m) {
    // Helper API: explicit field declarations
    nb::named_tuple<Point>(m, "Point")
        .def_rw("x", &Point::x)
        .def_rw("y", &Point::y);

    // Macro API: same effect with the field list given once.
    NB_NAMED_TUPLE(m, Color, r, g);

    m.def("make_point", [](int x, float y) { return Point{x, y}; });
    m.def("point_x", [](Point p) { return p.x; });
    m.def("point_y", [](Point p) { return p.y; });
    m.def("roundtrip_point", [](Point p) { return p; });

    m.def("make_color", [](int r, int g) { return Color{r, g}; });
    m.def("color_sum", [](Color c) { return c.r + c.g; });
    m.def("roundtrip_color", [](Color c) { return c; });
}
