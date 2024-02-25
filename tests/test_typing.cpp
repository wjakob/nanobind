#include <nanobind/nanobind.h>
#include <nanobind/operators.h>

namespace nb = nanobind;
using namespace nb::literals;

// Declarations of various advanced constructions to test the stub generator
NB_MODULE(test_typing_ext, m) {
    // A submodule which won't be included, but we must be able to import it
    // and resolve declarations from there
    nb::module_ sm = m.def_submodule("submodule");

    // Some elements of the submodule
    struct F { };
    sm.def("f", [] { });
    nb::class_<F>(sm, "F");

    // Submodule aliases
    m.attr("f2") = sm.attr("f");
    m.attr("F") = sm.attr("F");

    // A top-level type and a function
    class Foo { };
    nb::class_<Foo>(m, "Foo");
    m.def("f", []{});

    // Aliases to local functoins and types
    m.attr("FooAlias") = m.attr("Foo");
    m.attr("f_alias") = m.attr("f");

    // Custom signature generation for classes and methods
    struct CustomSignature { int value; };
    nb::class_<CustomSignature>(
        m, "CustomSignature", nb::signature("@my_decorator\nclass CustomSignature(Iterable[int])"))
        .def("method", []{}, nb::signature("@my_decorator\ndef method(self: typing.Self)"))
        .def_rw("value", &CustomSignature::value,
                nb::for_getter(nb::signature("def value(self, /) -> Optional[int]")),
                nb::for_setter(nb::signature("def value(self, value: Optional[int], /) -> None")),
                nb::for_getter("docstring for getter"),
                nb::for_setter("docstring for setter"));

    // Stubification of simple constants
    nb::dict d;
    nb::list l;
    l.append(123);
    d["a"] = nb::make_tuple("b", l);
    m.attr("pytree") = d;

    // A generic type
    struct Wrapper {
        nb::object value;
        bool operator==(const Wrapper &w) const { return value.is(w.value); }
    };

    // 1. Instantiate a placeholder ("type variable") used below
    m.attr("T") = nb::type_var("T", "contravariant"_a = true);

    // 2. Create a generic type, and indicate in generated stubs
    //    that it derives from Generic[T]
    nb::class_<Wrapper>(m, "Wrapper", nb::is_generic(),
                        nb::signature("class Wrapper(typing.Generic[T])"))
       .def(nb::init<nb::object>(),
            nb::signature("def __init__(self, arg: T, /) -> None"))
       .def("get", [](Wrapper &w) { return w.value; },
            nb::signature("def get(self, /) -> T"))
       .def(nb::self == nb::self, nb::signature("def __eq__(self, arg: object, /) -> bool"));
}
