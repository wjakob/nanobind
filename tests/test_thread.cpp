#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>

#include <memory>
#include <vector>

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

struct AnInt {
    int value;
    AnInt(int v) : value(v) {}
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

    nb::class_<AnInt>(m, "AnInt")
        .def(nb::init<int>())
        .def_rw("value", &AnInt::value);

    std::vector<std::shared_ptr<AnInt>> shared_ints;
    for (int i = 0; i < 5; ++i) {
        shared_ints.push_back(std::make_shared<AnInt>(i));
    }
    m.def("fetch_shared_int", [shared_ints](int i) {
        return shared_ints.at(i);
    });
    m.def("consume_an_int", [](AnInt* p) { return p->value; });
}
