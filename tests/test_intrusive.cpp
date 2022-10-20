#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/trampoline.h>
#include "object.h"
#include "object_py.h"

namespace nb = nanobind;
using namespace nb::literals;

static int test_constructed = 0;
static int test_destructed = 0;

class Test : Object {
public:
    Test() {
        test_constructed++;
    }

    virtual ~Test() {
        test_destructed++;
    }

    virtual int value() const { return 123; }

    static Test *create_raw() { return new Test(); }
    static ref<Test> create_ref() { return new Test(); }
};

class PyTest : Test {
    NB_TRAMPOLINE(Test, 1);
    virtual int value() const {
        NB_OVERRIDE(int, Test, value);
    }
};

NB_MODULE(test_intrusive_ext, m) {
    object_init_py(
        [](PyObject *o) noexcept {
            nb::gil_scoped_acquire guard;
            Py_INCREF(o);
        },
        [](PyObject *o) noexcept {
            nb::gil_scoped_acquire guard;
            Py_DECREF(o);
        });

    nb::class_<Object>(
        m, "Object",
        nb::intrusive_ptr<Object>(
            [](Object *o, PyObject *po) noexcept { o->set_self_py(po); }));

    nb::class_<Test, Object, PyTest>(m, "Test")
        .def(nb::init<>())
        .def("value", &Test::value)
        .def_static("create_raw", &Test::create_raw)
        .def_static("create_ref", &Test::create_ref);

    m.def("reset", [] {
        test_constructed = 0;
        test_destructed = 0;
    });

    m.def("stats", []() -> std::pair<int, int> {
        return { test_constructed, test_destructed };
    });

    m.def("get_value_1", [](Test *o) { ref<Test> x(o); return x->value(); });
    m.def("get_value_2", [](ref<Test> x) { return x->value(); });
    m.def("get_value_3", [](const ref<Test> &x) { return x->value(); });
}
