#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Value_, typename Key, typename Element> struct dict_caster {
    NB_TYPE_CASTER(Value_, const_name("dict[") + make_caster<Key>::Name +
                               const_name(", ") + make_caster<Element>::Name +
                               const_name("]"));

    using KeyCaster = make_caster<Key>;
    using ElementCaster = make_caster<Element>;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        value.clear();

        PyObject *items = PyDict_Items(src.ptr());
        if (items == nullptr) return false;

        Py_ssize_t size = PyList_Size(items);
        bool success = (size >= 0);

        KeyCaster key_caster;
        ElementCaster element_caster;
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject *item = PyList_GetItem(items, i);
            PyObject *key = PyTuple_GetItem(item, 0);
            PyObject *element = PyTuple_GetItem(item, 1);
            if (!key_caster.from_python(key, flags, cleanup)) {
                success = false;
                break;
            }
            if (!element_caster.from_python(element, flags, cleanup)) {
                success = false;
                break;
            }
            value.emplace(((KeyCaster &&) key_caster).operator cast_t<Key &&>(),
                          ((ElementCaster &&) element_caster).operator cast_t<Element &&>());
        }

        Py_XDECREF(items);

        return success;
    }

    template <typename T>
    static handle from_cpp(T &&src, rv_policy policy, cleanup_list *cleanup) {
        object dict = steal(PyDict_New());
        if (dict) {
            for (auto &item : src) {
                object k = steal(KeyCaster::from_cpp(
                    forward_like<typename T::key_type>(item.first), policy, cleanup));
                object e = steal(ElementCaster::from_cpp(
                    forward_like<typename T::mapped_type>(item.second), policy, cleanup));
                if (PyDict_SetItem(dict.ptr(), k.ptr(), e.ptr()) != 0) return handle();
                if (!k.is_valid() || !e.is_valid()) return handle();
            }
        }
        return dict.release();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
