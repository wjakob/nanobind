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

#if PY_VERSION_HEX >= 0x030C0000

void python_error_init(python_error *self) {
    self->m_value = PyErr_GetRaisedException();
    check(self->m_value,
          "nanobind::python_error::python_error(): error indicator unset!");
}

void python_error_destroy(python_error *self) noexcept {
    if (self->m_value) {
        gil_scoped_acquire acq;
        /* With GIL held */ {
            // Clear error status in case the following executes Python code
            error_scope scope;
            Py_DECREF(self->m_value);
        }
    }

    free(self->m_what);
}

void python_error_copy(python_error *self, const python_error *e) {
    self->m_value = e->m_value;
    if (self->m_value) {
        gil_scoped_acquire acq;
        Py_INCREF(self->m_value);
    }
    if (e->m_what)
        self->m_what = strdup_check(e->m_what);
}

void python_error_move(python_error *self, python_error *e) noexcept {
    self->m_value = e->m_value;
    self->m_what = e->m_what;
    e->m_value = nullptr;
    e->m_what = nullptr;
}

void python_error_restore(python_error *self) noexcept {
    check(self->m_value,
          "nanobind::python_error::restore(): error was already restored!");

    PyErr_SetRaisedException(self->m_value);
    self->m_value = nullptr;
}

#else /* Exception handling for Python 3.11 and older versions */

void python_error_init(python_error *self) {
    PyErr_Fetch(&self->m_type, &self->m_value, &self->m_traceback);
    check(self->m_type,
          "nanobind::python_error::python_error(): error indicator unset!");
}

void python_error_destroy(python_error *self) noexcept {
    if (self->m_type) {
        gil_scoped_acquire acq;
        /* With GIL held */ {
            // Clear error status in case the following executes Python code
            error_scope scope;
            Py_XDECREF(self->m_type);
            Py_XDECREF(self->m_value);
            Py_XDECREF(self->m_traceback);
        }
    }
    free(self->m_what);
}

void python_error_copy(python_error *self, const python_error *e) {
    self->m_type = e->m_type;
    self->m_value = e->m_value;
    self->m_traceback = e->m_traceback;
    if (self->m_type) {
        gil_scoped_acquire acq;
        Py_INCREF(self->m_type);
        Py_XINCREF(self->m_value);
        Py_XINCREF(self->m_traceback);
    }
    if (e->m_what)
        self->m_what = strdup_check(e->m_what);
}

void python_error_move(python_error *self, python_error *e) noexcept {
    self->m_type = e->m_type;
    self->m_value = e->m_value;
    self->m_traceback = e->m_traceback;
    self->m_what = e->m_what;
    e->m_type = e->m_value = e->m_traceback = nullptr;
    e->m_what = nullptr;
}

void python_error_restore(python_error *self) noexcept {
    check(self->m_type,
          "nanobind::python_error::restore(): error was already restored!");

    PyErr_Restore(self->m_type, self->m_value, self->m_traceback);
    self->m_type = self->m_value = self->m_traceback = nullptr;
}

#endif

const char *python_error_what(const python_error *self) noexcept {
    // Return the existing error message if already computed once
    if (self->m_what)
        return self->m_what;

    gil_scoped_acquire acq;

    // Try again with GIL held
    if (self->m_what)
        return self->m_what;

#if PY_VERSION_HEX < 0x030C0000
    PyErr_NormalizeException(&self->m_type, &self->m_value,
                             &self->m_traceback);
    check(self->m_type,
          "nanobind::python_error::what(): PyErr_NormalizeException() failed!");

    if (self->m_traceback) {
        if (PyException_SetTraceback(self->m_value, self->m_traceback) < 0)
            PyErr_Clear();
    }

    handle exc_type = self->m_type, exc_value = self->m_value;
    object exc_traceback = borrow(self->m_traceback);
#else
    handle exc_value = self->m_value, exc_type = exc_value.type();
    object exc_traceback = steal(PyException_GetTraceback(self->m_value));
#endif

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
    Buffer buf(128);
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

        buf.put("Traceback (most recent call last):\n");
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
            buf.put("  File \"");
            buf.put_dstr(filename);
            buf.put("\", line ");
            buf.put_uint32((uint32_t) PyFrame_GetLineNumber(frame));
            buf.put(", in ");
            buf.put_dstr(name);
            buf.put('\n');
            Py_DECREF(f_code);
            Py_DECREF(frame);
        }
    }

    if (exc_type.is_valid()) {
        try {
            object name = exc_type.attr(NB_INTERNED(__name__));
            buf.put_dstr(borrow<str>(name).c_str());
            buf.put(": ");
        } catch (...) { PyErr_Clear(); }
    }

    if (exc_value.is_valid()) {
        try {
            buf.put_dstr(str(exc_value).c_str());
        } catch (...) {
            PyErr_Clear();
            buf.put("<exception str() failed>");
        }
    }

    char *tmp = buf.copy();
#endif

    // Publish the message with a CAS; if a concurrent call raced us to it,
    // free our copy and return the winner's message instead.
    char *expected = nullptr;
#if defined(_MSC_VER)
    expected = (char *) _InterlockedCompareExchangePointer(
        (void *volatile *) &self->m_what, tmp, nullptr);
    if (!expected)
        return tmp;
#else
    if (__atomic_compare_exchange_n(&self->m_what, &expected, tmp, false,
                                    __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
        return tmp;
#endif
    free(tmp);
    return expected;
}

PyObject *exception_new(PyObject *scope, const char *name,
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
    check(result.is_valid(), "nanobind::detail::exception_new(): creation failed!");

    if (hasattr(scope, name))
        raise("nanobind::detail::exception_new(): an object of the same name "
              "already exists!");

    setattr(scope, name, result);
    return result.release().ptr();
}

void register_exception_translator(nb_internals *internals,
                                   exception_translator t, void *payload) {
    nb_translator_seq *head = new nb_translator_seq{ t, payload,
                                                     internals->translators.load_acquire() };
    internals->translators.store_release(head);
}


void chain_error_v(PyObject *type, const char *fmt, va_list args) noexcept {
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
    PyErr_FormatV(type, fmt, args);
#else
    PyObject *exc_str = PyUnicode_FromFormatV(fmt, args);
    check(exc_str, "nanobind::detail::raise_from(): PyUnicode_FromFormatV() failed!");
    PyErr_SetObject(type, exc_str);
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

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
