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
    __attribute__((noreturn, nothrow, __format__ (__printf__, 1, 2)))
#else
    [[noreturn, noexcept]]
#endif
void fail(const char *fmt, ...) {
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

PyObject *module_new(const char *name, PyModuleDef *def) noexcept {
    // Placement new (not an allocation).
    new (def) PyModuleDef{ /* m_base */ PyModuleDef_HEAD_INIT,
                           /* m_name */ name,
                           /* m_doc */ nullptr,
                           /* m_size */ -1,
                           /* m_methods */ nullptr,
                           /* m_slots */ nullptr,
                           /* m_traverse */ nullptr,
                           /* m_clear */ nullptr,
                           /* m_free */ nullptr };
    PyObject *m = PyModule_Create(def);
    if (!m)
        fail("nanobind::detail::module_new(): allocation failed!");
    return m;
}

void python_error_raise() {
    if (PyErr_Occurred())
        throw python_error();
    else
        fail("nanobind::detail::raise_python_error() called without "
             "an error condition!");
}

// ========================================================================

size_t obj_len(PyObject *o) {
    Py_ssize_t res = PyObject_Length(o);
    if (res < 0)
        python_error_raise();
    return (size_t) res;
}

PyObject *obj_repr(PyObject *o) {
    PyObject *res = PyObject_Repr(o);
    if (!res)
        python_error_raise();
    return res;
}

bool obj_compare(PyObject *a, PyObject *b, int value) {
    int rv = PyObject_RichCompareBool(a, b, value);
    if (rv == -1)
        python_error_raise();
    return rv == 1;
}

PyObject *obj_op_1(PyObject *a, PyObject* (*op)(PyObject*)) {
    PyObject *res = op(a);
    if (!res)
        python_error_raise();
    return res;
}

PyObject *obj_op_2(PyObject *a, PyObject *b,
                   PyObject *(*op)(PyObject *, PyObject *) ) {
    PyObject *res = op(a, b);
    if (!res)
        python_error_raise();

    return res;
}

#if PY_VERSION_HEX < 0x03090000
PyObject *nb_vectorcall_method(PyObject *name, PyObject *const *args,
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
        python_error_raise();

    return res;
}

// ========================================================================

PyObject *getattr(PyObject *obj, const char *key) {
    PyObject *res = PyObject_GetAttrString(obj, key);
    if (!res)
        python_error_raise();
    return res;
}

PyObject *getattr(PyObject *obj, PyObject *key) {
    PyObject *res = PyObject_GetAttr(obj, key);
    if (!res)
        python_error_raise();
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
        python_error_raise();

    *out = res;
}

void getattr_maybe(PyObject *obj, PyObject *key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PyObject_GetAttr(obj, key);
    if (!res)
        python_error_raise();

    *out = res;
}

void setattr(PyObject *obj, const char *key, PyObject *value) {
    int rv = PyObject_SetAttrString(obj, key, value);
    if (rv)
        python_error_raise();
}

void setattr(PyObject *obj, PyObject *key, PyObject *value) {
    int rv = PyObject_SetAttr(obj, key, value);
    if (rv)
        python_error_raise();
}

// ========================================================================

void getitem_maybe(PyObject *obj, Py_ssize_t key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PySequence_GetItem(obj, key);
    if (!res)
        python_error_raise();

    *out = res;
}

void getitem_maybe(PyObject *obj, const char *key_, PyObject **out) {
    if (*out)
        return;

    PyObject *key, *res;

    key = PyUnicode_FromString(key_);
    if (!key)
        python_error_raise();

    res = PyObject_GetItem(obj, key);
    Py_DECREF(key);

    if (!res)
        python_error_raise();

    *out = res;
}

void getitem_maybe(PyObject *obj, PyObject *key, PyObject **out) {
    if (*out)
        return;

    PyObject *res = PyObject_GetItem(obj, key);
    if (!res)
        python_error_raise();

    *out = res;
}

void setitem(PyObject *obj, Py_ssize_t key, PyObject *value) {
    int rv = PySequence_SetItem(obj, key, value);
    if (rv)
        python_error_raise();
}

void setitem(PyObject *obj, const char *key_, PyObject *value) {
    PyObject *key = PyUnicode_FromString(key_);
    if (!key)
        python_error_raise();

    int rv = PyObject_SetItem(obj, key, value);
    Py_DECREF(key);

    if (rv)
        python_error_raise();
}

void setitem(PyObject *obj, PyObject *key, PyObject *value) {
    int rv = PyObject_SetItem(obj, key, value);
    if (rv)
        python_error_raise();
}

// ========================================================================

PyObject *str_from_obj(PyObject *o) {
    PyObject *result = PyObject_Str(o);
    if (!result)
        python_error_raise();
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

bool seq_size_fetch(PyObject *seq, size_t size, PyObject **out) noexcept {
    Py_ssize_t rv = PySequence_Size(seq);
    if (rv == -1)
        PyErr_Clear();

    if (rv != (Py_ssize_t) size)
        return false;

    for (size_t i = 0; i < size; ++i) {
        out[i] = PySequence_GetItem(seq, (Py_ssize_t) i);

        if (!out[i]) {
            PyErr_Clear();
            for (size_t j = 0; j < i; ++j)
                Py_DECREF(out[j]);
            return false;
        }
    }

    return true;
}

// ========================================================================

void property_install(PyObject *scope, const char *name, bool is_static,
                      PyObject *getter, PyObject *setter) noexcept {
    handle property = (PyObject *) &PyProperty_Type;
    (void) is_static;
    PyObject *m = getter ? getter : setter;
    object doc = none();
    const internals &internals = internals_get();

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

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
