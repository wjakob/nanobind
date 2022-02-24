#include <nanobind/nanobind.h>

namespace nb = nanobind;

enum class Enum1  : uint32_t { A, B, C = (uint32_t) -1 };
enum class SEnum1 : int32_t { A, B, C = (int32_t) -1 };

NB_MODULE(test_enum_ext, m) {
    nb::enum_<Enum1>(m, "Enum1")
        .value("A", Enum1::A, "Value A")
        .value("B", Enum1::B, "Value B")
        .value("C", Enum1::C, "Value C");

    nb::enum_<SEnum1>(m, "SEnum1")
        .value("A", SEnum1::A)
        .value("B", SEnum1::B)
        .value("C", SEnum1::C);
}
