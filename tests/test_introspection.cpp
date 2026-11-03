#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

struct Methods {};
struct Arrow {};
struct ReentrantType {};

namespace nanobind::detail {
template <> struct type_caster<Arrow> {
    // An arrow inside a type name must not be mistaken for the return separator.
    static constexpr auto Name = const_name("typing.Literal['->']");
};
}
using ArrowObject = nb::typed<nb::object, Arrow>;

NB_MODULE(test_introspection_ext, m) {
    nb::class_<ReentrantType>(m, "ReentrantType");
    // Rebinding target transfers the old callable's records during introspection.
    m.def("add_overload", [scope = m.ptr()](nb::object value, bool native) {
        if (native)
            nb::borrow<nb::module_>(scope).def("target", [](ReentrantType, nb::object x) { return x; },
                       "value"_a, "x"_a = value);
        else
            nb::borrow<nb::module_>(scope).def("target", [](nb::object x) { return x; },
                       nb::sig("def target(x: int = 1) -> int"), "x"_a = value);
    });
    m.def("arrow_annotations", [](ArrowObject, std::vector<ArrowObject> values) { return values; },
          "first"_a, "values"_a);
    m.def("make_function", [](nb::object value, bool display_override) {
        // Display text must not replace the actual default object.
        return nb::cpp_function([](nb::object x) { return x; },
                                nb::arg("x").sig(display_override ? "display_only()" : nullptr) = value);
    }, "value"_a, "display_override"_a = false);
    m.def("make_custom_function", [](const char *signature, nb::object value) {
        // nb::sig changes introspection only; the callable still takes one object.
        return nb::cpp_function([](nb::object x) { return x; },
                                nb::sig(signature), "x"_a = value);
    });
    m.def("make_custom_method", [](const char *signature) {
        return nb::cpp_function([](nb::object, nb::args) {},
                                nb::is_method(), nb::sig(signature));
    });
    m.def("unnamed_after_named", [](int x, int y) { return x + y; }, "x"_a, nb::arg());
    // Receiver keyword support depends on dispatch metadata, not just is_method.
    nb::class_<Methods>(m, "Methods")
        .def(nb::init<>())
        .def("unnamed", [](Methods &, int x) { return x; }, nb::arg())
        .def("unnamed_none", [](Methods &, nb::object x) { return x; }, nb::arg().none())
        .def("unnamed_default", [](Methods &, int x) { return x; }, nb::arg() = 1)
        .def("mixed", [](Methods &, int x, int y) { return x + y; }, nb::arg() = 1, "y"_a)
        .def("mixed_defaults", [](Methods &, int x, int y) { return x + y; }, nb::arg() = 1, "y"_a = 2)
        .def("named", [](Methods &, double x) { return x; }, "x"_a)
        .def("varargs", [](nb::args args) { return args.size(); })
        .def("empty", [] { return 7; })
        .def("overloaded", [](Methods &, int x) { return x; }, "x"_a)
        .def("overloaded", [](Methods &, double x) { return x; }, "x"_a);
}
