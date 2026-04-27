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
        value = std::bitset<N>(src.ptr());
        return true;
    }

    static handle from_cpp(const std::bitset<N> &value, rv_policy,
                           cleanup_list *) noexcept {
        return cast(value.to_ullong());
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
