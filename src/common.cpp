/*
    src/common.cpp: miscellaneous libnanobind functionality

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <complex>
#include "nb_internals.h"

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

/// Abort the process with a fatal error
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
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

/* Cleanup lists are created on both sides of the header/backend boundary
   (the dispatcher builds one per call, nb::cast builds one in the
   extension), so a list may be grown in one binary and released in another.
   The overflow buffer therefore uses PyMem_Malloc/PyMem_Free, which live in
   libpython and are shared by every binary in the process; raw
   malloc/free would pair allocators across static-CRT boundaries. */

void cleanup_list::release() noexcept {
    /* Don't decrease the reference count of the first
       element, it stores the 'self' element. */
    for (size_t i = 1; i < m_size; ++i)
        Py_DECREF(m_data[i]);
    if (m_capacity != Small)
        PyMem_Free(m_data);
    m_data = nullptr;
}

void cleanup_list::expand() noexcept {
    uint32_t new_capacity = m_capacity * 2;
    PyObject **new_data = (PyObject **) PyMem_Malloc(new_capacity * sizeof(PyObject *));
    check(new_data, "nanobind::detail::cleanup_list::expand(): out of memory!");
    memcpy(new_data, m_data, m_size * sizeof(PyObject *));
    if (m_capacity != Small)
        PyMem_Free(m_data);
    m_data = new_data;
    m_capacity = new_capacity;
}

// ========================================================================

PyObject *module_import(const char *name) {
    PyObject *res = PyImport_ImportModule(name);
    if (!res)
        throw python_error();
    return res;
}

PyObject *module_import(PyObject *o) {
    PyObject *res = PyImport_Import(o);
    if (!res)
        throw python_error();
    return res;
}

PyObject *module_new_submodule(PyObject *base, const char *name,
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

#if PY_VERSION_HEX < 0x030D00A0 || defined(Py_LIMITED_API)
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

    res.inc_ref(); // For PyModule_AddObject, which steals upon success
    if (PyModule_AddObject(base, name, res.ptr())) {
        res.dec_ref();
        goto fail;
    }

    return res.release().ptr();

fail:
    raise_python_error();
}

// ========================================================================

