/*
    nanobind/nb_error.h: Python exception handling, binding of exceptions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

/// RAII wrapper that temporarily clears any Python error state
struct error_scope {
    error_scope() { PyErr_Fetch(&type, &value, &trace); }
    ~error_scope() { PyErr_Restore(type, value, trace); }
    PyObject *type, *value, *trace;
};

/// Wraps a Python error state as a C++ exception
class NB_EXPORT python_error : public std::exception {
public:
    python_error();
    python_error(const python_error &);
    python_error(python_error &&) noexcept;
    ~python_error() override;

    bool matches(handle exc) const noexcept {
        return PyErr_GivenExceptionMatches(m_type, exc.ptr()) != 0;
    }

    /// Move the error back into the Python domain
    void restore() noexcept;

    handle type() const { return m_type; }
    handle value() const { return m_value; }
    handle trace() const { return m_trace; }

    const char *what() const noexcept override;

private:
    mutable PyObject *m_type = nullptr;
    mutable PyObject *m_value = nullptr;
    mutable PyObject *m_trace = nullptr;
    mutable char *m_what = nullptr;
};

/// Throw from a bound method to skip to the next overload
class NB_EXPORT next_overload : public std::exception {
public:
    next_overload();
    ~next_overload() override;
    const char *what() const noexcept override;
};

/// Thrown by nanobind::cast when casting fails
class NB_EXPORT cast_error : public std::exception {
public:
    cast_error();
    ~cast_error() override;
    const char *what() const noexcept override;
};

// Base interface used to expose common Python exceptions in C++
class NB_EXPORT builtin_exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    virtual void set_error() const = 0;
};

#define NB_EXCEPTION(type)                                          \
    class NB_EXPORT type : public builtin_exception {               \
    public:                                                         \
        using builtin_exception::builtin_exception;                 \
        type();                                                     \
        void set_error() const override;                            \
    };

NB_EXCEPTION(stop_iteration)
NB_EXCEPTION(index_error)
NB_EXCEPTION(key_error)
NB_EXCEPTION(value_error)
NB_EXCEPTION(type_error)
NB_EXCEPTION(buffer_error)
NB_EXCEPTION(import_error)
NB_EXCEPTION(attribute_error)

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

NAMESPACE_END(NB_NAMESPACE)
