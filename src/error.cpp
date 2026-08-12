/*
    src/error.cpp: libnanobind functionality for exceptions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <cstdarg>
#include "buffer.h"
#include "nb_internals.h"

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Protected by internals->mutex in free-threaded builds
Buffer buf(128);

PyObject *error_fetch() noexcept {
    #if PY_VERSION_HEX >= 0x030C0000
        PyObject *value = PyErr_GetRaisedException();
        check(value,
              "nanobind::python_error::python_error(): error indicator unset!");
        return value;
    #else
        PyObject *t, *v, *tb;
        PyErr_Fetch(&t, &v, &tb);
        check(t, "nanobind::python_error::python_error(): error indicator unset!");
        PyErr_NormalizeException(&t, &v, &tb);
        check(v, "nanobind::python_error::python_error(): "
                 "PyErr_NormalizeException() failed!");
        if (tb) {
            if (PyException_SetTraceback(v, tb) < 0)
                PyErr_Clear();
            Py_DECREF(tb);
        }
        Py_DECREF(t);
        return v;
    #endif
}

void error_restore(PyObject *value) noexcept {
    check(value, "nanobind::python_error::restore(): error was already restored!");
    #if PY_VERSION_HEX >= 0x030C0000
        PyErr_SetRaisedException(value);
    #else
        PyErr_Restore(Py_NewRef(Py_TYPE(value)), value,
                      PyException_GetTraceback(value));
    #endif
}

void error_release(PyObject *value, char *what) noexcept {
    if (value) {
        gil_scoped_acquire acq;
        /* With GIL held */ {
            // Clear error status in case the following executes Python code
            error_scope scope;
            Py_DECREF(value);
        }
    }

    free(what);
}

PyObject *error_copy(PyObject *value, char **what) noexcept {
    if (*what)
        *what = strdup_check(*what);
    if (value) {
        gil_scoped_acquire acq;
        Py_INCREF(value);
    }
    return value;
}

const char *error_what(PyObject *value, char **what) noexcept {
    // Return the existing error message if already computed once
    if (*what)
        return *what;

    gil_scoped_acquire acq;

    // Try again with GIL held
    if (*what)
        return *what;

    handle exc_value = value, exc_type = exc_value.type();
    object exc_traceback = steal(PyException_GetTraceback(value));

#if defined(Py_LIMITED_API) || defined(PYPY_VERSION)
    char *tmp;
    try {
        object mod = module_::import_("traceback"),
               result = mod.attr("format_exception")(exc_type, exc_value, exc_traceback);
        str s = borrow<str>(str("\n").attr("join")(result));
        const char *cstr = s.c_str();
        if (!cstr) // e.g. lone surrogates from an unencodable file name
            raise_python_error();
        tmp = strdup_check(cstr);
    } catch (...) {
        PyErr_Clear();
        tmp = strdup_check("<error while formatting exception>");
    }
#else
    Buffer exc_buf(128);
    if (exc_traceback.is_valid()) {
        PyTracebackObject *to = (PyTracebackObject *) exc_traceback.ptr();

        // Get the deepest trace possible
        while (to->tb_next)
            to = to->tb_next;

        PyFrameObject *frame = to->tb_frame;
        Py_XINCREF(frame);

        std::vector<PyFrameObject *, py_allocator<PyFrameObject *>> frames;

        while (frame) {
            frames.push_back(frame);
            frame = PyFrame_GetBack(frame);
        }

        exc_buf.put("Traceback (most recent call last):\n");
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            frame = *it;
            PyCodeObject *f_code = PyFrame_GetCode(frame);
            const char *filename = borrow<str>(f_code->co_filename).c_str();
            if (!filename) {
                PyErr_Clear();
                filename = "<unencodable filename>";
            }
            const char *name = borrow<str>(f_code->co_name).c_str();
            if (!name) {
                PyErr_Clear();
                name = "<unencodable name>";
            }
            exc_buf.put("  File \"");
            exc_buf.put_dstr(filename);
            exc_buf.put("\", line ");
            exc_buf.put_uint32((uint32_t) PyFrame_GetLineNumber(frame));
            exc_buf.put(", in ");
            exc_buf.put_dstr(name);
            exc_buf.put('\n');
            Py_DECREF(f_code);
            Py_DECREF(frame);
        }
    }

    if (exc_type.is_valid()) {
        try {
            object name = exc_type.attr(NB_INTERNED(__name__));
            exc_buf.put_dstr(borrow<str>(name).c_str());
            exc_buf.put(": ");
        } catch (...) { PyErr_Clear(); }
    }

    if (exc_value.is_valid()) {
        try {
            exc_buf.put_dstr(str(exc_value).c_str());
        } catch (...) {
            PyErr_Clear();
            exc_buf.put("<exception str() failed>");
        }
    }

    char *tmp = exc_buf.copy();
