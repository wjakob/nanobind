#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T1, typename T2> struct type_caster<std::pair<T1, T2>> {
    using T = std::pair<T1, T2>;
    using C1 = make_caster<T1>;
    using C2 = make_caster<T2>;

    NB_TYPE_CASTER(T, const_name("Tuple[") + concat(C1::cname, C2::cname) +
                          const_name("]"))

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        PyObject *o[2];

        if (!seq_size_fetch(src.ptr(), 2, o))
            return false;

        C1 c1;
        C2 c2;

        if (!c1.from_python(o[0], flags, cleanup))
            goto fail;
        if (!c2.from_python(o[1], flags, cleanup))
            goto fail;

        value.first  = std::move(c1.value);
        value.second = std::move(c2.value);

        return true;

    fail:
        Py_DECREF(o[0]);
        Py_DECREF(o[1]);

        return false;
    }

    static handle from_cpp(const T &value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        object o1 = steal(C1::from_cpp(value.first, policy, cleanup));
        if (!o1.is_valid())
            return handle();

        object o2 = steal(C2::from_cpp(value.second, policy, cleanup));
        if (!o2.is_valid())
            return handle();

        PyObject *r = PyTuple_New(2);
        PyTuple_SET_ITEM(r, 0, o1.release().ptr());
        PyTuple_SET_ITEM(r, 1, o2.release().ptr());
        return r;
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
