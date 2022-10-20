#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Value_, typename Key> struct set_caster {
    NB_TYPE_CASTER(Value_, const_name("set[") + make_caster<Key>::Name + const_name("]"));

    using KeyCaster = make_caster<Key>;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        value.clear();

        PyObject* iter = obj_iter(src.ptr());
        if (iter == nullptr) {
            PyErr_Clear();
            return false;
        }

        bool success = true;

        KeyCaster key_caster;
        for (PyObject* key = PyIter_Next(iter); key; key = PyIter_Next(iter)) {
            if (!key_caster.from_python(key, flags, cleanup)) {
                Py_DECREF(key);
                success = false;
                break;
            }
            value.emplace(((KeyCaster &&) key_caster).operator cast_t<Key&&>());
            Py_DECREF(key);
        }
        if (PyErr_Occurred()) {
            PyErr_Clear();
            success = false;
        }

        Py_DECREF(iter);

        return success;
    }

    template <typename T>
    static handle from_cpp(T &&src, rv_policy policy, cleanup_list *cleanup) {
        object ret = steal(PySet_New(nullptr));
        if (ret) {
            for (auto& key : src) {
                object k = steal(
                    KeyCaster::from_cpp(forward_like<T>(key), policy, cleanup));
                if (PySet_Add(ret.ptr(), k.ptr()) != 0) {
                    if (PyErr_Occurred()) {
                        PyErr_Clear();
                    }
                    return handle();
                }
                if (!k.is_valid()) return handle();
            }
        } else if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        return ret.release();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
