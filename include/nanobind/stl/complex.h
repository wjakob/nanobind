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

        // TODO Faster way to get real part without string mapping? (and imag part below)
        PyObject* obj_real = PyObject_GetAttrString(obj, "real");
        // TODO If T1==float32 and obj==numpy.float32, PyFloat_AsDouble implies 2 useless conversions
        value.real(PyFloat_AsDouble(obj_real));
        PyObject* obj_imag = PyObject_GetAttrString(obj, "imag");
        value.imag(PyFloat_AsDouble(obj_imag));

        return true;
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        (void)policy;
        (void)cleanup;

        // There is no such float32 in Python, so always build as double.
        // We could build a numpy.float32, though it would force dependency to numpy.
        return PyComplex_FromDoubles(value.real(), value.imag());
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
