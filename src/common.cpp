/*
    src/common.cpp: miscellaneous libnanobind functionality

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include "internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
void raise(const char *fmt, ...) {
    char buf[512], *ptr = buf;
    va_list args;

    va_start(args, fmt);
    size_t size = vsnprintf(ptr, sizeof(buf), fmt, args);
    va_end(args);

    if (size < sizeof(buf))
        throw std::runtime_error(buf);

    ptr = (char *) malloc(size + 1);
    if (!ptr) {
        fprintf(stderr, "nb::detail::raise(): out of memory!");
        abort();
    }

    va_start(args, fmt);
    vsnprintf(ptr, size + 1, fmt, args);
    va_end(args);

    std::runtime_error err(ptr);
    free(ptr);
    throw err;
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

PyObject *capsule_new(const void *ptr, void (*free)(void *)) noexcept {
    auto capsule_free = [](PyObject *o) {
        void (*free_2)(void *) = (void (*)(void *))(PyCapsule_GetContext(o));
        if (free_2)
            free_2(PyCapsule_GetPointer(o, nullptr));
    };

    PyObject *c = PyCapsule_New((void *) ptr, nullptr, capsule_free);

    if (!c)
        fail("nanobind::detail::capsule_new(): allocation failed!");

    if (PyCapsule_SetContext(c, (void *) free) != 0)
        fail("nanobind::detail::capsule_new(): could not set context!");

    return c;
}

void raise_python_error() {
    if (PyErr_Occurred())
        throw python_error();
    else
        fail("nanobind::detail::raise_python_error() called without "
             "an error condition!");
}

void raise_next_overload() {
    throw next_overload();
}

// ========================================================================

void cleanup_list::release() noexcept {
    /* Don't decrease the reference count of the first
       element, it stores the 'self' element. */
    for (size_t i = 1; i < m_size; ++i)
        Py_DECREF(m_data[i]);
    if (m_capacity != Small)
        free(m_data);
    m_data = nullptr;
}

void cleanup_list::expand() noexcept {
    uint32_t new_capacity = m_capacity * 2;
    PyObject **new_data = (PyObject **) malloc(new_capacity * sizeof(PyObject *));
    if (!new_data)
        fail("nanobind::detail::cleanup_list::expand(): out of memory!");
    memcpy(new_data, m_data, m_size * sizeof(PyObject *));
    if (m_capacity != Small)
        free(m_data);
    m_data = new_data;
    m_capacity = new_capacity;
}

// ========================================================================

PyObject *module_new(const char *name, PyModuleDef *def) noexcept {
    memset(def, 0, sizeof(PyModuleDef));
    def->m_name = name;
    def->m_size = -1;
    PyObject *m = PyModule_Create(def);
    if (!m)
        fail("nanobind::detail::module_new(): allocation failed!");
    return m;
}

PyObject *module_import(const char *name) {
    PyObject *res = PyImport_ImportModule(name);
    if (!res)
        throw python_error();
    return res;
}

PyObject *module_new_submodule(PyObject *base, const char *name,
                               const char *doc) noexcept {

    PyObject *base_name = PyModule_GetNameObject(base),
             *name_py, *res;
    if (!base_name)
        goto fail;

    name_py = PyUnicode_FromFormat("%U.%s", base_name, name);
    if (!name_py)
        goto fail;

    res = PyImport_AddModuleObject(name_py);
    if (doc) {
        PyObject *doc_py = PyUnicode_FromString(doc);
        if (!doc_py || PyObject_SetAttrString(res, "__doc__", doc_py))
            goto fail;
        Py_DECREF(doc_py);
    }
    Py_DECREF(name_py);
    Py_DECREF(base_name);

    Py_INCREF(res);
    if (PyModule_AddObject(base, name, res))
        goto fail;

    return res;

fail:
    fail("nanobind::detail::module_new_submodule(): failed.");
}

// ========================================================================

size_t obj_len(PyObject *o) {
    Py_ssize_t res = PyObject_Length(o);
    if (res < 0)
        raise_python_error();
    return (size_t) res;
}

PyObject *obj_repr(PyObject *o) {
    PyObject *res = PyObject_Repr(o);
    if (!res)
        raise_python_error();
    return res;
}

bool obj_compare(PyObject *a, PyObject *b, int value) {
    int rv = PyObject_RichCompareBool(a, b, value);
    if (rv == -1)
        raise_python_error();
    return rv == 1;
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

#if PY_VERSION_HEX < 0x03090000
static PyObject *nb_vectorcall_method(PyObject *name, PyObject *const *args,
                                      size_t nargsf, PyObject *kwnames) {
    PyObject *obj = PyObject_GetAttr(args[0], name);
    if (!obj)
        return obj;
    PyObject *result = NB_VECTORCALL(obj, args + 1, nargsf - 1, kwnames);
    Py_DECREF(obj);
    return result;
}
#endif

PyObject *obj_vectorcall(PyObject *base, PyObject *const *args, size_t nargsf,
                         PyObject *kwnames, bool method_call) {
    const char *error = nullptr;
    PyObject *res = nullptr;

    size_t nargs_total =
        PyVectorcall_NARGS(nargsf) + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0);

    if (!PyGILState_Check()) {
        error = "nanobind::detail::obj_vectorcall(): PyGILState_Check() failure." ;
        goto end;
    }

    for (size_t i = 0; i < nargs_total; ++i) {
        if (!args[i]) {
            error = "nanobind::detail::obj_vectorcall(): argument conversion failure." ;
            goto end;
        }
    }

    res = (method_call ? NB_VECTORCALL_METHOD
                       : NB_VECTORCALL)(base, args, nargsf, kwnames);

end:
    for (size_t i = 0; i < nargs_total; ++i)
        Py_XDECREF(args[i]);
    Py_XDECREF(kwnames);
    Py_DECREF(base);

    if (error)
        raise("%s", error);
    else if (!res)
        raise_python_error();

    return res;
}


