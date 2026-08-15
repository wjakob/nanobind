/*
    src/common.cpp: miscellaneous libnanobind functionality

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include "nb_internals.h"

#if defined(_MSC_VER)
#  pragma warning(disable: 6255) // _alloca indicates failure by raising a stack overflow exception
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

NB_NOINLINE static builtin_exception
create_exception(exception_type type, const char *fmt, va_list args_) {
    char buf[512];
    va_list args;

    va_copy(args, args_);
    size_t size = (size_t) vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (size < sizeof(buf)) {
        return builtin_exception(type, buf);
    } else {
        scoped_pymalloc<char> temp(size + 1);

        va_copy(args, args_);
        vsnprintf(temp.get(), size + 1, fmt, args);
        va_end(args);

        return builtin_exception(type, temp.get());
    }
}

void raise_v(exception_type type, const char *fmt, va_list args) {
    throw create_exception(type, fmt, args);
}

void fail(const char *fmt, ...) noexcept {
    va_list args;
    fprintf(stderr, "Critical nanobind error: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

// ========================================================================

PyObject *submodule_new(PyObject *base, const char *name,
                               const char *doc) noexcept {
    const char *base_name, *tmp_str;
    Py_ssize_t tmp_size = 0;
    object tmp, res;

    base_name = PyModule_GetName(base);
    if (!base_name)
        goto fail;

    tmp = steal(PyUnicode_FromFormat("%s.%s", base_name, name));
    if (!tmp.is_valid())
        goto fail;

    tmp_str = PyUnicode_AsUTF8AndSize(tmp.ptr(), &tmp_size);
    if (!tmp_str)
        goto fail;

#if NB_PYTHON_VERSION < 0x030D0000
    res = borrow(PyImport_AddModule(tmp_str));
#else
    res = steal(PyImport_AddModuleRef(tmp_str));
#endif

    if (!res.is_valid())
        goto fail;

    if (doc) {
        tmp = steal(PyUnicode_FromString(doc));
        if (!tmp.is_valid())
            goto fail;
        if (PyObject_SetAttrString(res.ptr(), "__doc__", tmp.ptr()))
            goto fail;
    }

    if (PyModule_AddObjectRef(base, name, res.ptr()))
        goto fail;

    return res.release().ptr();

fail:
    raise_python_error();
}

// ========================================================================

size_t len_hint(PyObject *o) noexcept {
#if !defined(Py_LIMITED_API)
    Py_ssize_t res = PyObject_LengthHint(o, 0);
    if (res < 0) {
        PyErr_Clear();
        res = 0;
    }
    return (size_t) res;
#else
    PyTypeObject *tp = Py_TYPE(o);
    lenfunc l = (lenfunc) type_get_slot(tp, Py_sq_length);
    if (!l)
        l = (lenfunc) type_get_slot(tp, Py_mp_length);

    if (l) {
        Py_ssize_t res = l(o);
        if (res < 0) {
            PyErr_Clear();
            res = 0;
        }
        return (size_t) res;
    }

    try {
        return cast<size_t>(handle(o).attr(NB_INTERNED(__length_hint__))());
    } catch (...) {
        return 0;
    }
#endif
}

PyObject *obj_vectorcall(PyObject *base, PyObject *const *args, size_t nargsf,
                         uint64_t owned, uint32_t flags) {
    size_t nargs = (size_t) PyVectorcall_NARGS(nargsf);
    PyObject *res = nullptr;
    bool cast_error = !base;

    for (size_t i = 0; i < nargs; ++i)
        cast_error |= !args[i];

    if (!cast_error)
        res = (flags & (uint32_t) call_flags::method
                   ? PyObject_VectorcallMethod
                   : PyObject_Vectorcall)(base, args, nargsf, nullptr);

    // Calls that pass every argument as a borrowed pointer skip this pass
    if (owned || nargs > 64) {
        for (size_t i = 0; i < nargs; ++i) {
            if (i >= 64 || (owned & ((uint64_t) 1 << i)))
                Py_XDECREF(args[i]);
        }
    }

    if (flags & (uint32_t) call_flags::base_owned)
        Py_XDECREF(base);

    if (!res) {
        if (cast_error)
            raise_python_or_cast_error();
        else
            raise_python_error();
    }

    return res;
}

/// Turn the operand of a '**' expansion into a private dict, with the error
/// message that CPython uses for the equivalent Python syntax
static PyObject *kwargs_dict(PyObject *value) {
    PyObject *dict = PyDict_New();
    if (dict && PyDict_Update(dict, value) < 0) {
        Py_CLEAR(dict);
        if (PyErr_ExceptionMatches(PyExc_AttributeError))
            PyErr_SetString(PyExc_TypeError,
                            "argument after ** must be a mapping");
    }
    return dict;
}

PyObject *obj_vectorcall_ex(PyObject *base, call_arg *args, size_t n,
                            uint32_t flags) {
    PyObject *res = nullptr, *kwnames = nullptr, **stack, **pos, **kw;
    size_t nargs = 0, nkwargs = 0, nkw = 0;
    bool cast_error = !base;

    /* Pass 1: turn expansion operands into tuples and private dicts (as CPython
       does before CALL_FUNCTION_EX) and count. Only the conversions can run
       Python code, and they cannot change the size of the entries before them. */
    for (size_t i = 0; i < n && !cast_error; ++i) {
        call_arg &a = args[i];
        bool star = a.kind == call_arg_kind::args;

        if (!a.value || (a.kind == call_arg_kind::keyword && !a.name)) {
            cast_error = true;
        } else if (a.kind == call_arg_kind::positional) {
            nargs++;
        } else if (a.kind == call_arg_kind::keyword) {
            nkwargs++;
        } else {
            PyObject *tmp = star ? PySequence_Tuple(a.value) : kwargs_dict(a.value);
            if (!tmp)
                goto cleanup;
            Py_DECREF(a.value);
            a.value = tmp;
            if (star)
                nargs += (size_t) NB_TUPLE_GET_SIZE(tmp);
            else
                nkwargs += (size_t) NB_DICT_GET_SIZE(tmp);
        }
    }
    if (cast_error)
        goto cleanup;

    /* Pass 2: fill the stack, whose entries borrow from the 'call_arg' array
       and the containers created above, and 'kwnames'. 'stack[0]' is the
       writable slot required by PEP 590. */
    stack = (PyObject **) alloca((nargs + nkwargs + 1) * sizeof(PyObject *));
    stack[0] = nullptr;
    pos = stack + 1;
    kw = pos + nargs;

    if (nkwargs && !(kwnames = PyTuple_New((Py_ssize_t) nkwargs)))
        goto cleanup;

    for (size_t i = 0; i < n; ++i) {
        call_arg &a = args[i];
        switch (a.kind) {
            case call_arg_kind::positional:
                *pos++ = a.value;
                break;

            case call_arg_kind::keyword: // 'kwnames' takes over the name
                *kw++ = a.value;
                NB_TUPLE_SET_ITEM(kwnames, (Py_ssize_t) nkw++, a.name);
                a.name = nullptr;
                break;

            case call_arg_kind::args:
                for (Py_ssize_t j = 0, l = NB_TUPLE_GET_SIZE(a.value); j < l; ++j)
                    *pos++ = NB_TUPLE_GET_ITEM(a.value, j);
                break;

            case call_arg_kind::kwargs: {
                PyObject *key, *value;
                Py_ssize_t p = 0;
                while (PyDict_Next(a.value, &p, &key, &value)) {
                    if (!PyUnicode_Check(key)) {
                        PyErr_SetString(PyExc_TypeError, "keywords must be strings");
                        goto cleanup;
                    }
                    Py_INCREF(key);
                    NB_TUPLE_SET_ITEM(kwnames, (Py_ssize_t) nkw++, key);
                    *kw++ = value;
                }
                break;
            }
        }
    }

    res = (flags & (uint32_t) call_flags::method
               ? PyObject_VectorcallMethod
               : PyObject_Vectorcall)(base, stack + 1,
                                      nargs | PY_VECTORCALL_ARGUMENTS_OFFSET,
                                      kwnames);

