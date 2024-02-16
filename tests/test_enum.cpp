#include <nanobind/nanobind.h>

namespace nb = nanobind;

enum class Enum  : uint32_t { A, B, C = (uint32_t) -1 };
enum class SEnum : int32_t { A, B, C = (int32_t) -1 };
enum ClassicEnum { Item1, Item2 };

struct EnumProperty { Enum get_enum() { return Enum::A; } };

enum class Color : uint8_t {
    Black = 0,
    Red = 1,
    Green = 2,
    Yellow = 3,
    Blue = 4,
    Magenta = 5,
    Cyan = 6,
    White = 7
};
static PyObject *color_or(PyObject *a, PyObject *b) {
    PyObject *ia = PyNumber_Long(a);
    PyObject *ib = PyNumber_Long(b);
    if (!ia || !ib)
        return nullptr;
    PyObject *result = PyNumber_Or(ia, ib);
    Py_DECREF(ia);
    Py_DECREF(ib);
    if (!result)
        return nullptr;
    PyObject *wrapped_result = PyObject_CallFunctionObjArgs(
            (PyObject *) Py_TYPE(a), result, nullptr);
    Py_DECREF(result);
    return wrapped_result;
}
static PyType_Slot color_slots[] = {
    { Py_nb_or, (void *) color_or },
    { 0, nullptr }
};

NB_MODULE(test_enum_ext, m) {
    nb::enum_<Enum>(m, "Enum", "enum-level docstring")
        .value("A", Enum::A, "Value A")
        .value("B", Enum::B, "Value B")
        .value("C", Enum::C, "Value C")
        // ensure that cyclic dependencies are handled correctly
        .def("dummy", [](Enum, Enum) { }, nb::arg("arg") = Enum::A);

    nb::enum_<SEnum>(m, "SEnum", nb::is_arithmetic())
        .value("A", SEnum::A)
        .value("B", SEnum::B)
        .value("C", SEnum::C);

    nb::enum_<ClassicEnum>(m, "ClassicEnum")
        .value("Item1", ClassicEnum::Item1)
        .value("Item2", ClassicEnum::Item2)
        .export_values();

    // test with custom type slots
    nb::enum_<Color>(m, "Color", nb::is_arithmetic(), nb::type_slots(color_slots))
        .value("Black", Color::Black)
        .value("Red", Color::Red)
        .value("Green", Color::Green)
        .value("Blue", Color::Blue)
        .value("Cyan", Color::Cyan)
        .value("Yellow", Color::Yellow)
        .value("Magenta", Color::Magenta)
        .value("White", Color::White);

    m.def("from_enum", [](Enum value) { return (uint32_t) value; });
    m.def("to_enum", [](uint32_t value) { return (Enum) value; });
    m.def("from_enum", [](SEnum value) { return (int32_t) value; });

    // test for issue #39
    nb::class_<EnumProperty>(m, "EnumProperty")
        .def(nb::init<>())
        .def_prop_ro("read_enum", &EnumProperty::get_enum);
}
