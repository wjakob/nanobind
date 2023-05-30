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
constexpr int num_dimensions = bool(T::IsVectorAtCompileTime) ? 1 : 2;

template<typename T> struct StrideExtr {
    using Type = Eigen::Stride<0, 0>;
};

template <typename T, int Options, typename StrideType> struct StrideExtr<Eigen::Map<T, Options, StrideType>> {
    using Type = StrideType;
};

template<typename T> using Stride = typename StrideExtr<T>::Type;

/// Is true for Eigen types that are known at compile-time to hold contiguous memory only, which includes all specializations of Matrix and Array,
/// and specializations of Map and Ref with according stride types and shapes. A (compile-time) stride of 0 means "contiguous" to Eigen.
template<typename T> constexpr bool requires_contig_memory =
    (Stride<T>::InnerStrideAtCompileTime == 0 || Stride<T>::InnerStrideAtCompileTime == 1) &&
    (num_dimensions<T> == 1 ||
     Stride<T>::OuterStrideAtCompileTime == 0 ||
     Stride<T>::OuterStrideAtCompileTime != Eigen::Dynamic && Stride<T>::OuterStrideAtCompileTime == T::InnerSizeAtCompileTime);

/// Alias ndarray for a given Eigen type, to be used by type_caster<EigenType>::from_python, which calls type_caster<array_for_eigen_t<EigenType>>::from_python.
/// If the Eigen type is known at compile-time to handle contiguous memory only, then this alias makes type_caster<array_for_eigen_t<EigenType>>::from_python
/// either fail or provide an ndarray with contiguous memory, triggering a conversion if necessary and supported by flags.
/// Otherwise, this alias makes type_caster<array_for_eigen_t<EigenType>>::from_python either fail or provide an ndarray with arbitrary strides,
/// which need to be checked for compatibility then. There is no way to ask type_caster<ndarray> for specific strides other than c_contig and f_contig.
/// Hence, if an Eigen type requires non-contiguous strides (at compile-time) and type_caster<array_for_eigen_t<EigenType>> provides an ndarray with unsuitable strides (at run-time),
/// then type_caster<EigenType>::from_python just fails. Note, however, that this is rather unusual, since the default stride type of Map requires contiguous memory,
/// and the one of Ref requires a contiguous inner stride, while it can handle any outer stride.
template <typename T> using array_for_eigen_t = ndarray<
    typename T::Scalar,
    numpy,
    std::conditional_t<
        num_dimensions<T> == 1,
        shape<(size_t) T::SizeAtCompileTime>,
        shape<(size_t) T::RowsAtCompileTime,
              (size_t) T::ColsAtCompileTime>>,
    std::conditional_t<
        requires_contig_memory<T>,
        std::conditional_t<
            num_dimensions<T> == 1 || T::IsRowMajor,
            c_contig,
            f_contig>,
        any_contig>>;

/// Any kind of Eigen class
template <typename T> constexpr bool is_eigen_v = is_base_of_template_v<T, Eigen::EigenBase>;

/// Detects Eigen::Array, Eigen::Matrix, etc.
template <typename T> constexpr bool is_eigen_plain_v = is_base_of_template_v<T, Eigen::PlainObjectBase>;

/// Detect Eigen::SparseMatrix
template <typename T> constexpr bool is_eigen_sparse_v = is_base_of_template_v<T, Eigen::SparseMatrixBase>;

/// Detects expression templates
template <typename T> constexpr bool is_eigen_xpr_v =
    is_eigen_v<T> && !is_eigen_plain_v<T> && !is_eigen_sparse_v<T> &&
    !std::is_base_of_v<Eigen::MapBase<T, Eigen::ReadOnlyAccessors>, T>;

