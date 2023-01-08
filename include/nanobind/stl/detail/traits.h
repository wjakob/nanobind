/*
    nanobind/stl/detail/traits.h: detail::is_copy_constructible<T>
    partial overloads for STL types

    Adapted from pybind11.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/* The builtin `std::is_copy_constructible` type trait merely checks whether
   a copy constructor is present and returns `true` even when the this copy
   constructor cannot be compiled. This a problem for older STL types like
   `std::vector<T>` when `T` is noncopyable. The alternative below recurses
   into STL types to work around this problem. */

template <typename T>
struct is_copy_constructible<
    T, enable_if_t<
           std::is_same_v<typename T::value_type &, typename T::reference> &&
           std::is_copy_constructible_v<T> &&
           !std::is_same_v<T, typename T::value_type>>> {
    static constexpr bool value =
        is_copy_constructible<typename T::value_type>::value;
};

template <typename T1, typename T2>
struct is_copy_constructible<std::pair<T1, T2>> {
    static constexpr bool value =
        is_copy_constructible<T1>::value ||
        is_copy_constructible<T2>::value;
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

