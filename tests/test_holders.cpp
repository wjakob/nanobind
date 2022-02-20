#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;

static int created = 0;
static int deleted = 0;

struct Example {
    int value;
    Example(int value) : value(value) { created++; }
    ~Example() { deleted++; }

    static Example *make(int value) { return new Example(value); }
    static std::shared_ptr<Example> make_shared(int value) {
        return std::make_shared<Example>(value);
    }
};

struct ExampleWrapper {
    std::shared_ptr<Example> test;
};

NB_MODULE(test_holders_ext, m) {
    nb::class_<Example>(m, "Example")
        .def(nb::init<int>())
        .def_readwrite("value", &Example::value)
        .def_static("make", &Example::make)
        .def_static("make_shared", &Example::make_shared);

    nb::class_<ExampleWrapper>(m, "ExampleWrapper")
        .def(nb::init<std::shared_ptr<Example>>())
        .def_readwrite("test", &ExampleWrapper::test)
        .def_property("value",
            [](ExampleWrapper &t) { return t.test->value; },
            [](ExampleWrapper &t, int value) { t.test->value = value; }
        );

    m.def("query_shared_1", [](Example *shared) { return shared->value; });
    m.def("query_shared_2", [](std::shared_ptr<Example> &shared) { return shared->value; });
    m.def("passthrough", [](std::shared_ptr<Example> shared) { return shared; });

    m.def("stats", []{ return std::make_pair(created, deleted); });
    m.def("clear", []{ created = deleted = 0; });
}
