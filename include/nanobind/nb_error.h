NAMESPACE_BEGIN(NB_NAMESPACE)

/// RAII wrapper that temporarily clears any Python error state
struct error_scope {
    PyObject *type, *value, *trace;
    error_scope() { PyErr_Fetch(&type, &value, &trace); }
    ~error_scope() { PyErr_Restore(type, value, trace); }
};

/// Wraps a Python error state as a C++ exception
class NB_EXPORT python_error : public std::runtime_error {
public:
    python_error();
    python_error(const python_error &);
    python_error(python_error &&);
    virtual ~python_error();

    /// Move the error back into the Python domain
    void restore();

    const handle type() const { return m_type; }
    const handle value() const { return m_value; }
    const handle trace() const { return m_trace; }

private:
    object m_type, m_value, m_trace;
};

class NB_EXPORT next_overload : public std::runtime_error {
public:
    next_overload();
    virtual ~next_overload();
};

NAMESPACE_END(NB_NAMESPACE)
