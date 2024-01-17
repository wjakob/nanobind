/*
    nanobind/pyarrow/datatype.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once


#include <nanobind/nanobind.h>
#include <memory>
#include <nanobind/pyarrow/detail/caster.h>
#include <arrow/type.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
NAMESPACE_BEGIN(pyarrow)

template <typename T>
struct pyarrow_data_type_caster : pyarrow_caster<T, arrow::py::is_data_type, arrow::py::wrap_data_type, arrow::py::unwrap_data_type> {};

NAMESPACE_END(pyarrow)

#define NB_REGISTER_PYARROW_DATATYPE(name)                                                           \
template<>                                                                                           \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                             \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                     \
};                                                                                                   \
template<>                                                                                           \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_data_type_caster<arrow::name> {};

NB_REGISTER_PYARROW_DATATYPE(DataType)
NB_REGISTER_PYARROW_DATATYPE(FixedWidthType)
NB_REGISTER_PYARROW_DATATYPE(PrimitiveCType)
NB_REGISTER_PYARROW_DATATYPE(NumberType)
NB_REGISTER_PYARROW_DATATYPE(IntervalType)
NB_REGISTER_PYARROW_DATATYPE(FloatingPointType)
NB_REGISTER_PYARROW_DATATYPE(ParametricType)
NB_REGISTER_PYARROW_DATATYPE(NestedType)
NB_REGISTER_PYARROW_DATATYPE(UnionType)

NB_REGISTER_PYARROW_DATATYPE(NullType)
NB_REGISTER_PYARROW_DATATYPE(BooleanType)
NB_REGISTER_PYARROW_DATATYPE(Int8Type)
NB_REGISTER_PYARROW_DATATYPE(Int16Type)
NB_REGISTER_PYARROW_DATATYPE(Int32Type)
NB_REGISTER_PYARROW_DATATYPE(Int64Type)
NB_REGISTER_PYARROW_DATATYPE(UInt8Type)
NB_REGISTER_PYARROW_DATATYPE(UInt16Type)
NB_REGISTER_PYARROW_DATATYPE(UInt32Type)
NB_REGISTER_PYARROW_DATATYPE(UInt64Type)
NB_REGISTER_PYARROW_DATATYPE(Time32Type)
NB_REGISTER_PYARROW_DATATYPE(Time64Type)
NB_REGISTER_PYARROW_DATATYPE(Date32Type)
NB_REGISTER_PYARROW_DATATYPE(Date64Type)
NB_REGISTER_PYARROW_DATATYPE(TimestampType)
NB_REGISTER_PYARROW_DATATYPE(DurationType)
NB_REGISTER_PYARROW_DATATYPE(MonthDayNanoIntervalType)
NB_REGISTER_PYARROW_DATATYPE(HalfFloatType)
NB_REGISTER_PYARROW_DATATYPE(FloatType)
NB_REGISTER_PYARROW_DATATYPE(DoubleType)
NB_REGISTER_PYARROW_DATATYPE(Decimal128Type)
NB_REGISTER_PYARROW_DATATYPE(Decimal256Type)
NB_REGISTER_PYARROW_DATATYPE(StringType)
NB_REGISTER_PYARROW_DATATYPE(BinaryType)
NB_REGISTER_PYARROW_DATATYPE(FixedSizeBinaryType)
NB_REGISTER_PYARROW_DATATYPE(LargeStringType)
NB_REGISTER_PYARROW_DATATYPE(LargeBinaryType)
NB_REGISTER_PYARROW_DATATYPE(ListType)
NB_REGISTER_PYARROW_DATATYPE(FixedSizeListType)
NB_REGISTER_PYARROW_DATATYPE(LargeListType)
NB_REGISTER_PYARROW_DATATYPE(MapType)
NB_REGISTER_PYARROW_DATATYPE(StructType)
NB_REGISTER_PYARROW_DATATYPE(DenseUnionType)
NB_REGISTER_PYARROW_DATATYPE(SparseUnionType)
NB_REGISTER_PYARROW_DATATYPE(DictionaryType)
NB_REGISTER_PYARROW_DATATYPE(RunEndEncodedType)

#undef NB_REGISTER_PYARROW_DATATYPE

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)