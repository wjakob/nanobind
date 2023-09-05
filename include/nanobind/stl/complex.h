/*
    nanobind/stl/complex.h: type caster for std::complex<...>

    Copyright (c) 2023 Degottex Gilles

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <complex>

#include <iostream>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T1> struct type_caster<std::complex<T1>> {

    // Sub type caster
    // TODO This definition seems useless, but I can't find a neat way to replace `Caster::Name` below.
    using Caster = make_caster<T1>; 
    NB_TYPE_CASTER(std::complex<T1>, const_name("complex [") + Caster::Name + const_name("]") )

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        (void)flags;
        (void)cleanup;

        PyObject* obj = src.ptr();
        // Real and Imaginary parts of a python complex object are always double:
        //   https://docs.python.org/3/c-api/complex.html#c.Py_complex
        value.real(PyComplex_RealAsDouble(obj));
        value.imag(PyComplex_ImagAsDouble(obj));

        return true;
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {

        // Conversion from 2xfloat to 2xdouble if needs be
        PyObject* r = PyComplex_FromDoubles(value.real(), value.imag());

        return r;
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
