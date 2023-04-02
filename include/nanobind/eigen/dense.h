/*
    nanobind/eigen/dense.h: type casters for dense Eigen
    vectors and matrices

    Copyright (c) 2023 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/ndarray.h>
#include <Eigen/Core>

static_assert(EIGEN_VERSION_AT_LEAST(3, 3, 1),
              "Eigen matrix support in nanobind requires Eigen >= 3.3.1");

NAMESPACE_BEGIN(NB_NAMESPACE)

/// Types for func. arguments that are compatible with various flavors of arrays
using DStride = Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>;
template <typename T> using DRef = Eigen::Ref<T, 0, DStride>;
template <typename T> using DMap = Eigen::Map<T, 0, DStride>;

NAMESPACE_BEGIN(detail)

template <typename T>
constexpr int NumDimensions = bool(T::IsVectorAtCompileTime) ? 1 : 2;

template <typename T>
using array_for_eigen_t = ndarray<
    typename T::Scalar,
    numpy,
    std::conditional_t<
        NumDimensions<T> == 1,
        shape<(size_t) T::SizeAtCompileTime>,
        shape<(size_t) T::RowsAtCompileTime,
              (size_t) T::ColsAtCompileTime>>,
    std::conditional_t<
        T::InnerStrideAtCompileTime == Eigen::Dynamic,
        any_contig,
        std::conditional_t<
            T::IsRowMajor || NumDimensions<T> == 1,
            c_contig,
            f_contig
        >
    >
>;

/// Any kind of Eigen class
template <typename T> constexpr bool is_eigen_v =
is_base_of_template_v<T, Eigen::EigenBase>;

/// Detects Eigen::Array, Eigen::Matrix, etc.
template <typename T> constexpr bool is_eigen_plain_v =
is_base_of_template_v<T, Eigen::PlainObjectBase>;

/// Detect Eigen::SparseMatrix
template <typename T> constexpr bool is_eigen_sparse_v =
is_base_of_template_v<T, Eigen::SparseMatrixBase>;

/// Detects expression templates
template <typename T> constexpr bool is_eigen_xpr_v =
    is_eigen_v<T> && !is_eigen_plain_v<T> && !is_eigen_sparse_v<T> &&
    !std::is_base_of_v<Eigen::MapBase<T, Eigen::ReadOnlyAccessors>, T>;

template <typename T> struct type_caster<T, enable_if_t<is_eigen_plain_v<T>>> {
    using Scalar = typename T::Scalar;
    using NDArray = array_for_eigen_t<T>;
    using NDArrayCaster = make_caster<NDArray>;

    NB_TYPE_CASTER(T, NDArrayCaster::Name);

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        NDArrayCaster caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;
        const NDArray &array = caster.value;

        if constexpr (NumDimensions<T> == 1) {
            value.resize(array.shape(0));
            memcpy(value.data(), array.data(),
                   array.shape(0) * sizeof(Scalar));
        } else {
            value.resize(array.shape(0), array.shape(1));
            memcpy(value.data(), array.data(),
                   array.shape(0) * array.shape(1) * sizeof(Scalar));
        }

        return true;
    }

    static handle from_cpp(T &&v, rv_policy policy, cleanup_list *cleanup) noexcept {
        if (policy == rv_policy::automatic ||
            policy == rv_policy::automatic_reference)
            policy = rv_policy::move;

        return from_cpp((const T &) v, policy, cleanup);
    }

    static handle from_cpp(const T &v, rv_policy policy, cleanup_list *cleanup) noexcept {
        size_t shape[NumDimensions<T>];
        int64_t strides[NumDimensions<T>];

        if constexpr (NumDimensions<T> == 1) {
            shape[0] = v.size();
            strides[0] = v.innerStride();
        } else {
            shape[0] = v.rows();
            shape[1] = v.cols();
            strides[0] = v.rowStride();
            strides[1] = v.colStride();
        }

        void *ptr = (void *) v.data();

        switch (policy) {
            case rv_policy::automatic:
                policy = rv_policy::copy;
                break;

            case rv_policy::automatic_reference:
                policy = rv_policy::reference;
                break;

            case rv_policy::move:
                // Don't bother moving when the data is static or occupies <1KB
                if ((T::SizeAtCompileTime != Eigen::Dynamic ||
                     (size_t) v.size() < (1024 / sizeof(Scalar))))
                    policy = rv_policy::copy;
                break;

            default: // leave policy unchanged
                break;
        }

        object owner;
        if (policy == rv_policy::move) {
            T *temp = new T(std::move(v));
            owner = capsule(temp, [](void *p) noexcept { delete (T *) p; });
            ptr = temp->data();
        }

        rv_policy array_rv_policy =
            policy == rv_policy::move ? rv_policy::reference : policy;

        object o = steal(NDArrayCaster::from_cpp(
            NDArray(ptr, NumDimensions<T>, shape, owner, strides),
            array_rv_policy, cleanup));

        return o.release();
    }
};

/// Caster for Eigen expression templates
template <typename T> struct type_caster<T, enable_if_t<is_eigen_xpr_v<T>>> {
    using Array = Eigen::Array<typename T::Scalar, T::RowsAtCompileTime,
                               T::ColsAtCompileTime>;
    using Caster = make_caster<Array>;
    static constexpr auto Name = Caster::Name;
    template <typename T_> using Cast = T;

    /// Generating an expression template from a Python object is, of course, not possible
    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept = delete;

    template <typename T2>
    static handle from_cpp(T2 &&v, rv_policy policy, cleanup_list *cleanup) noexcept {
        return Caster::from_cpp(std::forward<T2>(v), policy, cleanup);
    }
};

/// Caster for Eigen::Map<T>
template <typename T, int Options, typename StrideType>
struct type_caster<Eigen::Map<T, Options, StrideType>, enable_if_t<is_eigen_plain_v<T>>> {
    using Map = Eigen::Map<T, Options, StrideType>;
    using NDArray = array_for_eigen_t<Map>;
    using NDArrayCaster = type_caster<NDArray>;
    static constexpr auto Name = NDArrayCaster::Name;
    template <typename T_> using Cast = Map;

    NDArrayCaster caster;

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        return caster.from_python(src, flags, cleanup);
    }

    static handle from_cpp(const Map &v, rv_policy, cleanup_list *cleanup) noexcept {
        size_t shape[NumDimensions<T>];
        int64_t strides[NumDimensions<T>];

        if constexpr (NumDimensions<T> == 1) {
            shape[0] = v.size();
            strides[0] = v.innerStride();
        } else {
            shape[0] = v.rows();
            shape[1] = v.cols();
            strides[0] = v.rowStride();
            strides[1] = v.colStride();
        }

        return NDArrayCaster::from_cpp(
            NDArray((void *) v.data(), NumDimensions<T>, shape, handle(), strides),
            rv_policy::reference, cleanup);
    }

    StrideType strides() const {
        constexpr int IS = StrideType::InnerStrideAtCompileTime,
                      OS = StrideType::OuterStrideAtCompileTime;

        int64_t inner = caster.value.stride(0),
                outer = caster.value.stride(1);

        if constexpr (T::IsRowMajor)
            std::swap(inner, outer);

        if constexpr (std::is_same_v<StrideType, Eigen::InnerStride<IS>>)
            return StrideType(inner);
        else if constexpr (std::is_same_v<StrideType, Eigen::OuterStride<OS>>)
            return StrideType(outer);
        else
            return StrideType(outer, inner);
    }

    operator Map() {
        NDArray &t = caster.value;
        return Map(t.data(), t.shape(0), t.shape(1), strides());
    }
};

/// Caster for Eigen::Ref<T>
template <typename T, int Options, typename StrideType>
struct type_caster<Eigen::Ref<T, Options, StrideType>, enable_if_t<is_eigen_plain_v<T>>> {
    using Ref = Eigen::Ref<T, Options, StrideType>;
    using Map = Eigen::Map<T, Options, StrideType>;
    using MapCaster = make_caster<Map>;
    static constexpr auto Name = MapCaster::Name;
    template <typename T_> using Cast = Ref;

    MapCaster caster;

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        return caster.from_python(src, flags, cleanup);
    }

    operator Ref() { return Ref(caster.operator Map()); }
};

NAMESPACE_END(detail)

NAMESPACE_END(NB_NAMESPACE)
