#include <nanobind/nanobind.h>

namespace nb = nanobind;

enum class Enum  : uint32_t { A, B, C = (uint32_t) -1 };
enum class SEnum : int32_t { A, B, C = (int32_t) -1 };

NB_MODULE(test_enum_ext, m) {
    nb::enum_<Enum>(m, "Enum")
        .value("A", Enum::A, "Value A")
        .value("B", Enum::B, "Value B")
        .value("C", Enum::C, "Value C");

    nb::enum_<SEnum>(m, "SEnum", nb::is_arithmetic())
        .value("A", SEnum::A)
        .value("B", SEnum::B)
        .value("C", SEnum::C);

    m.def("from_enum", [](Enum value) { return (uint32_t) value; });
    m.def("to_enum", [](uint32_t value) { return (Enum) value; });
    m.def("from_enum", [](SEnum value) { return (int32_t) value; });
}
