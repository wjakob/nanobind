/*
    nanobind/stl/bitset.h: type caster for std::bitset

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <bitset>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <size_t N>
struct type_caster<std::bitset<N>> {
    NB_TYPE_CASTER(std::bitset<N>, const_name("int"))

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        if (!PyNumber_Check(src.ptr())) {
            PyErr_Clear();
            return false;
        }
        str bin(builtins()["bin"](src));
        if (len(bin) - 2 > N) {
            PyErr_Clear();
            return false;
        }
        value = std::bitset<N>(bin.c_str() + 2); // std::bitset does not allow 0b prefix in string
        return true;
    }

    static handle from_cpp(const std::bitset<N> &value, rv_policy,
                           cleanup_list *) noexcept {
        auto str = value.to_string();
        return PyLong_FromString(str.data(), NULL, 2);
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
