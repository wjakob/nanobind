NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

// ========================================================================

/// Raise a std::runtime_error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
extern void raise(const char *fmt, ...);

/// Abort the process with a fatal error
#if defined(__GNUC__)
    __attribute__((noreturn, nothrow, __format__ (__printf__, 1, 2)))
#else
    [[noreturn, noexcept]]
#endif
extern void fail(const char *fmt, ...);

// ========================================================================

/// Create a new capsule object
extern PyObject *capsule_new(const void *ptr, void (*free)(void *)) noexcept;

/// Create a new extension module with the given name
extern PyObject *module_new(const char *name, PyModuleDef *def) noexcept;

// ========================================================================

/// Create a new handle for a function to be bound
extern void *func_alloc() noexcept;

/// Free all memory associated with a function handle
extern void func_free(void *handle) noexcept;

/// Annotate a function handle with the given flag
extern void func_set_flag(void *handle, uint32_t flag) noexcept;

/// Set the function name
extern void func_set_name(void *handle, const char *name) noexcept;

/// Set the function docstring
extern void func_set_docstr(void *handle, const char *docstr) noexcept;

/// Set the function scope
extern void func_set_scope(void *handle, PyObject *scope) noexcept;

/// Set the predecessor of a overload chain
extern void func_set_pred(void *handle, PyObject *pred) noexcept;

/// Create a Python function object for the given function handle
extern PyObject *func_init(void *handle, void (*free_captured)(void *),
                           PyObject *(*impl)(void *)) noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
