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

// DocPoint exercises the helper-API docstring overload (class doc +
// per-field docs through ``nb::doc("...")``).
struct DocPoint { int x; int y; };

namespace geom {
struct QualPoint { int x; int y; };
} // namespace geom

// Templated NamedTuple regression case: the macros cannot accept ``Foo<int,
// float>`` directly because the comma terminates the variadic preprocessor
// argument list. The supported workarounds are (a) a typedef + ``NB_NAMED_TUPLE``
// and (b) the helper API which does not go through the preprocessor.
template <typename A, typename B> struct Pair { A first; B second; };
using PairIF = Pair<int, float>;
using PairFI = Pair<float, int>;

// Two helper structs used only by the validation regression tests for the
// non-trailing-defaults check (fix #2) and the throwing-default-thunk
// recovery check (fix #7). They are exposed via test-only ``trigger_*``
// functions that invoke ``finalize()`` explicitly so the exception surfaces
// to Python.
struct BadDefaults { int a; int b; };

// Field type whose ``from_cpp`` always raises -- used to exercise the
// exception path in the default-value thunk.
struct ThrowOnFromCpp { int x; };

NB_NAMED_TUPLE_CASTER(Point)
NB_NAMED_TUPLE_CASTER(Color)
NB_NAMED_TUPLE_CASTER(Config)
NB_NAMED_TUPLE_CASTER(OptItem)
NB_NAMED_TUPLE_CASTER(Outer)
NB_NAMED_TUPLE_CASTER(Tree)
NB_NAMED_TUPLE_CASTER(DocPoint)
NB_NAMED_TUPLE_CASTER(geom::QualPoint)
NB_NAMED_TUPLE_CASTER(PairIF)
NB_NAMED_TUPLE_CASTER(PairFI)

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
template <> struct type_caster<ThrowOnFromCpp> {
    NB_TYPE_CASTER(ThrowOnFromCpp, const_name("ThrowOnFromCpp"))
    bool from_python(handle, uint8_t, cleanup_list *) noexcept { return false; }
    static handle from_cpp(const ThrowOnFromCpp &, rv_policy,
                           cleanup_list *) noexcept {
        PyErr_SetString(PyExc_ValueError,
                        "test_named_tuple: from_cpp deliberately failed");
        return handle();
    }
};
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

struct ThrowField { ThrowOnFromCpp a; int b; };

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

    // Class + per-field docstrings via the helper API.
    nb::named_tuple<DocPoint>(m, "DocPoint",
                              "A 2D point with documented fields.")
        .def_rw("x", &DocPoint::x, nb::doc("horizontal coordinate"))
        .def_rw("y", &DocPoint::y, 0, nb::doc("vertical coordinate (default 0)"));

    // Qualified C++ type bound via NB_NAMED_TUPLE_NAMED. The stringified
    // C++ name ("geom::QualPoint") is not a valid Python identifier, so the
    // macro accepts an explicit Python name as the third argument.
    NB_NAMED_TUPLE_NAMED(m, geom::QualPoint, "QualPoint", x, y);

    // Templated type bound two ways:
    //  - Via typedef + the macro (PairIF -> Pair<int,float>)
    //  - Via the helper API with an explicit Python name (PairFI ->
    //    Pair<float,int>); this path is preferred when the C++ type alias
    //    name and the desired Python identifier need to differ.
    NB_NAMED_TUPLE(m, PairIF, first, second);
    nb::named_tuple<PairFI>(m, "PairFI")
        .def_rw("first", &PairFI::first)
        .def_rw("second", &PairFI::second);

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

    m.def("roundtrip_docpoint", [](DocPoint p) { return p; });
    m.def("roundtrip_qualpoint", [](geom::QualPoint p) { return p; });
    m.def("roundtrip_pair_if", [](PairIF p) { return p; });
    m.def("roundtrip_pair_fi", [](PairFI p) { return p; });

    // Regression test for fix #2: a non-trailing default must be rejected
    // with a std::runtime_error (surfaced as RuntimeError on the Python
    // side) at finalize() time -- not as a confusing None-placeholder
    // failure from collections.namedtuple.
    m.def("trigger_non_trailing_defaults", [](nb::handle scope) {
        nb::named_tuple<BadDefaults>(scope, "BadDefaults_NoTrailing")
            .def_rw("a", &BadDefaults::a, 1) // has default
            .def_rw("b", &BadDefaults::b)    // missing default after default
            .finalize();
    });

    // Regression test for fix #7: a default-value thunk whose ``from_cpp``
    // raises must surface as a Python exception out of ``finalize()`` (now
    // safe to throw thanks to the public ``finalize()`` + ``noexcept(false)``
    // destructor pair) and must not bring down the process.
    m.def("trigger_throwing_default", [](nb::handle scope) {
        nb::named_tuple<ThrowField>(scope, "ThrowField")
            .def_rw("a", &ThrowField::a, ThrowOnFromCpp{})
            .def_rw("b", &ThrowField::b, 0)
            .finalize();
    });
}
