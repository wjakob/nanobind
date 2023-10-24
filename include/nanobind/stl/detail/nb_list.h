/*
    nanobind/stl/detail/nb_list.h: base class of list casters

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename List, typename Entry> struct list_caster {
    NB_TYPE_CASTER(List, const_name(NB_TYPING_LIST "[") +
                               make_caster<Entry>::Name + const_name("]"))

    using Caster = make_caster<Entry>;

    template <typename T> using has_reserve = decltype(std::declval<T>().reserve(0));

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        size_t size;
        PyObject *temp;

        /* Will initialize 'size' and 'temp'. All return values and
           return parameters are zero/NULL in the case of a failure. */
        PyObject **o = seq_get(src.ptr(), &size, &temp);

        value.clear();

        if constexpr (is_detected_v<has_reserve, List>)
            value.reserve(size);

        Caster caster;
        bool success = o != nullptr;

        if constexpr (is_base_caster_v<Caster> && !std::is_pointer_v<Entry>)
            flags |= (uint8_t) cast_flags::none_disallowed;

        for (size_t i = 0; i < size; ++i) {
            if (!caster.from_python(o[i], flags, cleanup)) {
                success = false;
                break;
            }

            value.push_back(caster.operator cast_t<Entry>());
        }

        Py_XDECREF(temp);

        return success;
    }

    template <typename T>
    static handle from_cpp(T &&src, rv_policy policy, cleanup_list *cleanup) {
        object ret = steal(PyList_New(src.size()));

        if (ret.is_valid()) {
            Py_ssize_t index = 0;

            for (auto &&value : src) {
                handle h = Caster::from_cpp(forward_like<T>(value), policy, cleanup);

                if (!h.is_valid()) {
                    ret.reset();
                    break;
                }

                NB_LIST_SET_ITEM(ret.ptr(), index++, h.ptr());
            }
        }

        return ret.release();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
