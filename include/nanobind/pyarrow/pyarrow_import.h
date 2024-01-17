/*
    nanobind/pyarrow/pyarrow_import.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <arrow/python/pyarrow.h>
#include <stdexcept>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

NAMESPACE_BEGIN(pyarrow)

class ImportPyarrow {
public:
  inline ImportPyarrow() {
    if (arrow::py::import_pyarrow() != 0) {
      nanobind::python_error error;
      throw std::runtime_error(error.what());
    }
  }
};

NAMESPACE_END(pyarrow)
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)