PyObject *obj_iter(PyObject *o) {
    PyObject *result = PyObject_GetIter(o);
    if (!result)
        raise_python_error();
    return result;
}

PyObject *obj_iter_next(PyObject *o) {
    PyObject *result = PyIter_Next(o);
    if (!result && PyErr_Occurred())
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

PyObject *getattr(PyObject *obj, const char *key, PyObject *def) noexcept {
    PyObject *res = PyObject_GetAttrString(obj, key);
    if (res)
        return res;
    PyErr_Clear();
    Py_XINCREF(def);
    return def;
}

PyObject *getattr(PyObject *obj, PyObject *key, PyObject *def) noexcept {
    PyObject *res = PyObject_GetAttr(obj, key);
    if (res)
        return res;
    PyErr_Clear();
    Py_XINCREF(def);
    return def;
}

void getattr_maybe(PyObject *obj, const char *key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PyObject_GetAttrString(obj, key);
    if (!res)
        raise_python_error();

    *out = res;
}

void getattr_maybe(PyObject *obj, PyObject *key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PyObject_GetAttr(obj, key);
    if (!res)
        raise_python_error();

    *out = res;
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

// ========================================================================

void getitem_maybe(PyObject *obj, Py_ssize_t key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PySequence_GetItem(obj, key);
    if (!res)
        raise_python_error();

    *out = res;
}

void getitem_maybe(PyObject *obj, const char *key_, PyObject **out) {
    if (*out)
        return;

    PyObject *key, *res;

    key = PyUnicode_FromString(key_);
    if (!key)
        raise_python_error();

    res = PyObject_GetItem(obj, key);
    Py_DECREF(key);

    if (!res)
        raise_python_error();

    *out = res;
}

void getitem_maybe(PyObject *obj, PyObject *key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PyObject_GetItem(obj, key);
    if (!res)
        raise_python_error();

    *out = res;
}

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

PyObject **seq_get(PyObject *seq, size_t *size, PyObject **temp) noexcept {
    if (PyTuple_CheckExact(seq)) {
        PyTupleObject *tuple = (PyTupleObject *) seq;
        *size = Py_SIZE(tuple);
        return tuple->ob_item;
    } else if (PyList_CheckExact(seq)) {
        PyListObject *list = (PyListObject *) seq;
        *size = Py_SIZE(list);
        return list->ob_item;
    } else {
        seq = PySequence_List(seq);
        if (!seq) {
            PyErr_Clear();
            return nullptr;
        }
        *temp = seq;
        PyListObject *list = (PyListObject *) seq;
        *size = Py_SIZE(list);
        return list->ob_item;
    }
}


PyObject **seq_get_with_size(PyObject *seq, size_t size,
                             PyObject **temp) noexcept {
    if (PyTuple_CheckExact(seq)) {
        PyTupleObject *tuple = (PyTupleObject *) seq;
        if (size != (size_t) Py_SIZE(tuple))
            return nullptr;
        return tuple->ob_item;
    } else if (PyList_CheckExact(seq)) {
        PyListObject *list = (PyListObject *) seq;
        if (size != (size_t) Py_SIZE(list))
            return nullptr;
        return list->ob_item;
    } else {
        PySequenceMethods *m = Py_TYPE(seq)->tp_as_sequence;
        if (m && m->sq_length) {
            Py_ssize_t len = m->sq_length(seq);
            if (len < 0) {
                PyErr_Clear();
                return nullptr;
            }
            if (size != (size_t) len)
                return nullptr;
        }

        seq = PySequence_List(seq);
        if (!seq) {
            PyErr_Clear();
            return nullptr;
        }

        PyListObject *list = (PyListObject *) seq;
        if (size != (size_t) Py_SIZE(list)) {
            Py_CLEAR(seq);
            return nullptr;
        }

        *temp = seq;
        return list->ob_item;
    }
}

// ========================================================================

void property_install(PyObject *scope, const char *name, bool is_static,
                      PyObject *getter, PyObject *setter) noexcept {
    const internals &internals = internals_get();
    handle property = (PyObject *) (is_static ? internals.nb_static_property
                                              : &PyProperty_Type);
    (void) is_static;
    PyObject *m = getter ? getter : setter;
    object doc = none();

    if (m && (Py_TYPE(m) == internals.nb_func ||
              Py_TYPE(m) == internals.nb_meth)) {
        func_record *f = nb_func_get(m);
        if (f->flags & (uint32_t) func_flags::has_doc)
            doc = str(f->doc);
    }

    handle(scope).attr(name) = property(
        getter ? handle(getter) : handle(Py_None),
        setter ? handle(setter) : handle(Py_None),
        handle(Py_None), // deleter
        doc
    );
}

// ========================================================================

void tuple_check(PyObject *tuple, size_t nargs) {
    for (size_t i = 0; i < nargs; ++i) {
        if (!PyTuple_GET_ITEM(tuple, i))
            raise("nanobind::detail::tuple_check(...): conversion of argument "
                  "%zu failed!", i + 1);
    }
}

// ========================================================================

_Py_IDENTIFIER(stdout);

void print(PyObject *value, PyObject *end, PyObject *file) {
    if (!file)
        file = _PySys_GetObjectId(&PyId_stdout);

    int rv = PyFile_WriteObject(value, file, Py_PRINT_RAW);
    if (rv)
        raise_python_error();

    if (end)
        rv = PyFile_WriteObject(end, file, Py_PRINT_RAW);
    else
        rv = PyFile_WriteString("\n", file);

    if (rv)
        raise_python_error();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
