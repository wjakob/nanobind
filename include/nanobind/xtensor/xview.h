#pragma once

#include <optional>
#include <nanobind/ndarray.h>
#include <nanobind/xtensor/traits.h>
#include <xtensor/containers/xadapt.hpp>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Caster for zero-copy views (xt::xarray_adaptor / xt::xtensor_adaptor).
/// Views wrap existing numpy memory without copying.
///
/// The input ndarray should match the container static layout.
/// Mismatched layouts are rejected, since views cannot copy the data.
/// Dynamic layouts accept any strides.
template <typename View>
struct xview_caster {
    using Traits = xcaster_traits<View>;
    using Scalar = typename Traits::scalar_type;

    /// Compile-time layout from the View type determines iteration strategy.
    /// The strict layout (row_major / column_major) enables fast iteration,
    /// dynamic layout falls back to the slower stride-based stepping.
    static constexpr xt::layout_type ViewLayout = View::static_layout;

    using NDArray = layout_ndarray<Scalar, ViewLayout>;
    using NDArrayCaster = make_caster<NDArray>;

    static constexpr auto Name = NDArrayCaster::Name;
    template <typename T_> using Cast = movable_cast_t<T_>;
    template <typename T_> static constexpr bool can_cast() { return true; }

    NDArrayCaster caster;
    std::optional<View> view_;

    explicit operator View*() { return &*view_; }
    explicit operator View&() { return *view_; }
    explicit operator View&&() { return (View&&) *view_; }

    bool from_python(handle src, uint8_t flags, cleanup_list *cl) noexcept {
        /// Disable convert flag. Views wrap existing memory without copying,
        /// so implicit conversions are not supported.
        if (!caster.from_python(src, flags & ~(uint8_t)cast_flags::convert, cl))
            return false;

        NDArray &arr = caster.value;
        size_t ndim = arr.ndim();
        if (!Traits::check_ndim(ndim))
            return false;

        auto shape = Traits::make_shape(ndim);
        for (size_t i = 0; i < ndim; ++i)
            shape[i] = arr.shape(i);

        if constexpr (ViewLayout != xt::layout_type::dynamic) {
            /// Iteration optimization: when the input is contiguous,
            /// xtensor computes strides from shape.
            view_.emplace(xt::adapt<ViewLayout>(
                static_cast<Scalar*>(arr.data()), arr.size(),
                xt::no_ownership(), std::move(shape), ViewLayout));
        } else {
            /// When the input is dynamic, we detect the input layout.
            xt::layout_type layout = detect_layout(arr);

            if (layout != xt::layout_type::dynamic) {
                /// If the array is contiguous, pass the detected layout
                /// to xtensor to enable its internal optimizations.
                view_.emplace(xt::adapt<xt::layout_type::dynamic>(
                    static_cast<Scalar*>(arr.data()), arr.size(),
                    xt::no_ownership(), std::move(shape), layout));
            } else {
                /// When the array is non-contiguous, pass the explicit strides.
                auto strides = Traits::make_strides(ndim);
                for (size_t i = 0; i < ndim; ++i)
                    strides[i] = static_cast<int64_t>(arr.stride(i));

                view_.emplace(xt::adapt(
                    static_cast<Scalar*>(arr.data()), arr.size(),
                    xt::no_ownership(), std::move(shape), std::move(strides)));
            }
        }

        return true;
    }

    template <typename T_>
    static handle from_cpp(T_ &&arr, rv_policy policy, cleanup_list *cl) noexcept {
        size_t ndim = arr.dimension();

        auto shape = Traits::make_shape(ndim);
        auto strides = Traits::make_strides(ndim);
        for (size_t i = 0; i < ndim; ++i) {
            shape[i] = arr.shape()[i];
            strides[i] = static_cast<int64_t>(arr.strides()[i]);
        }

        object owner;
        if (policy == rv_policy::reference_internal && cl->self()) {
            owner = borrow(cl->self());
            policy = rv_policy::reference;
        }

        NDArray ndarr((void *) arr.data(), ndim, shape.data(), owner, strides.data());
        if (policy == rv_policy::automatic || policy == rv_policy::automatic_reference)
            policy = rv_policy::reference;

        return NDArrayCaster::from_cpp(ndarr, policy, cl);
    }
};

template <typename T, xt::layout_type L>
struct type_caster<xarray_view<T, L>, enable_if_t<is_ndarray_scalar_v<T>>>
    : xview_caster<xarray_view<T, L>> {};

template <typename T, std::size_t N, xt::layout_type L>
struct type_caster<xtensor_view<T, N, L>, enable_if_t<is_ndarray_scalar_v<T>>>
    : xview_caster<xtensor_view<T, N, L>> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
