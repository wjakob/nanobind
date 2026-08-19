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

    // An accessor keyed by an lvalue borrows the key, since the caller holds
    // it for at least as long as the accessor exists.
    m.def("test_obj_item_accessor_borrows_key", []() {
        nb::dict d;
        // Use a freshly constructed (hence non-immortal) key so that
        // refcount changes are observable.
        nb::object key = nb::make_tuple(nb::int_(0xdead), nb::int_(0xbeef));
        d[key] = nb::int_(7);
        Py_ssize_t before = Py_REFCNT(key.ptr());
        auto acc = d[key];
        Py_ssize_t during = Py_REFCNT(key.ptr());
        return nb::cast<int>(acc) == 7 && during == before;
    });

    // Regression test for a dangling accessor key: an accessor keyed by a
    // temporary must keep that key alive for its own lifetime.
    m.def("test_obj_item_accessor_owns_key", []() {
        nb::dict d;
        nb::object key = nb::make_tuple(nb::int_(0xdead), nb::int_(0xbeef));
        d[key] = nb::int_(7);
        Py_ssize_t before = Py_REFCNT(key.ptr());
        {
            auto acc = d[nb::borrow(key)];
            if (nb::cast<int>(acc) != 7 ||
                Py_REFCNT(key.ptr()) != before + 1)
                return false;
        }
        return Py_REFCNT(key.ptr()) == before;
    });

    // The same for attribute access, whose key additionally feeds method calls
    m.def("test_obj_attr_accessor_owns_key", []() {
        nb::object a_ = nb::module_::import_("test_accessor_ext").attr("A")();
        nb::object key = nb::steal(PyUnicode_FromString("value"));
        Py_ssize_t before = Py_REFCNT(key.ptr());
        {
            auto acc = a_.attr(nb::borrow(key));
            acc = nb::int_(3);
            if (Py_REFCNT(key.ptr()) != before + 1)
                return false;
        }
        return Py_REFCNT(key.ptr()) == before &&
               nb::cast<int>(a_.attr("value")) == 3;
    });

    // A nested accessor is a temporary as well, so its key must be captured
    m.def("test_nested_accessor_key", []() {
        nb::dict outer, inner;
        inner["a"] = nb::make_tuple(nb::int_(0xdead), nb::int_(0xbeef));
        outer[nb::make_tuple(nb::int_(0xdead), nb::int_(0xbeef))] = nb::int_(9);
        auto acc = outer[inner["a"]];
        inner.clear();
        return nb::cast<int>(acc) == 9;
    });

    // Converting a temporary accessor hands out its cached reference; the
    // result must remain valid and correctly owned.
    // Attribute access with a runtime-generated key takes the pointer path
    // of the string cache: it must work without inserting into the cache or
    // the interpreter's interned-string table
    m.def("getattr_dynamic", [](nb::handle o, const char *name) {
        return nb::object(o.attr(name));
    });
    m.def("setattr_dynamic", [](nb::handle o, const char *name, nb::handle v) {
        o.attr(name) = v;
    });
    m.def("hasattr_dynamic", [](nb::handle o, const char *name) {
        return nb::hasattr(o, name);
    });

    // Attribute access with a default, keyed by a Python object
    m.def("getattr_obj_def", [](nb::handle o, nb::handle key, nb::handle def) {
        return nb::getattr(o, key, def);
    }, nb::arg(), nb::arg(), nb::arg().none());

    m.def("test_accessor_conversion_refcount", []() {
        nb::dict d;
        nb::object value = nb::make_tuple(nb::int_(1), nb::int_(2));
        d["k"] = value;
        Py_ssize_t before = Py_REFCNT(value.ptr());
        {
            nb::object v1 = d["k"];
            auto acc = d["k"];
            nb::object v2 = acc, v3 = acc;
            if (Py_REFCNT(value.ptr()) != before + 4)
                return false;
        }
        return Py_REFCNT(value.ptr()) == before;
    });

    // Returning an accessor defers the lookup to the return value conversion
    m.def("test_return_item_accessor", [](nb::object o) { return o["k"]; });
    m.def("test_return_dict_accessor", [](nb::dict d) { return d["k"]; });
}
