/*
    nanobind/stl/detail/nb_dict.h: base class of dict casters

    Copyright (c) 2022 Matej Ferencevic and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Value_, typename Key, typename Element> struct dict_caster {
    NB_TYPE_CASTER(Value_, const_name(NB_TYPING_DICT "[") + make_caster<Key>::Name +
                               const_name(", ") + make_caster<Element>::Name +
                               const_name("]"));

    using KeyCaster = make_caster<Key>;
    using ElementCaster = make_caster<Element>;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        value.clear();

        PyObject *items = PyMapping_Items(src.ptr());
        if (items == nullptr) {
            PyErr_Clear();
            return false;
        }

        Py_ssize_t size = NB_LIST_GET_SIZE(items);
        bool success = (size >= 0);

        KeyCaster key_caster;
        ElementCaster element_caster;
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject *item = NB_LIST_GET_ITEM(items, i);
            PyObject *key = NB_TUPLE_GET_ITEM(item, 0);
            PyObject *element = NB_TUPLE_GET_ITEM(item, 1);

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

        Py_DECREF(items);

        return success;
    }

    template <typename T>
    static handle from_cpp(T &&src, rv_policy policy, cleanup_list *cleanup) {
        dict ret;

        if (ret.is_valid()) {
            for (auto &item : src) {
                object k = steal(KeyCaster::from_cpp(
                    forward_like<T>(item.first), policy, cleanup));
                object e = steal(ElementCaster::from_cpp(
                    forward_like<T>(item.second), policy, cleanup));

                if (!k.is_valid() || !e.is_valid() ||
                    PyDict_SetItem(ret.ptr(), k.ptr(), e.ptr()) != 0) {
                    ret.reset();
                    break;
                }
            }
        }

        return ret.release();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
