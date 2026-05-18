#include <nanobind/nanobind.h>
#include <nanobind/nb_named_tuple.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

struct Point { int x; float y; };
struct Color { int r; int g; };

// Config exercises the per-field defaults overload of ``def_rw``.
struct Config { std::string name; int width; int height; };

// OptItem exercises optional<T> fields.
struct OptItem { int id; std::optional<std::string> label; };

// Outer holds a nested NamedTuple (Point) as a field.
struct Outer { Point origin; int weight; };

// Tree is a self-referential NamedTuple: a node value plus a list of children
// of the same type. ``std::vector<Tree>`` is allowed at struct-definition
// time in C++17 because ``std::vector`` only requires its element type to be
// complete at instantiation of certain member templates, not at declaration.
struct Tree { int value; std::vector<Tree> children; };

NB_NAMED_TUPLE_CASTER(Point)
NB_NAMED_TUPLE_CASTER(Color)
NB_NAMED_TUPLE_CASTER(Config)
NB_NAMED_TUPLE_CASTER(OptItem)
NB_NAMED_TUPLE_CASTER(Outer)
NB_NAMED_TUPLE_CASTER(Tree)

NB_MODULE(test_named_tuple_ext, m) {
    // Helper API: explicit field declarations.
    nb::named_tuple<Point>(m, "Point")
        .def_rw("x", &Point::x)
        .def_rw("y", &Point::y);

    // Macro API: same effect with the field list given once.
    NB_NAMED_TUPLE(m, Color, r, g);

    // Per-field defaults via the helper API. ``collections.namedtuple``
    // only supports trailing defaults: ``name`` is required, ``width``
    // defaults to 80, ``height`` defaults to 24.
    nb::named_tuple<Config>(m, "Config")
        .def_rw("name", &Config::name)
        .def_rw("width", &Config::width, 80)
        .def_rw("height", &Config::height, 24);

    // Optional fields: ``label`` may be ``None``.
    nb::named_tuple<OptItem>(m, "OptItem")
        .def_rw("id", &OptItem::id)
        .def_rw("label", &OptItem::label);

    // Nested NamedTuple: an ``Outer`` has a ``Point`` field.
    nb::named_tuple<Outer>(m, "Outer")
        .def_rw("origin", &Outer::origin)
        .def_rw("weight", &Outer::weight);

    // Self-referential NamedTuple: a ``Tree`` has a list of ``Tree``s.
    nb::named_tuple<Tree>(m, "Tree")
        .def_rw("value", &Tree::value)
        .def_rw("children", &Tree::children);

    m.def("make_point", [](int x, float y) { return Point{x, y}; });
    m.def("point_x", [](Point p) { return p.x; });
    m.def("point_y", [](Point p) { return p.y; });
    m.def("roundtrip_point", [](Point p) { return p; });

    m.def("make_color", [](int r, int g) { return Color{r, g}; });
    m.def("color_sum", [](Color c) { return c.r + c.g; });
    m.def("roundtrip_color", [](Color c) { return c; });

    m.def("default_config", []() { return Config{"untitled", 80, 24}; });
    m.def("roundtrip_config", [](Config c) { return c; });

    m.def("make_optitem",
          [](int id, std::optional<std::string> label) {
              return OptItem{id, label};
          });
    m.def("roundtrip_optitem", [](OptItem o) { return o; });

    m.def("make_outer", [](int x, float y, int w) {
        return Outer{Point{x, y}, w};
    });
    m.def("roundtrip_outer", [](Outer o) { return o; });

    m.def("tree_leaf", [](int v) { return Tree{v, {}}; });
    m.def("tree_branch", [](int v, std::vector<Tree> children) {
        return Tree{v, std::move(children)};
    });
    m.def("tree_sum", [](Tree t) {
        int total = 0;
        std::vector<Tree> stack;
        stack.push_back(std::move(t));
        while (!stack.empty()) {
            Tree cur = std::move(stack.back());
            stack.pop_back();
            total += cur.value;
            for (auto &child : cur.children)
                stack.push_back(std::move(child));
        }
        return total;
    });
}
