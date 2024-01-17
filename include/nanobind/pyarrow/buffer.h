/*
    nanobind/pyarrow/buffer.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/buffer.h>
#include <nanobind/pyarrow/detail/caster.h>


NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
NAMESPACE_BEGIN(pyarrow)

template <typename T>
struct pyarrow_buffer_caster : pyarrow_caster<T, arrow::py::is_buffer, arrow::py::wrap_buffer, arrow::py::unwrap_buffer> {};

NAMESPACE_END(pyarrow)

#define NB_REGISTER_PYARROW_BUFFER(name)                                                           \
template<>                                                                                         \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                                           \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                                   \
};                                                                                                 \
template<>                                                                                         \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::pyarrow_buffer_caster<arrow::name> {};

NB_REGISTER_PYARROW_BUFFER(Buffer)
NB_REGISTER_PYARROW_BUFFER(ResizableBuffer)
NB_REGISTER_PYARROW_BUFFER(MutableBuffer)

#undef NB_REGISTER_PYARROW_BUFFER

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)