template <typename T> struct type_caster<T, enable_if_t<is_eigen_plain_v<T> && is_ndarray_scalar_v<typename T::Scalar>>> {
    using Scalar = typename T::Scalar;
    using NDArray = array_for_eigen_t<T>;
    using NDArrayCaster = make_caster<NDArray>;

    NB_TYPE_CASTER(T, NDArrayCaster::Name);

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        NDArrayCaster caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;
        const NDArray &array = caster.value;
        if constexpr (num_dimensions<T> == 1)
            value.resize(array.shape(0));
        else
            value.resize(array.shape(0), array.shape(1));
        // array_for_eigen_t<T> ensures that array holds contiguous memory.
        memcpy(value.data(), array.data(), array.size() * sizeof(Scalar));
        return true;
    }

    static handle from_cpp(T &&v, rv_policy policy, cleanup_list *cleanup) noexcept {
        if (policy == rv_policy::automatic ||
            policy == rv_policy::automatic_reference)
            policy = rv_policy::move;

        return from_cpp((const T &) v, policy, cleanup);
    }

    static handle from_cpp(const T &v, rv_policy policy, cleanup_list *cleanup) noexcept {
        size_t shape[num_dimensions<T>];
        int64_t strides[num_dimensions<T>];

        if constexpr (num_dimensions<T> == 1) {
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
        } else if (policy == rv_policy::reference_internal) {
            owner = borrow(cleanup->self());
        }

        rv_policy array_rv_policy =
            policy == rv_policy::move ? rv_policy::reference : policy;

        object o = steal(NDArrayCaster::from_cpp(
            NDArray(ptr, num_dimensions<T>, shape, owner, strides),
            array_rv_policy, cleanup));

        return o.release();
    }
};

/// Caster for Eigen expression templates
template <typename T> struct type_caster<T, enable_if_t<is_eigen_xpr_v<T> && is_ndarray_scalar_v<typename T::Scalar>>> {
    using Array = Eigen::Array<typename T::Scalar, T::RowsAtCompileTime,
                               T::ColsAtCompileTime>;
    using Caster = make_caster<Array>;
    static constexpr bool IsClass = false;
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
struct type_caster<Eigen::Map<T, Options, StrideType>, enable_if_t<is_eigen_plain_v<T> && is_ndarray_scalar_v<typename T::Scalar>>> {
    using Map = Eigen::Map<T, Options, StrideType>;
    using NDArray = array_for_eigen_t<Map>;
    using NDArrayCaster = type_caster<NDArray>;
    static constexpr bool IsClass = false;
    static constexpr auto Name = NDArrayCaster::Name;
    template <typename T_> using Cast = Map;

    NDArrayCaster caster;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        // Conversions result in an Eigen::Map pointing into a temporary ndarray.
        // If src is not a bound function argument, but e.g. an argument of cast, then this temporary would be destroyed upon returning from cast.
        // Hence, conversions cannot be supported in this case.
        // If src is a bound function argument, then cleanup would keep alive this temporary until returning from the bound function.
        // Hence, conversions could be supported in this case, resulting in a bound function altering the Map without an effect on the Python side.
        // This behaviour would be surprising, however, as bound functions expecting a Map most probably expect that Map to point into the caller's data.
        // Hence, do not support conversions in any case.
        return from_python_(src, flags & ~(uint8_t)cast_flags::convert, cleanup);
    }

    bool from_python_(handle src, uint8_t flags, cleanup_list* cleanup) noexcept {
        if (!caster.from_python(src, flags, cleanup))
            return false;

        // Check if StrideType can cope with the strides of caster.value. Avoid this check if their types guarantee that, anyway.

        // If requires_contig_memory<Map> is true, then StrideType is known at compile-time to only cope with contiguous memory.
        // Then since caster.from_python has succeeded, caster.value now surely provides contiguous memory, and so its strides surely fit.
        if constexpr (!requires_contig_memory<Map>)  {
            // A stride that is dynamic at compile-time copes with any stride at run-time. 
            if constexpr (StrideType::InnerStrideAtCompileTime != Eigen::Dynamic) {
                // A stride of 0 at compile-time means "contiguous" to Eigen, which is always 1 for the inner stride.
                int64_t expected_inner_stride = StrideType::InnerStrideAtCompileTime == 0 ? 1 : StrideType::InnerStrideAtCompileTime;
                if (expected_inner_stride != (num_dimensions<T> == 1 || !T::IsRowMajor ? caster.value.stride(0) : caster.value.stride(1)))
                    return false;
            }
            if constexpr (num_dimensions<T> == 2 && StrideType::OuterStrideAtCompileTime != Eigen::Dynamic) {
                int64_t expected_outer_stride =
                    StrideType::OuterStrideAtCompileTime == 0
                    ? T::IsRowMajor ? caster.value.shape(1) : caster.value.shape(0)
                    : StrideType::OuterStrideAtCompileTime;
                if (expected_outer_stride != (T::IsRowMajor ? caster.value.stride(0) : caster.value.stride(1)))
                    return false;
            }
        }
        return true;
    }

    static handle from_cpp(const Map &v, rv_policy, cleanup_list *cleanup) noexcept {
        size_t shape[num_dimensions<T>];
        int64_t strides[num_dimensions<T>];

        if constexpr (num_dimensions<T> == 1) {
            shape[0] = v.size();
            strides[0] = v.innerStride();
        } else {
            shape[0] = v.rows();
            shape[1] = v.cols();
            strides[0] = v.rowStride();
            strides[1] = v.colStride();
        }

        return NDArrayCaster::from_cpp(
            NDArray((void *) v.data(), num_dimensions<T>, shape, handle(), strides),
            rv_policy::reference, cleanup);
    }

    StrideType strides() const {
        constexpr int is = StrideType::InnerStrideAtCompileTime,
                      os = StrideType::OuterStrideAtCompileTime;

        int64_t inner = caster.value.stride(0),
                outer;
        if constexpr (num_dimensions<T> == 1)
            outer = caster.value.shape(0);
        else
            outer = caster.value.stride(1);

        if constexpr (num_dimensions<T> == 2 && T::IsRowMajor)
            std::swap(inner, outer);

        // Compile-time strides of 0 must be passed as such to constructors of StrideType, to avoid assertions in Eigen.
        if constexpr (is == 0) {
            // Ensured by stride checks in from_python_:
            // assert(inner == 1);
            inner = 0;
        }

        if constexpr (os == 0) {
            // Ensured by stride checks in from_python_:
            // assert(num_dimensions<T> == 1 || outer == (T::IsRowMajor ? int64_t(caster.value.shape(1)) : int64_t(caster.value.shape(0))));
            outer = 0;
        }

        if constexpr (std::is_same_v<StrideType, Eigen::InnerStride<is>>)
            return StrideType(inner);
        else if constexpr (std::is_same_v<StrideType, Eigen::OuterStride<os>>)
            return StrideType(outer);
        else
            return StrideType(outer, inner);
    }

    operator Map() {
        NDArray &t = caster.value;
        if constexpr (num_dimensions<T> == 1)
            return Map(t.data(), t.shape(0), strides());
        return Map(t.data(), t.shape(0), t.shape(1), strides());
    }
};


