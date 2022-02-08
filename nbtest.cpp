#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(nbtest, m) {
    int i = 42;

    auto test_01 = [](int j, int k) -> int { return j - k; };

    m.def("test_01", (int (*)(int, int)) test_01, "j"_a = 8, "k"_a = 1,
          "a docstring");

    m.def("test_02", [i](int j, int k) -> int { return i + j - k; });

    uint64_t k = 10, l = 11, m_ = 12, n = 13, o = 14;
    m.def("test_03", [k, l, m_, n, o]() -> int { return k+l+m_+n+o; });

    m.def("test_04", []() { });
}
