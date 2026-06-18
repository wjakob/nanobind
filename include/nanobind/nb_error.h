/*
    nanobind/nb_error.h: Python exception handling, binding of exceptions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

class python_error;

NAMESPACE_BEGIN(detail)
extern void python_error_init(python_error *);
extern void python_error_copy(python_error *, const python_error *);
extern void python_error_move(python_error *, python_error *) noexcept;
extern void python_error_destroy(python_error *) noexcept;
extern void python_error_restore(python_error *) noexcept;
extern const char *python_error_what(const python_error *) noexcept;
NAMESPACE_END(detail)

/// RAII wrapper that temporarily clears any Python error state
#if PY_VERSION_HEX >= 0x030C0000
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

/// Wraps a Python error state as a C++ exception
class NB_EXPORT python_error : public std::exception {
public:
    python_error() { detail::nb_abi->python_error_init(this); }
    python_error(const python_error &e) : std::exception(e) {
        detail::nb_abi->python_error_copy(this, &e);
    }
    python_error(python_error &&e) noexcept : std::exception(e) {
        detail::nb_abi->python_error_move(this, &e);
    }
    ~python_error() override { detail::nb_abi->python_error_destroy(this); }

    bool matches(handle exc) const noexcept {
#if PY_VERSION_HEX < 0x030C0000
        return PyErr_GivenExceptionMatches(m_type, exc.ptr()) != 0;
#else
        return PyErr_GivenExceptionMatches(m_value, exc.ptr()) != 0;
#endif
    }

    /// Move the error back into the Python domain. This may only be called
    /// once, and you should not reraise the exception in C++ afterward.
    void restore() noexcept { detail::nb_abi->python_error_restore(this); }

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

#if PY_VERSION_HEX < 0x030C0000
    handle type() const { return m_type; }
    object traceback() const { return borrow(m_traceback); }
#else
    handle type() const { return value().type(); }
    object traceback() const { return steal(PyException_GetTraceback(m_value)); }
#endif
    [[deprecated]]
    object trace() const { return traceback(); }

    const char *what() const noexcept override {
        return detail::nb_abi->python_error_what(this);
    }

private:
    friend void detail::python_error_init(python_error *);
    friend void detail::python_error_copy(python_error *, const python_error *);
    friend void detail::python_error_move(python_error *, python_error *) noexcept;
    friend void detail::python_error_destroy(python_error *) noexcept;
    friend void detail::python_error_restore(python_error *) noexcept;
    friend const char *detail::python_error_what(const python_error *) noexcept;

#if PY_VERSION_HEX < 0x030C0000
    mutable PyObject *m_type = nullptr;
    mutable PyObject *m_value = nullptr;
    mutable PyObject *m_traceback = nullptr;
#else
    mutable PyObject *m_value = nullptr;
#endif
    mutable char *m_what = nullptr;
};

/// Thrown by nanobind::cast when casting fails
using cast_error = std::bad_cast;

enum class exception_type {
    runtime_error, stop_iteration, index_error, key_error, value_error,
    type_error, buffer_error, import_error, attribute_error, next_overload
};

// Base interface used to expose common Python exceptions in C++
class NB_EXPORT builtin_exception : public std::runtime_error {
public:
    builtin_exception(exception_type type, const char *what)
        : std::runtime_error(what ? what : ""), m_type(type) { }
    builtin_exception(builtin_exception &&) = default;
    builtin_exception(const builtin_exception &) = default;
    ~builtin_exception() override = default;
    exception_type type() const { return m_type; }
private:
    exception_type m_type;
};

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
        : object(detail::nb_abi->exception_new(scope.ptr(), name, base.ptr()),
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

#if defined(__GNUC__)
    __attribute__((__format__ (__printf__, 2, 3)))
#endif
inline void chain_error(handle type, const char *fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    detail::nb_abi->chain_error_v(type.ptr(), fmt, args);
    va_end(args);
}

#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 3, 4)))
#else
    [[noreturn]]
#endif
inline void raise_from(python_error &e, handle type, const char *fmt, ...) {
    e.restore();

    va_list args;
    va_start(args, fmt);
    detail::nb_abi->chain_error_v(type.ptr(), fmt, args);
    va_end(args);

    detail::raise_python_error();
}

NAMESPACE_END(NB_NAMESPACE)