size_t obj_len_hint(PyObject *o) noexcept {
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

PyObject *obj_repr(PyObject *o) {
    PyObject *res = PyObject_Repr(o);
    if (!res)
        raise_python_error();
    return res;
}

PyObject *obj_op_1(PyObject *a, PyObject* (*op)(PyObject*)) {
    PyObject *res = op(a);
    if (!res)
        raise_python_error();
    return res;
}

PyObject *obj_op_2(PyObject *a, PyObject *b,
                   PyObject *(*op)(PyObject *, PyObject *) ) {
    PyObject *res = op(a, b);
    if (!res)
        raise_python_error();

    return res;
}

PyObject *obj_vectorcall(PyObject *base, PyObject *const *args, size_t nargsf,
                         PyObject *kwnames, bool method_call) {
    PyObject *res = nullptr;
    bool cast_error = false;

    size_t nargs_total = (size_t) (PyVectorcall_NARGS(nargsf) +
                         (kwnames ? NB_TUPLE_GET_SIZE(kwnames) : 0));

#if !defined(Py_LIMITED_API)
    if (!PyGILState_Check()) {
        // Deliberately leak the argument references: decref'ing them without
        // holding the GIL would be undefined behavior, and we are about to raise.
        raise("nanobind::detail::obj_vectorcall(): PyGILState_Check() failure.");
    }
#endif

    for (size_t i = 0; i < nargs_total; ++i) {
        if (!args[i]) {
            cast_error = true;
            goto end;
        }
    }

    res = (method_call ? PyObject_VectorcallMethod
                       : PyObject_Vectorcall)(base, args, nargsf, kwnames);

end:
    for (size_t i = 0; i < nargs_total; ++i)
        Py_XDECREF(args[i]);
    Py_XDECREF(kwnames);
    Py_DECREF(base);

    if (!res) {
        if (cast_error)
            raise_python_or_cast_error();
        else
            raise_python_error();
    }

    return res;
}


PyObject *obj_iter(PyObject *o) {
    PyObject *result = PyObject_GetIter(o);
    if (!result)
        raise_python_error();
    return result;
}



// ========================================================================

PyObject *getattr(PyObject *obj, const char *key) {
    PyObject *res = PyObject_GetAttrString(obj, key);
    if (!res)
        raise_python_error();
    return res;
}

PyObject *getattr(PyObject *obj, PyObject *key) {
    PyObject *res = PyObject_GetAttr(obj, key);
    if (!res)
        raise_python_error();
    return res;
}

PyObject *getattr(PyObject *obj, const char *key_, PyObject *def) noexcept {
#if (defined(Py_LIMITED_API) && PY_LIMITED_API < 0x030d0000) || defined(PYPY_VERSION)
    str key(key_);
    if (PyObject_HasAttr(obj, key.ptr())) {
        PyObject *res = PyObject_GetAttr(obj, key.ptr());
        if (res)
            return res;
        PyErr_Clear();
    }
#else
    PyObject *res;
    int rv;

    #if PY_VERSION_HEX < 0x030d0000
        rv = _PyObject_LookupAttr(obj, str(key_).ptr(), &res);
    #else
        rv = PyObject_GetOptionalAttrString(obj, key_, &res);
    #endif

    if (rv == 1)
        return res;
    else if (rv < 0)
        PyErr_Clear();
#endif

    Py_XINCREF(def);
    return def;
}

PyObject *getattr(PyObject *obj, PyObject *key, PyObject *def) noexcept {
#if (defined(Py_LIMITED_API) && PY_LIMITED_API < 0x030d0000) || defined(PYPY_VERSION)
    if (PyObject_HasAttr(obj, key)) {
        PyObject *res = PyObject_GetAttr(obj, key);
        if (res)
            return res;
        PyErr_Clear();
    }
#else
    PyObject *res;
    int rv;

    #if PY_VERSION_HEX < 0x030d0000
        rv = _PyObject_LookupAttr(obj, key, &res);
    #else
        rv = PyObject_GetOptionalAttr(obj, key, &res);
    #endif

    if (rv == 1)
        return res;
    else if (rv < 0)
        PyErr_Clear();
#endif

    Py_XINCREF(def);
    return def;
}

void setattr(PyObject *obj, const char *key, PyObject *value) {
    int rv = PyObject_SetAttrString(obj, key, value);
    if (rv)
        raise_python_error();
}

void setattr(PyObject *obj, PyObject *key, PyObject *value) {
    int rv = PyObject_SetAttr(obj, key, value);
    if (rv)
        raise_python_error();
}

void delattr(PyObject *obj, const char *key) {
#if defined(Py_LIMITED_API) && PY_LIMITED_API < 0x030D0000
    int rv = PyObject_SetAttrString(obj, key, nullptr);
#else
    int rv = PyObject_DelAttrString(obj, key);
#endif

    if (rv)
        raise_python_error();
}

void delattr(PyObject *obj, PyObject *key) {
#if defined(Py_LIMITED_API) && PY_LIMITED_API < 0x030D0000
    int rv = PyObject_SetAttr(obj, key, nullptr);
#else
    int rv = PyObject_DelAttr(obj, key);
#endif

    if (rv)
        raise_python_error();
}

// ========================================================================

void setitem(PyObject *obj, Py_ssize_t key, PyObject *value) {
    int rv = PySequence_SetItem(obj, key, value);
    if (rv)
        raise_python_error();
}

void setitem(PyObject *obj, const char *key_, PyObject *value) {
    PyObject *key = PyUnicode_FromString(key_);
    if (!key)
        raise_python_error();

    int rv = PyObject_SetItem(obj, key, value);
    Py_DECREF(key);

    if (rv)
        raise_python_error();
}

void setitem(PyObject *obj, PyObject *key, PyObject *value) {
    int rv = PyObject_SetItem(obj, key, value);
    if (rv)
        raise_python_error();
}

void delitem(PyObject *obj, Py_ssize_t key_) {
    PyObject *key = PyLong_FromSsize_t(key_);
    if (!key)
        raise_python_error();

    int rv = PyObject_DelItem(obj, key);
    Py_DECREF(key);

    if (rv)
        raise_python_error();
}

void delitem(PyObject *obj, const char *key_) {
    PyObject *key = PyUnicode_FromString(key_);
    if (!key)
        raise_python_error();

    int rv = PyObject_DelItem(obj, key);
    Py_DECREF(key);

    if (rv)
        raise_python_error();
}

void delitem(PyObject *obj, PyObject *key) {
    int rv = PyObject_DelItem(obj, key);
    if (rv)
        raise_python_error();
}

// ========================================================================

PyObject *str_from_obj(PyObject *o) {
    PyObject *result = PyObject_Str(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *str_from_cstr(const char *str) {
    PyObject *result = PyUnicode_FromString(str);
    if (!result)
        raise("nanobind::detail::str_from_cstr(): conversion error!");
    return result;
}

PyObject *str_from_cstr_and_size(const char *str, size_t size) {
    PyObject *result = PyUnicode_FromStringAndSize(str, (Py_ssize_t) size);
    if (!result)
        raise("nanobind::detail::str_from_cstr_and_size(): conversion error!");
    return result;
}

// ========================================================================

PyObject *bytes_from_obj(PyObject *o) {
    PyObject *result = PyBytes_FromObject(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *bytes_from_cstr(const char *str) {
    PyObject *result = PyBytes_FromString(str);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *bytes_from_cstr_and_size(const void *str, size_t size) {
    PyObject *result = PyBytes_FromStringAndSize((const char *) str, (Py_ssize_t) size);
    if (!result)
        raise_python_error();
    return result;
}

// ========================================================================

PyObject *bytearray_from_obj(PyObject *o) {
    PyObject *result = PyByteArray_FromObject(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *bytearray_from_cstr_and_size(const void *str, size_t size) {
    PyObject *result = PyByteArray_FromStringAndSize((const char *) str, (Py_ssize_t) size);
    if (!result)
        raise_python_error();
    return result;
}


// ========================================================================

PyObject *int_from_obj(PyObject *o) {
    PyObject *result = PyNumber_Long(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *float_from_obj(PyObject *o) {
    PyObject *result = PyNumber_Float(o);
    if (!result)
        raise_python_error();
    return result;
}

// ========================================================================

PyObject *tuple_from_obj(PyObject *o) {
    PyObject *result = PySequence_Tuple(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *list_from_obj(PyObject *o) {
    PyObject *result = PySequence_List(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *set_from_obj(PyObject *o) {
    PyObject *result = PySet_New(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *frozenset_from_obj(PyObject *o) {
    PyObject *result = PyFrozenSet_New(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *memoryview_from_obj(PyObject *o) {
    PyObject *result = PyMemoryView_FromObject(o);
    if (!result)
        raise_python_error();
    return result;
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

NB_CORE bool load_cmplx(PyObject *ob, uint32_t flags,
                        std::complex<double> *out) noexcept {
    bool is_complex = PyComplex_CheckExact(ob),
         convert = (flags & cast_flags::convert);
#if !defined(Py_LIMITED_API)
    if (is_complex || convert) {
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
                *out = std::complex<double>(re, im);
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
            *out = std::complex<double>(re, im);
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

// ========================================================================

void slice_compute(PyObject *slice, Py_ssize_t size, Py_ssize_t &start,
                   Py_ssize_t &stop, Py_ssize_t &step,
                   size_t &slice_length) {
    if (PySlice_Unpack(slice, &start, &stop, &step) < 0)
        detail::raise_python_error();
    Py_ssize_t slice_length_ =
        PySlice_AdjustIndices((Py_ssize_t) size, &start, &stop, step);
    slice_length = (size_t) slice_length_;
}

void dict_setitem(PyObject *obj, PyObject *key, PyObject *value) {
    if (PyDict_SetItem(obj, key, value))
        raise_python_error();
}

void dict_delitem(PyObject *obj, PyObject *key) {
    if (PyDict_DelItem(obj, key))
        raise_python_error();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
