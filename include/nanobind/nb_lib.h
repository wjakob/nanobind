NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/**
 * Helper class to clean temporaries created by function dispatch.
 * The first element serves a special role: it stores the 'self'
 * object of method calls (for rv_policy::reference_internal).
 */
struct NB_CORE cleanup_list {
public:
    static constexpr uint32_t Small = 6;

    cleanup_list(PyObject *self) {
        m_size = 1;
        m_capacity = Small;
        m_data = m_local;
        m_local[0] = self;
    }

    ~cleanup_list() = default;

    /// Append a single PyObject to the cleanup stack
    NB_INLINE void append(PyObject *value) noexcept {
        if (m_size >= m_capacity)
            expand();
        m_data[m_size++] = value;
    }

    NB_INLINE PyObject *self() const {
        return m_local[0];
    }

    /// This should only be called by nb_func_vectorcall_*
    void release() noexcept;

protected:
    /// Out of memory, expand..
    void expand() noexcept;

protected:
    uint32_t m_size;
    uint32_t m_capacity;
    PyObject **m_data;
    PyObject *m_local[Small];
};


// ========================================================================

/// Raise a std::runtime_error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
NB_CORE void raise(const char *fmt, ...);

/// Abort the process with a fatal error
#if defined(__GNUC__)
    __attribute__((noreturn, nothrow, __format__ (__printf__, 1, 2)))
#else
    [[noreturn, noexcept]]
#endif
NB_CORE void fail(const char *fmt, ...);

/// Raise nanobind::python_error after an error condition was found
NB_CORE void python_error_raise();

// ========================================================================

/// Convert a Python object into a Python unicode string
NB_CORE PyObject *str_from_obj(PyObject *o);

/// Convert an UTF8 null-terminated C string into a Python unicode string
NB_CORE PyObject *str_from_cstr(const char *c);

/// Convert an UTF8 C string + size into a Python unicode string
NB_CORE PyObject *str_from_cstr_and_size(const char *c, size_t n);

// ========================================================================

/// Get an object attribute or raise an exception
NB_CORE PyObject *getattr(PyObject *obj, const char *key);
NB_CORE PyObject *getattr(PyObject *obj, PyObject *key);

/// Get an object attribute or return a default value (never raises)
NB_CORE PyObject *getattr(PyObject *obj, const char *key, PyObject *def) noexcept;
NB_CORE PyObject *getattr(PyObject *obj, PyObject *key, PyObject *def) noexcept;

/// Get an object attribute or raise an exception. Skip if 'out' is non-null
NB_CORE void getattr_maybe(PyObject *obj, const char *key, PyObject **out);
NB_CORE void getattr_maybe(PyObject *obj, PyObject *key, PyObject **out);

/// Set an object attribute / item
NB_CORE void setattr(PyObject *obj, const char *key, PyObject *value);
NB_CORE void setattr(PyObject *obj, PyObject *key, PyObject *value);

// ========================================================================

/// Index into an object or raise an exception. Skip if 'out' is non-null
NB_CORE void getitem_maybe(PyObject *obj, Py_ssize_t, PyObject **out);
NB_CORE void getitem_maybe(PyObject *obj, const char *key, PyObject **out);
NB_CORE void getitem_maybe(PyObject *obj, PyObject *key, PyObject **out);

/// Set an item or raise an exception
NB_CORE void setitem(PyObject *obj, Py_ssize_t, PyObject *value);
NB_CORE void setitem(PyObject *obj, const char *key, PyObject *value);
NB_CORE void setitem(PyObject *obj, PyObject *key, PyObject *value);

// ========================================================================

/// Determine the length of a Python object
NB_CORE size_t obj_len(PyObject *o);

/// Obtain a string representation of a Python object
NB_CORE PyObject* obj_repr(PyObject *o);

/// Perform a comparison between Python objects and handle errors
NB_CORE bool obj_comp(PyObject *a, PyObject *b, int value);

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_1(PyObject *a, PyObject* (*op)(PyObject*));

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_2(PyObject *a, PyObject *b,
                           PyObject *(*op)(PyObject *, PyObject *));

// Perform a vector function call
NB_CORE PyObject *obj_vectorcall(PyObject *base, PyObject *const *args,
                                 size_t nargsf, PyObject *kwnames,
                                 bool method_call);

// ========================================================================

// Conversion validity check done by nb::make_tuple
NB_CORE void tuple_check(PyObject *tuple, size_t nargs);

// ========================================================================

// Append a single argument to a function call
NB_CORE void call_append_arg(PyObject *args, size_t &nargs, PyObject *value);

// Append a variable-length sequence of arguments to a function call
NB_CORE void call_append_args(PyObject *args, size_t &nargs, PyObject *value);

// Append a single keyword argument to a function call
NB_CORE void call_append_kwarg(PyObject *kwargs, const char *name, PyObject *value);

// Append a variable-length dictionary of keyword arguments to a function call
NB_CORE void call_append_kwargs(PyObject *kwargs, PyObject *value);

// ========================================================================

// Check if a sequence type has a given size, and fetch its values in that case
NB_CORE bool seq_size_fetch(PyObject *seq, size_t size, PyObject **out) noexcept;

// ========================================================================

/// Create a new capsule object
NB_CORE PyObject *capsule_new(const void *ptr, void (*free)(void *)) noexcept;

/// Create a new extension module with the given name
NB_CORE PyObject *module_new(const char *name, PyModuleDef *def) noexcept;

// ========================================================================

/// Create a Python function object for the given function record
NB_CORE PyObject *nb_func_new(const void *data) noexcept;

/// Generate docstrings for all newly defined functions
NB_CORE void nb_func_finalize() noexcept;

// ========================================================================

/// Create a Python type object for the given type record
struct type_data;
NB_CORE PyObject *nb_type_new(const type_data *c) noexcept;

/// Extract a pointer to a C++ type underlying a Python object, if possible
NB_CORE bool nb_type_get(const std::type_info *t, PyObject *o, uint8_t flags,
                         cleanup_list *cleanup, void **out) noexcept;

/// Cast a C++ type instance into a Python object
NB_CORE PyObject *nb_type_put(const std::type_info *cpp_type, void *value,
                              rv_policy rvp, cleanup_list *cleanup) noexcept;

// ========================================================================

// Create and install a Python property object
NB_CORE void property_install(PyObject *scope, const char *name, bool is_static,
                              PyObject *getter, PyObject *setter) noexcept;

// ========================================================================

NB_CORE PyObject *get_override(void *ptr, const std::type_info *type,
                               const char *name, bool pure);

// ========================================================================

NB_CORE void implicitly_convertible(const std::type_info *src,
                                    const std::type_info *dst) noexcept;

NB_CORE void implicitly_convertible(bool (*predicate)(PyObject *,
                                                      cleanup_list *),
                                    const std::type_info *dst) noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