cleanup:
    Py_XDECREF(kwnames);
    for (size_t i = 0; i < n; ++i) {
        Py_XDECREF(args[i].value);
        Py_XDECREF(args[i].name);
    }
    if (flags & (uint32_t) call_flags::base_owned)
        Py_XDECREF(base);

    if (!res) {
        if (cast_error)
            raise_python_or_cast_error();
        else
            raise_python_error();
    }

    return res;
}


// ========================================================================

PyObject **seq_get(PyObject *seq, size_t *size_out, PyObject **temp_out) noexcept {
    PyObject *temp = nullptr;
    size_t size = 0;
    PyObject **result = nullptr;

    /* This function is used during overload resolution; if anything
       goes wrong, it fails gracefully without reporting errors. Other
       overloads will then be tried. */

    if (PyUnicode_CheckExact(seq) || PyBytes_CheckExact(seq)) {
        *size_out = 0;
        *temp_out = nullptr;
        return nullptr;
    }

#if !defined(Py_LIMITED_API) && !defined(PYPY_VERSION)
    if (PyTuple_CheckExact(seq)) {
        size = (size_t) PyTuple_GET_SIZE(seq);
        result = ((PyTupleObject *) seq)->ob_item;
        /* Special case for zero-sized lists/tuples. CPython
           sets ob_item to NULL, which this function incidentally uses to
           signal an error. Return a nonzero pointer that will, however,
           still trigger a segfault if dereferenced. */
        if (size == 0)
            result = (PyObject **) 1;
#  if !defined(NB_FREE_THREADED) // Require immutable holder in free-threaded mode
    } else if (PyList_CheckExact(seq)) {
        size = (size_t) PyList_GET_SIZE(seq);
        result = ((PyListObject *) seq)->ob_item;
        if (size == 0) // ditto
            result = (PyObject **) 1;
#  endif
    } else if (PySequence_Check(seq)) {
        temp = PySequence_Tuple(seq);

        if (temp)
            result = seq_get(temp, &size, temp_out);
        else
            PyErr_Clear();
    }
#else
    /* There isn't a nice way to get a PyObject** in Py_LIMITED_API. This
       is going to be slow, but hopefully also very future-proof.. */
    if (PySequence_Check(seq)) {
        Py_ssize_t size_seq = PySequence_Length(seq);

        if (size_seq >= 0) {
            result = (PyObject **) PyMem_Malloc(sizeof(PyObject *) * (size_t) (size_seq + 1));

            if (result) {
                result[size_seq] = nullptr;

                for (Py_ssize_t i = 0; i < size_seq; ++i) {
                    PyObject *o = PySequence_GetItem(seq, i);

                    if (o) {
                        result[i] = o;
                    } else {
                        PyErr_Clear();
                        for (Py_ssize_t j = 0; j < i; ++j)
                            Py_DECREF(result[j]);

                        PyMem_Free(result);
                        result = nullptr;
                        break;
                    }
                }
            }

            if (result) {
                temp = PyCapsule_New(result, nullptr, [](PyObject *o) {
                    PyObject **ptr = (PyObject **) PyCapsule_GetPointer(o, nullptr);
                    for (size_t i = 0; ptr[i] != nullptr; ++i)
                        Py_DECREF(ptr[i]);
                    PyMem_Free(ptr);
                });

                if (temp) {
                    size = (size_t) size_seq;
                } else {
                    PyErr_Clear();
                    for (Py_ssize_t i = 0; i < size_seq; ++i)
                        Py_DECREF(result[i]);

                    PyMem_Free(result);
                    result = nullptr;
                }
            }
        } else if (size_seq < 0) {
            PyErr_Clear();
        }
    }
#endif

    *temp_out = temp;
    *size_out = size;
    return result;
}


