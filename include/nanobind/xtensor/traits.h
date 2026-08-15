#pragma once

#include <nanobind/ndarray.h>
#include <nanobind/xtensor/version.h>
#include <xtensor/containers/xarray.hpp>
#include <xtensor/containers/xtensor.hpp>
#include <xtensor/core/xexpression.hpp>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T>
struct xcaster_traits;

template <typename T, typename = void>
constexpr bool has_xcaster_traits_v = false;

template <typename T>
constexpr bool has_xcaster_traits_v<T,
    std::void_t<typename xcaster_traits<T>::scalar_type>> = true;

template <typename T>
constexpr bool is_xexpression_v =
    xt::is_xexpression<T>::value && !has_xcaster_traits_v<T>;

template <typename T, xt::layout_type L = xt::layout_type::dynamic>
using xarray_view = xt::xarray_adaptor<
    xt::xbuffer_adaptor<T*, xt::no_ownership>,
    L,
    std::vector<std::size_t>>;

template <typename T, std::size_t N, xt::layout_type L = xt::layout_type::dynamic>
using xtensor_view = xt::xtensor_adaptor<
    xt::xbuffer_adaptor<T*, xt::no_ownership>,
    N,
    L>;

/// Pick an ndarray type with an order annotation that matches xtensor static layout.
template <typename Scalar, xt::layout_type L>
using layout_ndarray =
    std::conditional_t<L == xt::layout_type::row_major,
        ndarray<Scalar, numpy, c_contig>,
    std::conditional_t<L == xt::layout_type::column_major,
        ndarray<Scalar, numpy, f_contig>, ndarray<Scalar, numpy>>>;

/// Trait base for xarray, uses dynamic-size vectors with any ndim.
template <typename Scalar, xt::layout_type L>
struct xarray_traits_base {
    using scalar_type = Scalar;
    using shape_type = std::vector<size_t>;
    using stride_type = std::vector<int64_t>;
    static constexpr xt::layout_type layout = L;

    static bool check_ndim(size_t) { return true; }
    static shape_type make_shape(size_t nd) { return shape_type(nd); }
    static stride_type make_strides(size_t nd) { return stride_type(nd); }
};

/// Trait base for xtensor, uses fixed-size arrays with ndim matches N.
template <typename Scalar, std::size_t N, xt::layout_type L>
struct xtensor_traits_base {
    using scalar_type = Scalar;
    using shape_type = std::array<size_t, N>;
    using stride_type = std::array<int64_t, N>;
    static constexpr xt::layout_type layout = L;

    static bool check_ndim(size_t nd) { return nd == N; }
    static shape_type make_shape(size_t) { return {}; }
    static stride_type make_strides(size_t) { return {}; }
};

template <typename EC, xt::layout_type L, typename SC, typename Tag>
struct xcaster_traits<xt::xarray_container<EC, L, SC, Tag>>
    : xarray_traits_base<typename EC::value_type, L> {};

template <typename EC, std::size_t N, xt::layout_type L, typename Tag>
struct xcaster_traits<xt::xtensor_container<EC, N, L, Tag>>
    : xtensor_traits_base<typename EC::value_type, N, L> {};

template <typename T, xt::layout_type L>
struct xcaster_traits<xarray_view<T, L>>
    : xarray_traits_base<T, L> {};

template <typename T, std::size_t N, xt::layout_type L>
struct xcaster_traits<xtensor_view<T, N, L>>
    : xtensor_traits_base<T, N, L> {};

/// Inspect strides to determine the memory layout of an ndarray.
/// Returns row_major / column_major for contiguous data, dynamic for non-contiguous.
/// 1D contiguous arrays always return row_major (since row and column are equivalent for 1D),
/// so column_major is only returned for ndim >= 2.
template <typename NDArray>
xt::layout_type detect_layout(const NDArray &arr) {
    size_t ndim = arr.ndim();
    if (ndim == 0)
        return xt::layout_type::row_major;

    bool row_major = (arr.stride(ndim - 1) == 1);
    for (size_t i = ndim - 1; row_major && i > 0; --i)
        row_major = (arr.stride(i - 1) == static_cast<int64_t>(arr.shape(i)) * arr.stride(i));

    if (row_major)
        return xt::layout_type::row_major;

    if (ndim > 1) {
        bool col_major = (arr.stride(0) == 1);
        for (size_t i = 1; col_major && i < ndim; ++i)
            col_major = (arr.stride(i) == static_cast<int64_t>(arr.shape(i - 1)) * arr.stride(i - 1));

        if (col_major)
            return xt::layout_type::column_major;
    }

    return xt::layout_type::dynamic;
}

NAMESPACE_END(detail)

template <typename T, xt::layout_type L = xt::layout_type::dynamic>
using xarray_view = detail::xarray_view<T, L>;

template <typename T, std::size_t N, xt::layout_type L = xt::layout_type::dynamic>
using xtensor_view = detail::xtensor_view<T, N, L>;

NAMESPACE_END(NB_NAMESPACE)