#endif

    // Publish the message with a CAS; if a concurrent call raced us to it,
    // free our copy and return the winner's message instead.
    char *expected = nullptr;
#if defined(_MSC_VER)
    expected = (char *) _InterlockedCompareExchangePointer(
        (void *volatile *) what, tmp, nullptr);
    if (!expected)
        return tmp;
#else
    if (__atomic_compare_exchange_n(what, &expected, tmp, false,
                                    __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
        return tmp;
#endif
    free(tmp);
    return expected;
}

NAMESPACE_END(detail)

// Deliberately out-of-line: the key function anchors the class's vtable and
// typeinfo (see the comment in nb_error.h)
python_error::~python_error() { detail::error_release(m_value, m_what); }

builtin_exception::builtin_exception(exception_type type, const char *what)
    : std::runtime_error(what ? what : ""), m_type(type) { }
builtin_exception::~builtin_exception() { }

NAMESPACE_BEGIN(detail)

void register_exception_translator(exception_translator t, void *payload) {
    nb_translator_seq *head = new nb_translator_seq{ t, payload,
                                                     internals->translators.load_acquire() };
    internals->translators.store_release(head);
}

NB_CORE PyObject *exception_new(PyObject *scope, const char *name,
                                PyObject *base) {
    object modname;
    if (PyModule_Check(scope))
        modname = getattr(scope, "__name__", handle());
    else
        modname = getattr(scope, NB_INTERNED(__module__), handle());

    if (!modname.is_valid())
        raise("nanobind::detail::exception_new(): could not determine module "
              "name!");

    str combined =
        steal<str>(PyUnicode_FromFormat("%U.%s", modname.ptr(), name));

    object result = steal(PyErr_NewException(combined.c_str(), base, nullptr));
    check(result, "nanobind::detail::exception_new(): creation failed!");

    if (hasattr(scope, name))
        raise("nanobind::detail::exception_new(): an object of the same name "
              "already exists!");

    setattr(scope, name, result);
    return result.release().ptr();
}

NAMESPACE_END(detail)

static void chain_error_v(handle type, const char *fmt, va_list args) noexcept {
#if PY_VERSION_HEX >= 0x030C0000
    PyObject *value = PyErr_GetRaisedException();
#else
    PyObject *tp = nullptr, *value = nullptr, *traceback = nullptr;

    PyErr_Fetch(&tp, &value, &traceback);

    if (tp) {
        PyErr_NormalizeException(&tp, &value, &traceback);
        if (traceback) {
            PyException_SetTraceback(value, traceback);
            Py_DECREF(traceback);
        }

        Py_DECREF(tp);
        tp = traceback = nullptr;
    }
#endif

#if !defined(PYPY_VERSION)
    PyErr_FormatV(type.ptr(), fmt, args);
#else
    PyObject *exc_str = PyUnicode_FromFormatV(fmt, args);
    check(exc_str, "nanobind::detail::raise_from(): PyUnicode_FromFormatV() failed!");
    PyErr_SetObject(type.ptr(), exc_str);
    Py_DECREF(exc_str);
#endif

    if (!value)
        return;

    PyObject *value_2 = nullptr;
#if PY_VERSION_HEX >= 0x030C0000
    value_2 = PyErr_GetRaisedException();
#else
    PyErr_Fetch(&tp, &value_2, &traceback);
    PyErr_NormalizeException(&tp, &value_2, &traceback);
#endif

    Py_INCREF(value);
    PyException_SetCause(value_2, value); // steals
    PyException_SetContext(value_2, value); // steals

#if PY_VERSION_HEX >= 0x030C0000
    PyErr_SetRaisedException(value_2);
#else
    PyErr_Restore(tp, value_2, traceback);
#endif
}

void chain_error(handle type, const char *fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    chain_error_v(type, fmt, args);
    va_end(args);
}

void raise_from(python_error &e, handle type, const char *fmt, ...) {
    e.restore();

    va_list args;
    va_start(args, fmt);
    chain_error_v(type, fmt, args);
    va_end(args);

    detail::raise_python_error();
}

NAMESPACE_END(NB_NAMESPACE)
