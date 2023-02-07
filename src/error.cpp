/*
    src/error.cpp: libnanobind functionality for exceptions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include "buffer.h"
#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

Buffer buf(128);

NAMESPACE_END(detail)

python_error::python_error() {
    PyErr_Fetch(&m_type, &m_value, &m_trace);
    if (!m_type)
        detail::fail("nanobind::python_error::python_error(): error indicator unset!");
}

python_error::~python_error() {
    if (m_type || m_value || m_trace) {
        gil_scoped_acquire acq;
        /* With GIL held */ {
            // Clear error status in case the following executes Python code
            error_scope scope;
            Py_XDECREF(m_type);
            Py_XDECREF(m_value);
            Py_XDECREF(m_trace);
        }
    }
    free(m_what);
}

python_error::python_error(const python_error &e)
    : std::exception(e), m_type(e.m_type), m_value(e.m_value),
      m_trace(e.m_trace) {
    if (m_type || m_value || m_trace) {
        gil_scoped_acquire acq;
        Py_XINCREF(m_type);
        Py_XINCREF(m_value);
        Py_XINCREF(m_trace);
    }
    if (e.m_what)
        m_what = NB_STRDUP(e.m_what);
}

python_error::python_error(python_error &&e) noexcept
    : std::exception(e), m_type(e.m_type), m_value(e.m_value),
      m_trace(e.m_trace), m_what(e.m_what) {
    e.m_type = e.m_value = e.m_trace = nullptr;
    e.m_what = nullptr;
}

const char *python_error::what() const noexcept {
    using detail::buf;

    // Return the existing error message if already computed once
    if (m_what)
        return m_what;

    gil_scoped_acquire acq;

    // Try again with GIL held
    if (m_what)
        return m_what;

    PyErr_NormalizeException(&m_type, &m_value, &m_trace);

    if (!m_type)
        detail::fail("nanobind::python_error::what(): PyNormalize_Exception() failed!");

    if (m_trace) {
        if (PyException_SetTraceback(m_value, m_trace) < 0)
            PyErr_Clear();
    }

#if defined(Py_LIMITED_API) || defined(PYPY_VERSION)
    object mod = module_::import_("traceback"),
           result = mod.attr("format_exception")(handle(m_type), handle(m_value), handle(m_trace));
    m_what = NB_STRDUP(borrow<str>(str("\n").attr("join")(result)).c_str());
#else
    buf.clear();
    if (m_trace) {
        PyTracebackObject *to = (PyTracebackObject *) m_trace;

        // Get the deepest trace possible
        while (to->tb_next)
            to = to->tb_next;

        PyFrameObject *frame = to->tb_frame;
        Py_XINCREF(frame);

        std::vector<PyFrameObject *, detail::py_allocator<PyFrameObject *>> frames;

        while (frame) {
            frames.push_back(frame);
#if PY_VERSION_HEX >= 0x03090000
            frame = PyFrame_GetBack(frame);
#else
            frame = frame->f_back;
            Py_XINCREF(frame);
#endif
        }

        buf.put("Traceback (most recent call last):\n");
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            frame = *it;
#if PY_VERSION_HEX >= 0x03090000
            PyCodeObject *f_code = PyFrame_GetCode(frame);
#else
            PyCodeObject *f_code = frame->f_code;
#endif
            buf.put("  File \"");
            buf.put_dstr(borrow<str>(f_code->co_filename).c_str());
            buf.put("\", line ");
            buf.put_uint32(PyFrame_GetLineNumber(frame));
            buf.put(", in ");
            buf.put_dstr(borrow<str>(f_code->co_name).c_str());
            buf.put('\n');
#if PY_VERSION_HEX >= 0x03090000
            Py_DECREF(f_code);
#endif
            Py_DECREF(frame);
        }
    }

    if (m_type) {
        object name = handle(m_type).attr("__name__");
        buf.put_dstr(borrow<str>(name).c_str());
        buf.put(": ");
    }

    if (m_value)
        buf.put_dstr(str(m_value).c_str());
    m_what = buf.copy();
#endif

    return m_what;
}

void python_error::restore() noexcept {
    if (!m_type)
        detail::fail("nanobind::python_error::restore(): error was already restored!");

    PyErr_Restore(m_type, m_value, m_trace);
    m_type = m_value = m_trace = nullptr;
}

next_overload::next_overload() : std::exception() { }
next_overload::~next_overload() = default;
const char *next_overload::what() const noexcept { return "nanobind::next_overload"; }

cast_error::cast_error() : std::exception() { }
cast_error::~cast_error() = default;
const char *cast_error::what() const noexcept { return "nanobind::cast_error"; }

#define NB_EXCEPTION(name, type)                                               \
    name::name() : builtin_exception("") { }                                   \
    void name::set_error() const { PyErr_SetString(type, what()); }

NB_EXCEPTION(stop_iteration, PyExc_StopIteration)
NB_EXCEPTION(index_error, PyExc_IndexError)
NB_EXCEPTION(key_error, PyExc_KeyError)
NB_EXCEPTION(value_error, PyExc_ValueError)
NB_EXCEPTION(type_error, PyExc_TypeError)
NB_EXCEPTION(buffer_error, PyExc_BufferError)
NB_EXCEPTION(import_error, PyExc_ImportError)
NB_EXCEPTION(attribute_error, PyExc_AttributeError)

#undef NB_EXCEPTION

NAMESPACE_BEGIN(detail)

void register_exception_translator(exception_translator t, void *payload) {
    auto &et = internals_get().exception_translators;
    et.insert(et.begin(), { t, payload });
}

NB_CORE PyObject *exception_new(PyObject *scope, const char *name,
                                PyObject *base) {
    object modname;
    if (PyModule_Check(scope))
        modname = getattr(scope, "__name__", handle());
    else
        modname = getattr(scope, "__module__", handle());

    if (!modname.is_valid())
        raise("nanobind::detail::exception_new(): could not determine module name!");

    str combined = steal<str>(
        PyUnicode_FromFormat("%U.%s", modname.ptr(), name));

    PyObject *result = PyErr_NewException(combined.c_str(), base, nullptr);
    if (!result)
        raise("nanobind::detail::exception_new(): creation failed!");

    if (hasattr(scope, name))
        raise("nanobind::detail::exception_new(): an object of the same name already "
              "exists!");

    setattr(scope, name, result);
    return result;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
