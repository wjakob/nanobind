#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(nbtest, m) {
    int i = 42;
    m.def("test_1", [i](int j, int k) {
        fprintf(stderr, "Function was called: %i %i %i.\n", i, j, k);
    }, "j"_a, "k"_a);
}
