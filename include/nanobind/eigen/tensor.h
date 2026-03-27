/*
    nanobind/eigen/tensor.h: type casters for Eigen tensors

    Copyright (c) 2026 Wilson Jallet

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/ndarray.h>

#include <unsupported/Eigen/CXX11/Tensor>


NAMESPACE_BEGIN(NB_NAMESPACE)

NAMESPACE_BEGIN(detail)

template<typename T> constexpr bool is_eigen_tensor_v = 
    std::is_base_of_v<Eigen::TensorBase<T, Eigen::ReadOnlyAccessors>, T>;

/** \brief Type caster for ``Eigen::TensorMap<T>``
 */
template<typename T, int MapOptions,
    template<class> class MakePointer>
struct type_caster<
    Eigen::TensorMap<T, MapOptions, MakePointer>,
    enable_if_t<is_eigen_tensor_v<T> && is_ndarray_scalar_v<typename T::Scalar>>>
     {
        using Scalar = typename T::Scalar;
        using IndexType = typename T::IndexType;
        static constexpr int NumIndices = T::NumIndices;
        static constexpr int Options = T::Options;
        using PlainTensor = Eigen::Tensor<Scalar, NumIndices, Options, IndexType>;
        using Dimensions = typename T::Dimensions;
        using MapType = Eigen::TensorMap<T, MapOptions, MakePointer>;

        // Only partial specification. Dimensions not known at compile time...
        using NDArray = ndarray<Scalar, numpy, device::cpu>;
        using NDArrayCaster = make_caster<NDArray>;

        static constexpr auto Name = NDArrayCaster::Name;
        template<typename T_> using Cast = MapType;
        template<typename T_> static constexpr bool can_cast() { return true; };

        NDArrayCaster caster;

        bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {

        }

        bool from_python_(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {}
};


template<typename Scalar, int NumIndices, int Options, typename IndexType>
struct type_caster<
        Eigen::Tensor<Scalar, NumIndices, Options, IndexType>
    > {
        using PlainTensor = Eigen::Tensor<Scalar, NumIndices, Options, IndexType>;
        using Dimensions = typename PlainTensor::Dimensions;
        using Coeffs = typename PlainTensor::CoeffReturnType;
};

/// TODO: implement caster for Eigen::TensorRef. 


NAMESPACE_END(detail)

NAMESPACE_END(NB_NAMESPACE)