PyObject **seq_get_with_size(PyObject *seq, size_t size,
                             PyObject **temp_out) noexcept {

    /* This function is used during overload resolution; if anything
       goes wrong, it fails gracefully without reporting errors. Other
       overloads will then be tried. */

    PyObject *temp = nullptr,
             **result = nullptr;

#if !defined(Py_LIMITED_API) && !defined(PYPY_VERSION)
    if (PyTuple_CheckExact(seq)) {
        if (size == (size_t) PyTuple_GET_SIZE(seq)) {
            result = ((PyTupleObject *) seq)->ob_item;
            /* Special case for zero-sized lists/tuples. CPython
               sets ob_item to NULL, which this function incidentally uses to
               signal an error. Return a nonzero pointer that will, however,
               still trigger a segfault if dereferenced. */
            if (size == 0)
                result = (PyObject **) 1;
        }
#  if !defined(NB_FREE_THREADED) // Require immutable holder in free-threaded mode
    } else if (PyList_CheckExact(seq)) {
        if (size == (size_t) PyList_GET_SIZE(seq)) {
            result = ((PyListObject *) seq)->ob_item;
            if (size == 0) // ditto
                result = (PyObject **) 1;
        }
#  endif
    } else if (PySequence_Check(seq)) {
        Py_ssize_t size_seq = PySequence_Size(seq);
        if (size_seq != (Py_ssize_t) size) {
            if (size_seq == -1)
                PyErr_Clear();
        } else {
            temp = PySequence_Tuple(seq);
            if (temp)
                result = seq_get_with_size(temp, size, temp_out);
            else
                PyErr_Clear();
        }
    }
#else
    /* There isn't a nice way to get a PyObject** in Py_LIMITED_API. This
       is going to be slow, but hopefully also very future-proof.. */
    if (PySequence_Check(seq)) {
        Py_ssize_t size_seq = PySequence_Length(seq);

        if (size == (size_t) size_seq) {
            result =
                (PyObject **) PyMem_Malloc(sizeof(PyObject *) * (size + 1));

            if (result) {
                result[size] = nullptr;

                for (Py_ssize_t i = 0; i < size_seq; ++i) {
                    PyObject *o = PySequence_GetItem(seq, i);

                    if (o) {
                        result[i] = o;
                    } else {
                        PyErr_Clear();
                        for (Py_ssize_t j = 0; j < i; ++j)
                            Py_DECREF(result[j]);

                        PyMem_Free(result);
                        result = nullptr;
                        break;
                    }
                }
            }

            if (result) {
                temp = PyCapsule_New(result, nullptr, [](PyObject *o) {
                    PyObject **ptr = (PyObject **) PyCapsule_GetPointer(o, nullptr);
                    for (size_t i = 0; ptr[i] != nullptr; ++i)
                        Py_DECREF(ptr[i]);
                    PyMem_Free(ptr);
                });

                if (!temp) {
                    PyErr_Clear();
                    for (Py_ssize_t i = 0; i < size_seq; ++i)
                        Py_DECREF(result[i]);

                    PyMem_Free(result);
                    result = nullptr;
                }
            }
        } else if (size_seq < 0) {
            PyErr_Clear();
        }
    }
#endif

    *temp_out = temp;
    return result;
}

