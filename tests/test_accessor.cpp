#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct A { int value; };

NB_MODULE(test_accessor_ext, m) {
    nb::class_<A>(m, "A")
    .def(nb::init<>())
    .def_rw("value", &A::value);

    m.def("test_str_attr_accessor_inplace_mutation", []() {
        nb::object a_ = nb::module_::import_("test_accessor_ext").attr("A")();
        a_.attr("value") += nb::int_(1);
        return a_;
    });

    m.def("test_str_item_accessor_inplace_mutation", []() {
        nb::dict d;
        d["a"] = nb::int_(0);
        d["a"] += nb::int_(1);
        return d;
    });

    m.def("test_num_item_list_accessor_inplace_mutation", []() {
        nb::list l;
        l.append(nb::int_(0));
        l[0] += nb::int_(1);
        return l;
    });

    m.def("test_obj_item_accessor_inplace_mutation", []() {
        nb::dict d;
        nb::int_ key = nb::int_(0);
        d[key] = nb::int_(0);
        d[key] += nb::int_(1);
        return d;
    });
}
