#pragma once

#include <nanobind/ndarray.h>
#include <nanobind/xtensor/traits.h>
#include <xtensor/containers/xadapt.hpp>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T>
struct type_caster<xt::xarray<T>, enable_if_t<is_ndarray_scalar_v<T>>> {
    using NDArray = ndarray<T, numpy>;
    using Caster = make_caster<NDArray>;

    NB_TYPE_CASTER(xt::xarray<T>, Caster::Name)

    Caster caster;

    bool from_python(handle src, uint8_t flags, cleanup_list *cl) noexcept {
        if (!caster.from_python(src, flags, cl))
            return false;

        const NDArray &a = caster.value;
        size_t nd = a.ndim();

        std::vector<size_t> shape(nd), strides(nd);
        for (size_t i = 0; i < nd; ++i) {
            shape[i] = a.shape(i);
            strides[i] = static_cast<size_t>(a.stride(i));
        }

        auto view = xt::adapt(a.data(), a.size(), xt::no_ownership(), shape, strides);
        value = view;
        return true;
    }

    template <typename T_>
    static handle from_cpp(T_ &&v, rv_policy policy, cleanup_list *cl) noexcept {
        policy = infer_policy<T_>(policy);
        return from_cpp_impl(std::forward<T_>(v), policy, cl,
                std::bool_constant<std::is_rvalue_reference_v<T_&&>>{});
    }

private:
    template <typename T_>
    static handle from_cpp_impl(T_ &&a, rv_policy policy, cleanup_list *cl,
                                std::true_type /* is_rvalue */) noexcept {
        size_t nd = a.dimension();
        std::vector<size_t> shape(nd);
        std::vector<int64_t> strides(nd);
        for (size_t i = 0; i < nd; ++i) {
            shape[i] = a.shape()[i];
            strides[i] = static_cast<int64_t>(a.strides()[i]);
        }

        void *ptr = (void *) a.data();
        object owner;

        if (policy == rv_policy::move) {
            Value *temp = new Value(std::move(a));
            owner = capsule(temp, [](void *p) noexcept { delete (Value*)p; });
            ptr = temp->data();
            policy = rv_policy::reference;
        } else if (policy == rv_policy::reference_internal && cl->self()) {
            owner = borrow(cl->self());
            policy = rv_policy::reference;
        }

        NDArray arr(ptr, nd, shape.data(), owner, strides.data());
        return Caster::from_cpp(arr, policy, cl);
    }
};

template <typename T>
struct type_caster<T, enable_if_t<is_xexpression_v<T> &&
                                  is_ndarray_scalar_v<typename T::value_type>>> {
    using Scalar = typename T::value_type;
    using Array = xt::xarray<Scalar>;
    using Caster = make_caster<Array>;

    static constexpr auto Name = Caster::Name;
    template <typename T_> using Cast = T;
    template <typename T_> static constexpr bool can_cast() { return true; }

    bool from_python(handle, uint8_t, cleanup_list*) noexcept = delete;

    template <typename T_>
    static handle from_cpp(T_ &&expr, rv_policy policy, cleanup_list *cl) noexcept {
        return Caster::from_cpp(Array(std::forward<T_>(expr)), policy, cl);
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
