/*
    nanobind/pyarrow/array_binary.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/array/array_binary.h>
#include <nanobind/pyarrow/detail/array_caster.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#define NB_REGISTER_PYARROW_BINARY_ARRAY(name)                                                   \
template<>                                                                                       \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                         \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                 \
} ;                                                                                              \
template<>                                                                                       \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_array_caster<arrow::name> {};

NB_REGISTER_PYARROW_BINARY_ARRAY(BinaryArray)
NB_REGISTER_PYARROW_BINARY_ARRAY(LargeBinaryArray)
NB_REGISTER_PYARROW_BINARY_ARRAY(StringArray)
NB_REGISTER_PYARROW_BINARY_ARRAY(LargeStringArray)
NB_REGISTER_PYARROW_BINARY_ARRAY(FixedSizeBinaryArray)
#undef NB_REGISTER_PYARROW_BINARY_ARRAY


NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)