// ========================================================================

void property_install(PyObject *scope, const char *name, PyObject *getter,
                      PyObject *setter, bool is_static) noexcept {
    PyTypeObject *tp = is_static ? nb_static_property_tp() : &PyProperty_Type;
    PyObject *m = getter ? getter : setter;
    object doc = none();

    PyTypeObject *mt = m ? Py_TYPE(m) : nullptr;
    if (m && (mt == internals->nb_func || mt == internals->nb_method)) {
        func_data *f = nb_func_data(m);
        if (f->flags & (uint32_t) func_flags::has_doc)
            doc = str(f->doc);
    }

    handle(scope).attr(name) = handle(tp)(
        getter ? handle(getter) : handle(Py_None),
        setter ? handle(setter) : handle(Py_None),
        handle(Py_None), // deleter
        doc
    );
}

// ========================================================================

bool load_cmplx(PyObject *ob, uint32_t flags,
                 double *out) noexcept {
    bool is_complex = PyComplex_CheckExact(ob),
         convert = (flags & cast_flags::convert);
#if !defined(Py_LIMITED_API)
    if (is_complex || convert) {
        Py_complex result = PyComplex_AsCComplex(ob);
        if (result.real != -1.0 || !PyErr_Occurred()) {
            out[0] = result.real; out[1] = result.imag;
            return true;
        } else {
            PyErr_Clear();
        }
    }
#else
#if Py_LIMITED_API < 0x030D0000
    // Before version 3.13, __complex__() was not called by the Stable ABI
    // functions PyComplex_{Real,Imag}AsDouble(), so we do so ourselves.
    if (!is_complex && convert
            && !PyType_IsSubtype(Py_TYPE(ob), &PyComplex_Type)
            && PyObject_HasAttr(ob, NB_INTERNED(__complex__))) {
        PyObject* tmp = PyObject_CallFunctionObjArgs(
                (PyObject*) &PyComplex_Type, ob, NULL);
        if (tmp) {
            double re = PyComplex_RealAsDouble(tmp);
            double im = PyComplex_ImagAsDouble(tmp);
            Py_DECREF(tmp);
            if ((re != -1.0 && im != -1.0) || !PyErr_Occurred()) {
                out[0] = re; out[1] = im;
                return true;
            }
        }
        PyErr_Clear();
        return false;
    }
#endif
    if (is_complex || convert) {
        double re = PyComplex_RealAsDouble(ob);
        double im = PyComplex_ImagAsDouble(ob);
        if ((re != -1.0 && im != -1.0) || !PyErr_Occurred()) {
            out[0] = re; out[1] = im;
            return true;
        } else {
            PyErr_Clear();
        }
    }
#endif

    return false;
}

