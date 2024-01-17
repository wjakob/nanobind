/*
    nanobind/pyarrow/detail/array_caster.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/array/array_binary.h>
#include <nanobind/pyarrow/detail/caster.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

NAMESPACE_BEGIN(pyarrow)

template <typename T>
struct pyarrow_array_caster : pyarrow_caster<T, arrow::py::is_array, arrow::py::wrap_array, arrow::py::unwrap_array> {};

NAMESPACE_END(pyarrow)
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)