/*
    nanobind/pyarrow/tabular.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <nanobind/pyarrow/detail/caster.h>
#include <arrow/record_batch.h>
#include <arrow/table.h>
#include <arrow/type.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::Table> {                                         
    static constexpr auto Name = const_name("Table");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::Table>> : pyarrow::pyarrow_caster<arrow::Table, arrow::py::is_table, arrow::py::wrap_table, arrow::py::unwrap_table> {};

template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::RecordBatch> {                                         
    static constexpr auto Name = const_name("RecordBatch");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::RecordBatch>> : pyarrow::pyarrow_caster<arrow::RecordBatch, arrow::py::is_batch, arrow::py::wrap_batch, arrow::py::unwrap_batch> {};


template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::Schema> {                                         
    static constexpr auto Name = const_name("Schema");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::Schema>> : pyarrow::pyarrow_caster<arrow::Schema, arrow::py::is_schema, arrow::py::wrap_schema, arrow::py::unwrap_schema> {};

template<>                                                                                       
struct pyarrow::pyarrow_caster_name_trait<arrow::Field> {                                         
    static constexpr auto Name = const_name("Field");                                 
};                                                                                               
template<>                                                                                       
struct type_caster<std::shared_ptr<arrow::Field>> : pyarrow::pyarrow_caster<arrow::Field, arrow::py::is_field, arrow::py::wrap_field, arrow::py::unwrap_field> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)