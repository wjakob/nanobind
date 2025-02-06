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

struct GlobalData {} global_data;

nb::ft_mutex mutex;

struct ClassWithProperty {
public:
    ClassWithProperty(int value): value_(value) {}
    int get_prop() const { return value_; }
private:
    int value_;
};

class ClassWithClassProperty {
public:
    ClassWithClassProperty(ClassWithProperty value) : value_(std::move(value)) {};
    const ClassWithProperty& get_prop() const { return value_; }
private:
    ClassWithProperty value_;
};


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

    nb::class_<GlobalData>(m, "GlobalData")
        .def_static("get", [] { return &global_data; }, nb::rv_policy::reference);

    nb::class_<ClassWithProperty>(m, "ClassWithProperty")
        .def(nb::init<int>(), nb::arg("value"))
        .def_prop_ro("prop2", &ClassWithProperty::get_prop);

    nb::class_<ClassWithClassProperty>(m, "ClassWithClassProperty")
        .def(
          "__init__",
          [](ClassWithClassProperty* self, ClassWithProperty value) {
            new (self) ClassWithClassProperty(std::move(value));
          }, nb::arg("value"))
        .def_prop_ro("prop1", &ClassWithClassProperty::get_prop);
}
