/*
    nanobind/nb_error.h: Python exception handling, binding of exceptions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

/// RAII wrapper that temporarily clears any Python error state
#if NB_PYTHON_VERSION >= 0x030C0000
struct error_scope {
    error_scope() { value = PyErr_GetRaisedException(); }
    ~error_scope() { PyErr_SetRaisedException(value); }
private:
    PyObject *value;
};
#else
struct error_scope {
    error_scope() { PyErr_Fetch(&type, &value, &trace); }
    ~error_scope() { PyErr_Restore(type, value, trace); }
private:
    PyObject *type, *value, *trace;
};
#endif

/// Tag distinguishing the exception kinds carried by a 'builtin_exception'
enum class exception_type {
    runtime_error, stop_iteration, index_error, key_error, value_error,
    type_error, buffer_error, import_error, attribute_error, next_overload
};

/* The layout of ``python_error`` and ``builtin_exception`` is part of
   nanobind's backend ABI contract and is frozen within major versions. */

inline namespace NB_BACKEND_ABI_NS {

// Wraps a Python error state as a C++ exception.
// The layout of this class is frozen and part of the nanobind ABI contract
class NB_EXPORT python_error : public std::exception {
public:
    python_error() { m_value = detail::error_fetch(); }
    python_error(const python_error &e)
        : std::exception(e), m_what(e.m_what) {
        m_value = detail::error_copy(e.m_value, &m_what);
    }
    python_error(python_error &&e) noexcept
        : std::exception(e), m_value(e.m_value), m_what(e.m_what) {
        e.m_value = nullptr;
        e.m_what = nullptr;
    }
    NB_EXPORT_SHARED ~python_error() override;

    bool matches(handle exc) const noexcept {
        return PyErr_GivenExceptionMatches(m_value, exc.ptr()) != 0;
    }

    /// Move the error back into the Python domain. This may only be called
    /// once, and you should not reraise the exception in C++ afterward.
    void restore() noexcept {
        detail::error_restore(m_value);
        m_value = nullptr;
    }

    /// Pass the error to Python's `sys.unraisablehook`, which prints
    /// a traceback to `sys.stderr` by default but may be overridden.
    /// The *context* should be some object whose repr() helps clarify where
    /// the error occurred. Like `.restore()`, this consumes the error and
    /// you should not reraise the exception in C++ afterward.
    void discard_as_unraisable(handle context) noexcept {
        restore();
        PyErr_WriteUnraisable(context.ptr());
    }

    void discard_as_unraisable(const char *context) noexcept {
        object context_s = steal(PyUnicode_FromString(context));
        discard_as_unraisable(context_s);
    }

    handle value() const { return m_value; }

    handle type() const { return value().type(); }

    object traceback() const {
        return steal(PyException_GetTraceback(m_value));
    }

    const char *what() const noexcept override {
        return detail::error_what(m_value, &m_what);
    }

private:
    mutable PyObject *m_value = nullptr;
    mutable char *m_what = nullptr;
};

static_assert(sizeof(python_error) ==
                  sizeof(std::exception) + 2 * sizeof(void *),
              "frozen layout of python_error changed");

/// Thrown by nanobind::cast when casting fails
using cast_error = std::bad_cast;

// Base interface used to expose common Python exceptions in C++
class NB_EXPORT builtin_exception : public std::runtime_error {
public:
    NB_EXPORT_SHARED builtin_exception(exception_type type, const char *what);
    NB_EXPORT_SHARED builtin_exception(builtin_exception &&) = default;
    NB_EXPORT_SHARED builtin_exception(const builtin_exception &) = default;
    NB_EXPORT_SHARED ~builtin_exception();
    NB_EXPORT_SHARED exception_type type() const { return m_type; }
private:
    exception_type m_type;
};

} // namespace NB_BACKEND_ABI_NS

#define NB_EXCEPTION(name)                                                     \
    inline builtin_exception name(const char *what = nullptr) {                \
        return builtin_exception(exception_type::name, what);                  \
    }

NB_EXCEPTION(stop_iteration)
NB_EXCEPTION(index_error)
NB_EXCEPTION(key_error)
NB_EXCEPTION(value_error)
NB_EXCEPTION(type_error)
NB_EXCEPTION(buffer_error)
NB_EXCEPTION(import_error)
NB_EXCEPTION(attribute_error)
NB_EXCEPTION(next_overload)

#undef NB_EXCEPTION

inline void register_exception_translator(detail::exception_translator t,
                                          void *payload = nullptr) {
    detail::register_exception_translator(t, payload);
}

template <typename T>
class exception : public object {
    NB_OBJECT_DEFAULT(exception, object, "Exception", PyExceptionClass_Check)

    exception(handle scope, const char *name, handle base = PyExc_Exception)
        : object(detail::exception_new(scope.ptr(), name, base.ptr()),
                 detail::steal_t()) {
        detail::register_exception_translator(
            [](const std::exception_ptr &p, void *payload) {
                try {
                    std::rethrow_exception(p);
                } catch (T &e) {
                    PyErr_SetString((PyObject *) payload, e.what());
                }
            }, m_ptr);
    }
};

NB_CORE void chain_error(handle type, const char *fmt, ...) noexcept;
[[noreturn]] NB_CORE void raise_from(python_error &e, handle type, const char *fmt, ...);

NAMESPACE_END(NB_NAMESPACE)
