/*
    nanobind/pyarrow/table.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <nanobind/pyarrow/detail/caster.h>
#include <arrow/table.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::Table> {                                         
    static constexpr auto Name = const_name("Table");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::Table>> : pyarrow::pyarrow_caster<arrow::Table, arrow::py::is_table, arrow::py::wrap_table, arrow::py::unwrap_table> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)