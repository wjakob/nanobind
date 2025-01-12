/*
    nanobind/stl/complex.h: type caster for std::complex<...>

    Copyright (c) 2023 Degottex Gilles and Wenzel Jakob
    Copyright (c) 2025 High Performance Kernels LLC

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <complex>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

inline bool load_cmplx(PyObject *ob, uint8_t flags,
                       std::complex<double> *out) noexcept {
    bool is_complex = PyComplex_CheckExact(ob);
#if !defined(Py_LIMITED_API)
    if (is_complex || (flags & (uint8_t) cast_flags::convert)) {
        Py_complex result = PyComplex_AsCComplex(ob);
        if (result.real != -1.0 || !PyErr_Occurred()) {
            *out = std::complex<double>(result.real, result.imag);
            return true;
        } else {
            PyErr_Clear();
        }
    }
#else
#if Py_LIMITED_API < 0x030D0000
    // Before version 3.13, __complex__() was not called by the Stable ABI
    // functions PyComplex_{Real,Imag}AsDouble(), so we do so ourselves.
    if (!is_complex && (flags & (uint8_t) cast_flags::convert)
            && !PyType_IsSubtype(Py_TYPE(ob), &PyComplex_Type)
            && PyObject_HasAttrString(ob, "__complex__")) {
        PyObject* tmp = PyObject_CallFunctionObjArgs(
                (PyObject*) &PyComplex_Type, ob, NULL);
        if (tmp) {
            double re = PyComplex_RealAsDouble(tmp);
            double im = PyComplex_ImagAsDouble(tmp);
            Py_DECREF(tmp);
            if ((re != -1.0 && im != -1.0) || !PyErr_Occurred()) {
                *out = std::complex<double>(re, im);
                return true;
            }
        }
        PyErr_Clear();
        return false;
    }
#endif
    if (is_complex || (flags & (uint8_t) cast_flags::convert)) {
        double re = PyComplex_RealAsDouble(ob);
        double im = PyComplex_ImagAsDouble(ob);
        if ((re != -1.0 && im != -1.0) || !PyErr_Occurred()) {
            *out = std::complex<double>(re, im);
            return true;
        } else {
            PyErr_Clear();
        }
    }
#endif
    return false;
}

template <typename T> struct type_caster<std::complex<T>> {
    NB_TYPE_CASTER(std::complex<T>, const_name("complex"))

    bool from_python(handle src, uint8_t flags, cleanup_list*) noexcept {
        std::complex<double> cmplx;
        if (!load_cmplx(src.ptr(), flags, &cmplx))
            return false;
        if constexpr (std::is_same_v<T, double>) {
            value = cmplx;
            return true;
        } else {
            T re = (T) cmplx.real();
            T im = (T) cmplx.imag();
            if ((flags & (uint8_t) cast_flags::convert)
                    ||  (((double) re == cmplx.real()
                             || (re != re && cmplx.real() != cmplx.real()))
                      && ((double) im == cmplx.imag()
                             || (im != im && cmplx.imag() != cmplx.imag())))) {
                value = std::complex<T>(re, im);
                return true;
            }
            return false;
        }
    }

    static handle from_cpp(const std::complex<T>& value, rv_policy,
                           cleanup_list*) noexcept {
        return PyComplex_FromDoubles((double) value.real(),
                                     (double) value.imag());
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
