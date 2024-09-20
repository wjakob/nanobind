#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

struct Counter {
    size_t value = 0;
    void inc() { value++; }
    void merge(Counter &o) {
        value += o.value;
        o.value = 0;
    }
};

nb::ft_mutex mutex;

NB_MODULE(test_thread_ext, m) {
    nb::class_<Counter>(m, "Counter")
        .def(nb::init<>())
        .def_ro("value", &Counter::value)
        .def("inc_unsafe", &Counter::inc)
        .def("inc_safe", &Counter::inc, nb::lock_self())
        .def("merge_unsafe", &Counter::merge)
        .def("merge_safe", &Counter::merge, nb::lock_self(), "o"_a.lock());

    m.def("return_self", [](Counter *c) -> Counter * { return c; });

    m.def("inc_safe",
          [](Counter &c) { c.inc(); },
          nb::arg().lock());

    m.def("inc_global",
          [](Counter &c) {
              nb::ft_lock_guard guard(mutex);
              c.inc();
          }, "counter");
}
