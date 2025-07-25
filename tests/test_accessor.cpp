#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct A { int value; };

NB_MODULE(test_accessor_ext, m) {
    nb::class_<A>(m, "A")
    .def(nb::init<>())
    .def_rw("value", &A::value);

    m.def("test_accessor_inplace_mutation", []() {
        nb::object a_ = nb::module_::import_("test_accessor_ext").attr("A")();
        a_.attr("value") += nb::int_(1);
        return nb::cast<int>(a_.attr("value"));
    });
}
