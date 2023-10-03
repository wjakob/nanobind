#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(test_eval_ext, m) {
    m.def("globals_contains_a", []() {
        return nb::globals().contains("a");
    });

    m.def("globals_add_b", []() {
        auto globals = nb::globals();
        globals["b"] = 123;
        return globals;
    });
}
