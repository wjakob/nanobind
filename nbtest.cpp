#include <nanobind/nanobind.h>

NB_MODULE(nbtest, m) {
    int i = 42;
    m.def("func", [i]() { fprintf(stderr, "Function was called: %i.", i); });
}
