#if defined(__GNUC__)
// warning: '..' declared with greater visibility than the type of its field '..'
#  pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/unique_ptr.h>
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

struct SharedWrapper { std::shared_ptr<Example> value; };
struct UniqueWrapper { std::unique_ptr<Example> value; };
struct UniqueWrapper2 { std::unique_ptr<Example, nb::deleter<Example>> value; };

NB_MODULE(test_holders_ext, m) {
    nb::class_<Example>(m, "Example")
        .def(nb::init<int>())
        .def_readwrite("value", &Example::value)
        .def_static("make", &Example::make)
        .def_static("make_shared", &Example::make_shared);

    // ------- shared_ptr -------

    nb::class_<SharedWrapper>(m, "SharedWrapper")
        .def(nb::init<std::shared_ptr<Example>>())
        .def_readwrite("ptr", &SharedWrapper::value)
        .def_property("value",
            [](SharedWrapper &t) { return t.value->value; },
            [](SharedWrapper &t, int value) { t.value->value = value; }
        );

    m.def("query_shared_1", [](Example *shared) { return shared->value; });
    m.def("query_shared_2",
          [](std::shared_ptr<Example> &shared) { return shared->value; });
    m.def("passthrough",
          [](std::shared_ptr<Example> shared) { return shared; });

    // ------- unique_ptr -------

    m.def("unique_from_cpp",
          []() { return std::make_unique<Example>(1); });
    m.def("unique_from_cpp_2", []() {
        return std::unique_ptr<Example, nb::deleter<Example>>(new Example(2));
    });

    nb::class_<UniqueWrapper>(m, "UniqueWrapper")
        .def(nb::init<std::unique_ptr<Example>>())
        .def("get", [](UniqueWrapper *uw) { return std::move(uw->value); });

    nb::class_<UniqueWrapper2>(m, "UniqueWrapper2")
        .def(nb::init<std::unique_ptr<Example, nb::deleter<Example>>>())
        .def("get", [](UniqueWrapper2 *uw) { return std::move(uw->value); });

    m.def("passthrough_unique",
          [](std::unique_ptr<Example> unique) { return unique; });
    m.def("passthrough_unique_2",
          [](std::unique_ptr<Example, nb::deleter<Example>> unique) { return unique; });

    m.def("stats", []{ return std::make_pair(created, deleted); });
    m.def("reset", []{ created = deleted = 0; });
}
