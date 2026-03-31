/*
    nanobind/eigen/tensor.h: type casters for Eigen tensors

    Copyright (c) 2026 INRIA

    Author(s): Wilson Jallet

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/ndarray.h>

#include <unsupported/Eigen/CXX11/Tensor>


NAMESPACE_BEGIN(NB_NAMESPACE)

NAMESPACE_BEGIN(detail)

/// Type trait for inheriting from Eigen::TensorBase.
/// All TensorBase specializations inherit from TensorBase<T, ReadOnlyAccessors>.
template<typename T> constexpr bool is_eigen_tensor_v = 
    std::is_base_of_v<Eigen::TensorBase<T, Eigen::ReadOnlyAccessors>, T>;

template<typename T> constexpr bool eigen_tensor_is_row_major_v = T::Layout == Eigen::RowMajor;
template<typename T> constexpr bool eigen_tensor_is_col_major_v = T::Layout == Eigen::ColMajor;

template<typename T>
constexpr bool is_eigen_tensor_map_v = false;

// Covers const case
template<typename T, int Options, template<class> class MakePointer>
constexpr bool is_eigen_tensor_map_v<Eigen::TensorMap<T, Options, MakePointer>> = true;

template<typename T>
constexpr bool is_eigen_tensor_ref_v = false;

// Covers const case
template<typename T>
constexpr bool is_eigen_tensor_ref_v<Eigen::TensorRef<T>> = true;

template<typename T, typename Scalar = typename T::Scalar>
using ndarray_for_eigen_tensor_t = ndarray<
    Scalar,
    numpy,
    ndim<T::NumDimensions>, 
    std::conditional_t<
        eigen_tensor_is_row_major_v<T>,
        c_contig,
        f_contig>>;

/** \brief Type caster for ``Eigen::TensorMap<T>``
 */
template<typename T, int MapOptions,
    template<class> class MakePointer>
struct type_caster<
    Eigen::TensorMap<T, MapOptions, MakePointer>,
    enable_if_t<is_eigen_tensor_v<T> && is_ndarray_scalar_v<typename T::Scalar>>> {

    using Scalar = typename T::Scalar;
    using IndexType = typename T::IndexType;
    static constexpr int NumIndices = T::NumIndices;
    static constexpr int Options = T::Options;
    using PlainTensor = Eigen::Tensor<Scalar, NumIndices, Options, IndexType>;
    using Dimensions = typename T::Dimensions;
    using MapType = Eigen::TensorMap<T, MapOptions, MakePointer>;

    // Only partial specification. Dimensions not known at compile time...
    using NDArray =
        ndarray_for_eigen_tensor_t<T, std::conditional_t<std::is_const_v<T>,
                                                         const Scalar,
                                                         Scalar>>;
    using NDArrayCaster = make_caster<NDArray>;
    static constexpr auto Name = NDArrayCaster::Name;
    template<typename T_> using Cast = MapType;
    template<typename T_> static constexpr bool can_cast() { return true; };

    NDArrayCaster caster;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        // Disable implicit conversions
        return from_python_(src, flags & ~(uint8_t)cast_flags::convert, cleanup);
    }

    bool from_python_(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        
    }

    static handle from_cpp(const MapType &v, rv_policy policy, cleanup_list *cleanup) noexcept = delete;
};


template<typename Scalar, int NumIndices, int Options, typename IndexType>
struct type_caster<
        Eigen::Tensor<Scalar, NumIndices, Options, IndexType>,
        enable_if_t<is_ndarray_scalar_v<Scalar>>> {

    using PlainTensor = Eigen::Tensor<Scalar, NumIndices, Options, IndexType>;
    using Dimensions = typename PlainTensor::Dimensions;
    using Coeffs = typename PlainTensor::CoeffReturnType;
    static constexpr bool IsRowMajor = bool(Options & Eigen::RowMajorBit);
    using NDArray = ndarray_for_eigen_tensor_t<PlainTensor>;
    using NDArrayCaster = make_caster<NDArray>;

    // PlainTensor value;
    NB_TYPE_CASTER(PlainTensor, NDArrayCaster::Name);

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        using NDArrayConst = ndarray_for_eigen_tensor_t<PlainTensor, const Scalar>;
        make_caster<NDArrayConst> caster;
        // Do not accept None
        if (!caster.from_python(src, flags & ~(uint8_t)cast_flags::accepts_none, cleanup))
            return false;

        const NDArrayConst &array = caster.value;
        // copy tensor dims
        std::array<long, NumIndices> out_dims;
        for(size_t i = 0; i < NumIndices; i++) {
            out_dims[i] = array.shape(i);
        }
        value.resize(out_dims);

        memcpy(value.data(), array.data(), array.size() * sizeof(Scalar));

        return true;
    }

    static handle from_cpp(const PlainTensor &v, rv_policy policy, cleanup_list *cleanup) noexcept {
        size_t shape[NumIndices];

        for (size_t i = 0 ; i < NumIndices; i++) {
            shape[i] = static_cast<size_t>(v.dimension(i));
        }
        
        void *ptr = (void *)v.data();

        object owner;
        if (policy == rv_policy::move) {
            PlainTensor *tmp = new PlainTensor((PlainTensor&&)v);
            owner = capsule(tmp, [](void* p) noexcept {
                delete (PlainTensor*) p;
            });
            ptr = tmp->data();
            policy = rv_policy::reference;
        } else if (policy == rv_policy::reference_internal && cleanup->self()) {
            owner = borrow(cleanup->self());
            policy = rv_policy::reference;
        }
        object o = steal(
            NDArrayCaster::from_cpp(
                NDArray {ptr, NumIndices, shape, owner},
                policy, cleanup)
        );
        return o.release();
    }
};

/// TODO: implement caster for Eigen::TensorRef. 


NAMESPACE_END(detail)

NAMESPACE_END(NB_NAMESPACE)
