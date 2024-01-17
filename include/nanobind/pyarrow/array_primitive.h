/*
    nanobind/pyarrow/array_primitive.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/array/array_primitive.h>
#include <nanobind/pyarrow/detail/array_caster.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#define NB_REGISTER_PYARROW_ARRAY(name)                                                          \
template<>                                                                                       \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                         \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                 \
};                                                                                               \
template<>                                                                                       \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_array_caster<arrow::name> {};

// array_base classes
NB_REGISTER_PYARROW_ARRAY(Array)
NB_REGISTER_PYARROW_ARRAY(FlatArray)
NB_REGISTER_PYARROW_ARRAY(PrimitiveArray)
NB_REGISTER_PYARROW_ARRAY(NullArray)

// array_primitive classes
NB_REGISTER_PYARROW_ARRAY(BooleanArray)
NB_REGISTER_PYARROW_ARRAY(DayTimeIntervalArray)
NB_REGISTER_PYARROW_ARRAY(MonthDayNanoIntervalArray)

// numeric arrays
NB_REGISTER_PYARROW_ARRAY(HalfFloatArray)
NB_REGISTER_PYARROW_ARRAY(FloatArray)
NB_REGISTER_PYARROW_ARRAY(DoubleArray)

NB_REGISTER_PYARROW_ARRAY(Int8Array)
NB_REGISTER_PYARROW_ARRAY(Int16Array)
NB_REGISTER_PYARROW_ARRAY(Int32Array)
NB_REGISTER_PYARROW_ARRAY(Int64Array)

NB_REGISTER_PYARROW_ARRAY(UInt8Array)
NB_REGISTER_PYARROW_ARRAY(UInt16Array)
NB_REGISTER_PYARROW_ARRAY(UInt32Array)
NB_REGISTER_PYARROW_ARRAY(UInt64Array)

NB_REGISTER_PYARROW_ARRAY(Decimal128Array)
NB_REGISTER_PYARROW_ARRAY(Decimal256Array)

NB_REGISTER_PYARROW_ARRAY(Date32Array)
NB_REGISTER_PYARROW_ARRAY(Date64Array)

NB_REGISTER_PYARROW_ARRAY(Time32Array)
NB_REGISTER_PYARROW_ARRAY(Time64Array)
NB_REGISTER_PYARROW_ARRAY(MonthIntervalArray)
NB_REGISTER_PYARROW_ARRAY(DurationArray)

// extension array
NB_REGISTER_PYARROW_ARRAY(ExtensionArray)
// run end encoded array
NB_REGISTER_PYARROW_ARRAY(RunEndEncodedArray)

#undef NB_REGISTER_PYARROW_ARRAY

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)