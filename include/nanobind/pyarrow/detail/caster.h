/*
    nanobind/pyarrow/detail/caster.h: conversion between arrow and pyarrow

    Copyright (c) 2024 Maximilian Kleinert <kleinert.max@gmail.com>  and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <cstddef>
#include <nanobind/nanobind.h>
#include <memory>
#include <arrow/python/pyarrow.h>


NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)
NAMESPACE_BEGIN(pyarrow)

template<typename T>
struct pyarrow_caster_name_trait;

template <typename T> 
using has_pyarrow_caster_name_trait = decltype(pyarrow_caster_name_trait<T>::Name);

template <typename T, auto& Check, auto& Wrap, auto& UnWrap>
struct pyarrow_caster {
    static_assert(is_detected_v<has_pyarrow_caster_name_trait, T>, "No Name member for NameType in pyarrow_caster");
    NB_TYPE_CASTER(std::shared_ptr<T>, const_name("pyarrow.lib.") + pyarrow_caster_name_trait<T>::Name);

    bool from_python(handle src, uint8_t /*flags*/, cleanup_list */*cleanup*/) noexcept {
      PyObject *source = src.ptr();
      if (!Check(source))
        return false;
      auto result = UnWrap(source);
      if (!result.ok())
        return false;
      value = std::dynamic_pointer_cast<T>(result.ValueOrDie());
      return static_cast<bool>(value);
    }

    static handle from_cpp(std::shared_ptr<T> arr, rv_policy /*policy*/, cleanup_list */*cleanup*/) noexcept {
        return Wrap(arr);
    }
};
NAMESPACE_END(pyarrow)
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)