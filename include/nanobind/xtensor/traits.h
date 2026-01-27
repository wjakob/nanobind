#pragma once

#include <nanobind/xtensor/version.h>
#include <xtensor/containers/xarray.hpp>
#include <xtensor/core/xexpression.hpp>
#include <type_traits>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// xarray detection
template <typename T>
struct is_xarray : std::false_type {};

template <typename T>
struct is_xarray<xt::xarray<T>> : std::true_type {};

template <typename T>
constexpr bool is_xarray_v = is_xarray<T>::value;

// xexpression detection
template <typename T>
constexpr bool is_xexpression_v =
    xt::is_xexpression<T>::value &&
    !is_xarray_v<T>;

NAMESPACE_END(detail)

using detail::is_xarray_v;
using detail::is_xexpression_v;

NAMESPACE_END(NB_NAMESPACE)
