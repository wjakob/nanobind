/*
    nanobind/pyarrow/scalar.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once


#include <nanobind/nanobind.h>
#include <memory>
#include <nanobind/pyarrow/detail/caster.h>
#include <arrow/scalar.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
NAMESPACE_BEGIN(pyarrow)

template <typename T>
struct pyarrow_scalar_caster : pyarrow_caster<T, arrow::py::is_scalar, arrow::py::wrap_scalar, arrow::py::unwrap_scalar> {};

NAMESPACE_END(pyarrow)

#define NB_REGISTER_PYARROW_SCALAR(name)                                                           \
template<>                                                                                         \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                           \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                   \
};                                                                                                 \
template<>                                                                                         \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_scalar_caster<arrow::name> {};

// See https://arrow.apache.org/docs/cpp/api/scalar.html
NB_REGISTER_PYARROW_SCALAR(Scalar)
NB_REGISTER_PYARROW_SCALAR(NullScalar)
NB_REGISTER_PYARROW_SCALAR(BooleanScalar)
NB_REGISTER_PYARROW_SCALAR(Int8Scalar)
NB_REGISTER_PYARROW_SCALAR(Int16Scalar)
NB_REGISTER_PYARROW_SCALAR(Int32Scalar)
NB_REGISTER_PYARROW_SCALAR(Int64Scalar)
NB_REGISTER_PYARROW_SCALAR(UInt8Scalar)
NB_REGISTER_PYARROW_SCALAR(UInt16Scalar)
NB_REGISTER_PYARROW_SCALAR(UInt32Scalar)
NB_REGISTER_PYARROW_SCALAR(UInt64Scalar)
NB_REGISTER_PYARROW_SCALAR(HalfFloatScalar)
NB_REGISTER_PYARROW_SCALAR(FloatScalar)
NB_REGISTER_PYARROW_SCALAR(DoubleScalar)
NB_REGISTER_PYARROW_SCALAR(BaseBinaryScalar)
NB_REGISTER_PYARROW_SCALAR(BinaryScalar)
NB_REGISTER_PYARROW_SCALAR(StringScalar)
NB_REGISTER_PYARROW_SCALAR(LargeBinaryScalar)
NB_REGISTER_PYARROW_SCALAR(LargeStringScalar)
NB_REGISTER_PYARROW_SCALAR(FixedSizeBinaryScalar)
NB_REGISTER_PYARROW_SCALAR(Date32Scalar)
NB_REGISTER_PYARROW_SCALAR(Date64Scalar)
NB_REGISTER_PYARROW_SCALAR(Time32Scalar)
NB_REGISTER_PYARROW_SCALAR(Time64Scalar)
NB_REGISTER_PYARROW_SCALAR(TimestampScalar)
NB_REGISTER_PYARROW_SCALAR(MonthIntervalScalar)
NB_REGISTER_PYARROW_SCALAR(DayTimeIntervalScalar)
NB_REGISTER_PYARROW_SCALAR(DurationScalar)
NB_REGISTER_PYARROW_SCALAR(MonthDayNanoIntervalScalar)
NB_REGISTER_PYARROW_SCALAR(Decimal128Scalar)
NB_REGISTER_PYARROW_SCALAR(Decimal256Scalar)
NB_REGISTER_PYARROW_SCALAR(BaseListScalar)
NB_REGISTER_PYARROW_SCALAR(ListScalar)
NB_REGISTER_PYARROW_SCALAR(LargeListScalar)
NB_REGISTER_PYARROW_SCALAR(MapScalar)
NB_REGISTER_PYARROW_SCALAR(FixedSizeListScalar)
NB_REGISTER_PYARROW_SCALAR(StructScalar)
NB_REGISTER_PYARROW_SCALAR(UnionScalar)
NB_REGISTER_PYARROW_SCALAR(SparseUnionScalar)
NB_REGISTER_PYARROW_SCALAR(DenseUnionScalar)
NB_REGISTER_PYARROW_SCALAR(RunEndEncodedScalar)
NB_REGISTER_PYARROW_SCALAR(DictionaryScalar)
NB_REGISTER_PYARROW_SCALAR(ExtensionScalar)

#undef NB_REGISTER_PYARROW_SCALAR

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)