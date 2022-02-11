NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

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

/// Set an object attribute
NB_CORE void setattr(PyObject *obj, const char *key, PyObject *value);
NB_CORE void setattr(PyObject *obj, PyObject *key, PyObject *value);

// ========================================================================

/// Perform a comparison between Python objects and handle errors
NB_CORE bool obj_comp(PyObject *a, PyObject *b, int value);

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_1(PyObject *a, PyObject* (*op)(PyObject*));

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_2(PyObject *a, PyObject *b,
                          PyObject *(*op)(PyObject *, PyObject *));

// ========================================================================

// Check if a sequence type has a given size
NB_CORE bool seq_size_fetch(PyObject *seq, size_t size, PyObject **out) noexcept;

// ========================================================================

/// Create a new capsule object
NB_CORE PyObject *capsule_new(const void *ptr, void (*free)(void *)) noexcept;

/// Create a new extension module with the given name
NB_CORE PyObject *module_new(const char *name, PyModuleDef *def) noexcept;

// ========================================================================

/// Create a Python function object for the given function record
NB_CORE PyObject *func_new(void *data, bool return_handle) noexcept;

/// Generate docstrings for all newly defined functions
NB_CORE void func_finalize() noexcept;

// ========================================================================

/// Create a Python type object for the given type record
struct type_data;
NB_CORE PyObject *type_new(const type_data *c) noexcept;

/// Extract a pointer to a C++ type underlying a Python object, if possible
NB_CORE bool type_get(PyObject *o, const std::type_info *, bool convert,
                      void **out) noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
