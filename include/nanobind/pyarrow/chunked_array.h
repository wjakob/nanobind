/*
    nanobind/pyarrow/chunked_array.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/chunked_array.h>

#include <nanobind/pyarrow/detail/caster.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::ChunkedArray> {                                         
    static constexpr auto Name = const_name("ChunkedArray");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::ChunkedArray>> : pyarrow::pyarrow_caster<arrow::ChunkedArray, arrow::py::is_chunked_array, arrow::py::wrap_chunked_array, arrow::py::unwrap_chunked_array> {};


NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)