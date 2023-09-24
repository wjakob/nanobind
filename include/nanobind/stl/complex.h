/*
    nanobind/stl/complex.h: type caster for std::complex<...>

    Copyright (c) 2023 Degottex Gilles

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <complex>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T1> struct type_caster<std::complex<T1>> {

    NB_TYPE_CASTER(std::complex<T1>, const_name("complex") )

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        (void)flags;
        (void)cleanup;

        PyObject* obj = src.ptr();
        value.real(PyComplex_RealAsDouble(obj));
        value.imag(PyComplex_ImagAsDouble(obj));

        return true;
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        (void)policy;
        (void)cleanup;

        // Conversion from 2xfloat to 2xdouble if needs be
        return PyComplex_FromDoubles(value.real(), value.imag());
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
