#pragma once

#include <cstring>

#include <nanobind/ndarray.h>
#include <nanobind/xtensor/traits.h>
#include <xtensor/containers/xadapt.hpp>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Caster for owning xtensor containers (xt::xarray / xt::xtensor).
///
/// The input ndarray is unconstrained, this preserves
/// the single copy of the data. The layout check would trigger ndarray's
/// re-layout of the data, costing two passes over the data.
///
/// When the caller wants to restrict the layout, the flag nonconvert()
/// could be used, then mismatched non-dynamic input layouts will be rejected.
template <typename Container>
struct xcontainer_caster {
    using Traits = xcaster_traits<Container>;
    using Scalar = typename Traits::scalar_type;
    using NDArray = ndarray<Scalar, numpy>;
    using NDArrayCaster = make_caster<NDArray>;

    NB_TYPE_CASTER(Container, NDArrayCaster::Name)

    NDArrayCaster caster;

    bool from_python(handle src, uint8_t flags, cleanup_list *cl) noexcept {
        if (!caster.from_python(src, flags, cl))
            return false;

        NDArray &arr = caster.value;
        size_t ndim = arr.ndim();
        if (!Traits::check_ndim(ndim))
            return false;

        xt::layout_type detected_layout = detect_layout(arr);

        /// When the container declares a strict layout and noconvert() flag is set,
        /// reject layout of the ndarray that does not match layout of the container.
        /// 1D contiguous arrays always pass, since row_major and column_major
        /// layouts are equivalent for 1D arrays.
        if constexpr (Traits::layout != xt::layout_type::dynamic) {
            bool layout_matches = (detected_layout == Traits::layout) ||
                                  (ndim <= 1 && detected_layout != xt::layout_type::dynamic);

            if (!layout_matches && !(flags & (uint8_t) cast_flags::convert))
                return false;
        }

        auto shape = Traits::make_shape(ndim);
        for (size_t i = 0; i < ndim; ++i)
            shape[i] = arr.shape(i);

        /// When the container layout is dynamic, we use XTENSOR_DEFAULT_LAYOUT
        /// as the storage layout of the owning container (row_major by default).
        constexpr xt::layout_type storage_layout =
            Traits::layout == xt::layout_type::dynamic
                ? XTENSOR_DEFAULT_LAYOUT
                : Traits::layout;

        if (detected_layout == storage_layout) {
            /// Iteration optimization: when input array is contiguous
            /// and its layout matches the storage layout, we just memcpy() raw bytes.
            /// This avoids xtensor's generic iteration, which uses iterators
            /// even when the array is contiguous.
            value = Container::from_shape(shape);
            std::memcpy(value.data(), arr.data(), arr.size() * sizeof(Scalar));
        } else if (detected_layout != xt::layout_type::dynamic) {
            /// When the layout is non-dynamic, but does not match the storage layout,
            /// we wrap the numpy data in the adaptor and assign it to the container,
            /// then xtensor re-layouts the array while copying.
            value = xt::adapt<xt::layout_type::dynamic>(
                arr.data(), arr.size(), xt::no_ownership(), shape, detected_layout);
        } else {
            /// When input layout is dynamic, we consider the input
            /// as strided array and pass explicit strides to xtensor.
            auto strides = Traits::make_strides(ndim);
            for (size_t i = 0; i < ndim; ++i)
                strides[i] = static_cast<int64_t>(arr.stride(i));

            value = xt::adapt(arr.data(), arr.size(), xt::no_ownership(), shape, strides);
        }

        return true;
    }

    template <typename T_>
    static handle from_cpp(T_ &&v, rv_policy policy, cleanup_list *cl) noexcept {
        policy = infer_policy<T_>(policy);
        if constexpr (std::is_pointer_v<T_>)
            return from_cpp_internal((const Value &) *v, policy, cl);
        else
            return from_cpp_internal((const Value &) v, policy, cl);
    }

private:
    static handle from_cpp_internal(const Value &arr, rv_policy policy, cleanup_list *cl) noexcept {
        /// We pass the strict layout to the output ndarray type,
        /// so the Python signature shows the order.
        using OutArray = layout_ndarray<Scalar, Traits::layout>;
        using OutCaster = make_caster<OutArray>;

        size_t ndim = arr.dimension();

        auto shape = Traits::make_shape(ndim);
        auto strides = Traits::make_strides(ndim);
        for (size_t i = 0; i < ndim; ++i) {
            shape[i] = arr.shape()[i];
            strides[i] = static_cast<int64_t>(arr.strides()[i]);
        }

        void *ptr = (void *) arr.data();
        object owner;

        if (policy == rv_policy::move) {
            Value *temp = new Value((Value&&) arr);
            owner = capsule(temp, [](void *p) noexcept { delete (Value*)p; });
            ptr = temp->data();
            policy = rv_policy::reference;
        } else if (policy == rv_policy::reference_internal && cl->self()) {
            owner = borrow(cl->self());
            policy = rv_policy::reference;
        }

        OutArray ndarr(ptr, ndim, shape.data(), owner, strides.data());
        return OutCaster::from_cpp(ndarr, policy, cl);
    }
};

template <typename EC, xt::layout_type L, typename SC, typename Tag>
struct type_caster<xt::xarray_container<EC, L, SC, Tag>,
    enable_if_t<is_ndarray_scalar_v<typename EC::value_type>>>
    : xcontainer_caster<xt::xarray_container<EC, L, SC, Tag>> {};

template <typename EC, std::size_t N, xt::layout_type L, typename Tag>
struct type_caster<xt::xtensor_container<EC, N, L, Tag>,
    enable_if_t<is_ndarray_scalar_v<typename EC::value_type>>>
    : xcontainer_caster<xt::xtensor_container<EC, N, L, Tag>> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
