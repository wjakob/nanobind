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

    // Regression test for a dangling/non-owning accessor key: an accessor
    // created from a handle key must keep that key alive for its own lifetime.
    m.def("test_obj_item_accessor_owns_key", []() {
        nb::dict d;
        // Use a freshly constructed (hence non-immortal) key so that
        // refcount changes are observable.
        nb::object key = nb::make_tuple(nb::int_(0xdead), nb::int_(0xbeef));
        d[key] = nb::int_(7);
        Py_ssize_t before = Py_REFCNT(key.ptr());
        auto acc = d[key];
        Py_ssize_t during = Py_REFCNT(key.ptr());
        return nb::cast<int>(acc) == 7 && during == before + 1;
    });
}