/// Caster for Eigen::Ref<T>
template <typename T, int Options, typename StrideType>
struct type_caster<Eigen::Ref<T, Options, StrideType>, enable_if_t<is_eigen_plain_v<T> && is_ndarray_scalar_v<typename T::Scalar>>> {
    using Ref = Eigen::Ref<T, Options, StrideType>;
    using Map = Eigen::Map<T, Options, StrideType>;
    using DMap = Eigen::Map<T, Options, DStride>;
    using MapCaster = make_caster<Map>;
    using DMapCaster = make_caster<DMap>;
    static constexpr bool IsClass = false;
    static constexpr auto Name = MapCaster::Name;
    template <typename T_> using Cast = Ref;

    MapCaster caster;
    DMapCaster dcaster;

    /// Both Ref<T> and Ref<T const> map data. type_caster<Ref<T>>::from_python behaves like type_caster<Map<T>>::from_python.
    /// Unlike Ref<T>, Ref<T const> may own the data it maps. It does so if constructed from e.g. an Eigen type that has non-matching strides.
    /// Hence, type_caster<Ref<T const>>::from_python may support conversions.
    /// It first calls the type_caster for matching strides, which does not support conversions,
    /// and only if that fails, it calls the one for arbitrary strides, supporting conversions to T::Scalar if flags say so.
    /// If the first type_caster succeeds, then the returned Ref maps the original data.
    /// Otherwise, because the first type_caster failed, the Ref is constructed such that it owns the data it maps.
    /// type_caster<Ref<T const>> always supports stride conversions, independent of flags, and so flags control the conversion of T::Scalar only.
    /// Reason: if the intention was to not allow stride conversions either, then the bound function would most probably expect a Map instead of a Ref.
    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        if constexpr (std::is_const_v<T>)
            return caster.from_python(src, flags, cleanup) ||
                   dcaster.from_python_(src, flags, cleanup);
        return caster.from_python(src, flags, cleanup);
    }

    operator Ref() {
        if constexpr (std::is_const_v<T>)
            if (dcaster.caster.value.is_valid()) {
                // Return a Ref<T const, ...> that owns the data it maps.
                // assert(!Eigen::internal::traits<Ref>::template match<DMap>::type::value);
                return Ref(dcaster.operator DMap());
            }
        return Ref(caster.operator Map());
    }
};

NAMESPACE_END(detail)

NAMESPACE_END(NB_NAMESPACE)
