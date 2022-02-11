#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(nbtest, m) {
    int i = 42;

    auto test_01 = [](int j, int k) -> int { return j - k; };

    // Function without inputs/outputs
    m.def("test_01", []() { });

    // Simple binary function
    m.def("test_02", (int (*)(int, int)) test_01, "j"_a = 8, "k"_a = 1);

    // Simple binary function with capture object
    m.def("test_03", [i](int j, int k) -> int { return i + j - k; });

    // Large capture object
    uint64_t k = 10, l = 11, m_ = 12, n = 13, o = 14;
    m.def("test_04", [k, l, m_, n, o]() -> int { return k + l + m_ + n + o; });

    // Overload chain with two docstrings
    m.def("test_05", [](int) -> int { return 1; }, "doc_1");
    m.def("test_05", [](float) -> int { return 2; }, "doc_2");

    /// Function raising an exception
    m.def("test_06", []() { throw std::runtime_error("oops!"); });
}