bool load_f64(PyObject *o, uint32_t flags, double *out) noexcept {
    bool is_float = PyFloat_CheckExact(o);

#if !defined(Py_LIMITED_API)
    if (NB_LIKELY(is_float)) {
        *out = PyFloat_AS_DOUBLE(o);
        return true;
    }

    is_float = false;
#endif

    if (is_float || (flags & cast_flags::convert)) {
        double result = PyFloat_AsDouble(o);

        if (result != -1.0 || !PyErr_Occurred()) {
            *out = result;
            return true;
        } else {
            PyErr_Clear();
        }
    }

    return false;
}

bool load_f32(PyObject *o, uint32_t flags, float *out) noexcept {
    bool is_float = PyFloat_CheckExact(o);
    bool convert = flags & cast_flags::convert;

#if !defined(Py_LIMITED_API)
    if (NB_LIKELY(is_float)) {
        double d = PyFloat_AS_DOUBLE(o);
        float result = (float) d;
        if (convert || (double) result == d || d != d) {
            *out = result;
            return true;
        } else {
            return false;
        }
    }

    is_float = false;
#endif

    if (is_float || convert) {
        double d = PyFloat_AsDouble(o);
        if (d != -1.0 || !PyErr_Occurred()) {
            float result = (float) d;
            if (convert || (double) result == d || d != d) {
                *out = result;
                return true;
            }
        } else {
            PyErr_Clear();
        }
    }

    return false;
}

#if !defined(Py_LIMITED_API) && !defined(PYPY_VERSION)

/// Sign and digits of a Python 'int', see Include/cpython/longintrepr.h
struct int_repr {
    const digit *digits; // Little-endian, PyLong_SHIFT bits each
    Py_ssize_t size;     // Number of digits, of which zero has none
    bool negative;
};

NB_INLINE int_repr int_repr_get(PyObject *o) noexcept {
    PyLongObject *l = (PyLongObject *) o;

#if PY_VERSION_HEX < 0x030c0000
    Py_ssize_t size = Py_SIZE(l);
    return { l->ob_digit, size < 0 ? -size : size, size < 0 };
#else
    uintptr_t tag = l->long_value.lv_tag;
    return { l->long_value.ob_digit,
             (Py_ssize_t) (tag >> _PyLong_NON_SIZE_BITS),
             (tag & _PyLong_SIGN_MASK) == 2 };
#endif
}

/// Convert an object that is already known to be an 'int'. Never raises.
template <typename T>
NB_INLINE bool load_int_exact(PyObject *o, T *out) noexcept {
    constexpr size_t bits = sizeof(T) * 8;

    // Number of digits of the largest value of type 'T'
    constexpr Py_ssize_t size_max =
        (Py_ssize_t) ((bits + PyLong_SHIFT - 1) / PyLong_SHIFT);

    // Largest leading digit that keeps the accumulator below 2**64
    constexpr uint64_t digit_max =
        (~(uint64_t) 0) >> ((size_max - 1) * PyLong_SHIFT);

    // Magnitude of the largest positive and negative value of type 'T'
    constexpr uint64_t max_pos = (~(uint64_t) 0) >> (64 - bits + std::is_signed_v<T>),
                       max_neg = std::is_signed_v<T> ? max_pos + 1 : 0;

    int_repr r = int_repr_get(o);
    uint64_t value;

    if (NB_LIKELY(r.size <= 1)) {
        value = r.size ? r.digits[0] : 0;
    } else {
        if (NB_UNLIKELY(r.size > size_max ||
                        (r.size == size_max && r.digits[size_max - 1] > digit_max)))
            return false;

        value = 0;
        for (Py_ssize_t i = r.size - 1; i >= 0; --i)
            value = (value << PyLong_SHIFT) | r.digits[i];
    }

    // 'max_neg' is zero when 'T' is unsigned, which rejects negative values
    if (NB_UNLIKELY(value > (r.negative ? max_neg : max_pos)))
        return false;

    *out = (T) (r.negative ? (uint64_t) 0 - value : value);
    return true;
}

#else

