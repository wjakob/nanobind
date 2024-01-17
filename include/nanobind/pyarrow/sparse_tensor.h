/*
    nanobind/pyarrow/sparse_tensor.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/sparse_tensor.h>
#include <nanobind/pyarrow/detail/caster.h>


NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
NAMESPACE_BEGIN(pyarrow)

template <typename T>
struct pyarrow_sparse_coo_tensor_caster : pyarrow_caster<T, arrow::py::is_sparse_coo_tensor, arrow::py::wrap_sparse_coo_tensor, arrow::py::unwrap_sparse_coo_tensor> {};

template <typename T>
struct pyarrow_sparse_csc_matrix_caster : pyarrow_caster<T, arrow::py::is_sparse_csc_matrix, arrow::py::wrap_sparse_csc_matrix, arrow::py::unwrap_sparse_csc_matrix> {};

template <typename T>
struct pyarrow_sparse_csf_tensor_caster : pyarrow_caster<T, arrow::py::is_sparse_csf_tensor, arrow::py::wrap_sparse_csf_tensor, arrow::py::unwrap_sparse_csf_tensor> {};

template <typename T>
struct pyarrow_sparse_csr_matrix_caster : pyarrow_caster<T, arrow::py::is_sparse_csr_matrix, arrow::py::wrap_sparse_csr_matrix, arrow::py::unwrap_sparse_csc_matrix> {};

NAMESPACE_END(pyarrow)

#define NB_REGISTER_PYARROW_SPARSE_TENSOR(name, caster)                             \
template<>                                                                          \
struct pyarrow::pyarrow_caster_name_trait<arrow::name> {                            \
    static constexpr auto Name = const_name(NB_STRINGIFY(name));                    \
};                                                                                  \
template<>                                                                          \
struct type_caster<std::shared_ptr<arrow::name>> : pyarrow::caster<arrow::name> {};

NB_REGISTER_PYARROW_SPARSE_TENSOR(SparseCOOTensor, pyarrow_sparse_coo_tensor_caster)
NB_REGISTER_PYARROW_SPARSE_TENSOR(SparseCSCMatrix, pyarrow_sparse_csc_matrix_caster)
NB_REGISTER_PYARROW_SPARSE_TENSOR(SparseCSFTensor, pyarrow_sparse_csf_tensor_caster)
NB_REGISTER_PYARROW_SPARSE_TENSOR(SparseCSRMatrix, pyarrow_sparse_csr_matrix_caster)

#undef NB_REGISTER_PYARROW_SPARSE_TENSOR

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)