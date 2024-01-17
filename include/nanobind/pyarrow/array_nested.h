/*
    nanobind/pyarrow/array_nested.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <nanobind/pyarrow/detail/array_caster.h>

#include <arrow/array/array_nested.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#define NB_REGISTER_PYARROW_NESTED_ARRAY(name)                                                   \
template<>                                                                                       \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                         \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                 \
};                                                                                               \
template<>                                                                                       \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_array_caster<arrow::name> {};

// array_nested classes
NB_REGISTER_PYARROW_NESTED_ARRAY(ListArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(LargeListArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(MapArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(FixedSizeListArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(StructArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(UnionArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(SparseUnionArray)
NB_REGISTER_PYARROW_NESTED_ARRAY(DenseUnionArray)

#undef NB_REGISTER_PYARROW_NESTED_ARRAY

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)