/// Convert an object that is already known to be an 'int'. Never raises.
template <typename T>
NB_INLINE bool load_int_exact(PyObject *o, T *out) noexcept {
    int overflow;
    long long value = PyLong_AsLongLongAndOverflow(o, &overflow);

    if (NB_LIKELY(overflow == 0)) {
        T value_t = (T) value;

        if (NB_UNLIKELY((std::is_unsigned_v<T> && value < 0) ||
                        (sizeof(T) != sizeof(long long) &&
                         value != (long long) value_t)))
            return false;

        *out = value_t;
        return true;
    }

    // Branch to handle the [2**63, 2**64) range (out of bounds of 'long long')
    if constexpr (std::is_unsigned_v<T> && sizeof(T) == sizeof(long long)) {
        if (overflow > 0) {
            // Guard against out of range values using a comparison instead of
            // letting PyLong_AsUnsignedLongLong fail, which would be much more expensive.
            if (PyObject_RichCompareBool(o, NB_INTERNED(u64_limit), Py_LT) == 1) {
                *out = (T) PyLong_AsUnsignedLongLongMask(o);
                return true;
            }
        }
    }

    return false;
}

#endif

template <typename T>
NB_INLINE bool load_int(PyObject *o, uint32_t flags, T *out) noexcept {
    if (NB_LIKELY(PyLong_CheckExact(o)))
        return load_int_exact(o, out);

    if (!(flags & cast_flags::convert))
        return false;

    // Handle subclasses of 'int' via the _exact() caster
    if (PyLong_Check(o))
        return load_int_exact(o, out);

    // Give up if we reach this point and __index__() does not exist
#if !defined(Py_LIMITED_API)
    PyNumberMethods *nm = Py_TYPE(o)->tp_as_number;
    if (!nm || !nm->nb_index)
        return false;
#else
    if (!PyIndex_Check(o))
        return false;
#endif

    PyObject *temp = PyNumber_Index(o);
    if (NB_UNLIKELY(!temp)) {
        PyErr_Clear();
        return false;
    }

    bool result = load_int_exact(temp, out);
    Py_DECREF(temp);
    return result;
}

bool load_u8(PyObject *o, uint32_t flags, uint8_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_i8(PyObject *o, uint32_t flags, int8_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_u16(PyObject *o, uint32_t flags, uint16_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_i16(PyObject *o, uint32_t flags, int16_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_u32(PyObject *o, uint32_t flags, uint32_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_i32(PyObject *o, uint32_t flags, int32_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_u64(PyObject *o, uint32_t flags, uint64_t *out) noexcept {
    return load_int(o, flags, out);
}

bool load_i64(PyObject *o, uint32_t flags, int64_t *out) noexcept {
    return load_int(o, flags, out);
}

// ========================================================================

bool gil_check() noexcept {
#if !defined(Py_LIMITED_API)
    return PyGILState_Check() != 0;
#else
    // Not expressible in the limited API; report success
    return true;
#endif
}

// ========================================================================

/* 'abi3t' extensions store the mutex as a single byte because PyMutex is not
   part of their stable ABI. A larger PyMutex would overflow that byte. */
#if defined(NB_FREE_THREADED)
static_assert(sizeof(PyMutex) == 1, "nb::ft_mutex assumes a one-byte PyMutex");
#endif

void ft_mutex_lock(void *m) noexcept {
#if defined(NB_FREE_THREADED)
    PyMutex_Lock((PyMutex *) m);
#else
    (void) m;
#endif
}

void ft_mutex_unlock(void *m) noexcept {
#if defined(NB_FREE_THREADED)
    PyMutex_Unlock((PyMutex *) m);
#else
    (void) m;
#endif
}

// ========================================================================

uint32_t read_flag(nb_flag f) noexcept {
    switch (f) {
        case nb_flag::leak_warnings:
            return internals->print_leak_warnings;
        case nb_flag::implicit_cast_warnings:
            return internals->print_implicit_cast_warnings;
        default:
            fail("nanobind::detail::read_flag(): unknown flag!");
    }
}

void write_flag(nb_flag f, uint32_t value) noexcept {
    switch (f) {
        case nb_flag::leak_warnings:
            internals->print_leak_warnings = value != 0;
            break;
        case nb_flag::implicit_cast_warnings:
            internals->print_implicit_cast_warnings = value != 0;
            break;
        default:
            fail("nanobind::detail::write_flag(): unknown flag!");
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
