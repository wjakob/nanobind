/*
    nanobind/nb_lib.h: Interface to libnanobind.so

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

NAMESPACE_BEGIN(dlpack)

// The version of DLPack that is supported by libnanobind
static constexpr uint32_t major_version = 1;
static constexpr uint32_t minor_version = 1;

// Forward declarations for types in ndarray.h (1)
struct dltensor;
struct dtype;

NAMESPACE_END(dlpack)

class python_error;

NAMESPACE_BEGIN(detail)

// Forward declarations for types in ndarray.h (2)
struct ndarray_handle;
struct ndarray_config;

// ========================================================================
// Backend function table (nb_abi). The compiled backend ('nanobind-abi')
// exposes its interface as this struct of function pointers; header-only
// nanobind reaches every backend entry point through it. The table is
// append-only: new revisions add slots at the end, never reorder or remove.
// ========================================================================

// Opaque to headers: the per-domain backend state, threaded as a handle.
struct nb_internals;

// Forward declarations for types named (only by pointer) in the table below.
// (ndarray_handle is already declared above.)
struct func_data_init_base;
struct cleanup_list;
struct enum_data_init;
struct type_data_init;
struct ticket;

// Highest table revision these headers target. Keep this at 1 while the ABI
// split is still under development.
#define NB_ABI_VERSION 1

// Single source of truth for the table layout. Each entry expands to a struct
// slot (here) and a populated function pointer (in the backend). Append only.
#define NB_ABI_FUNCTIONS(F)                                                    \
    F(nb_func_new, PyObject *, (nb_internals *, const func_data_init_base *) noexcept) \
    F(leak_warnings, bool, (nb_internals *) noexcept)                          \
    F(implicit_cast_warnings, bool, (nb_internals *) noexcept)                 \
    F(set_leak_warnings, void, (nb_internals *, bool) noexcept)                \
    F(set_implicit_cast_warnings, void, (nb_internals *, bool) noexcept)       \
    F(register_exception_translator, void,                                     \
      (nb_internals *, void (*)(const std::exception_ptr &, void *), void *))   \
    F(implicitly_convertible_cpp, void,                                        \
      (nb_internals *, const std::type_info *, const std::type_info *) noexcept)\
    F(implicitly_convertible_py, void,                                         \
      (nb_internals *, bool (*)(nb_internals *, PyTypeObject *, PyObject *,    \
                                cleanup_list *), const std::type_info *)       \
       noexcept)                                                              \
    F(enum_create, PyObject *, (nb_internals *, enum_data_init *) noexcept)    \
    F(enum_from_python, bool,                                                  \
      (nb_internals *, const std::type_info *, PyObject *, int64_t *, uint8_t) \
      noexcept)                                                                \
    F(enum_from_cpp, PyObject *,                                               \
      (nb_internals *, const std::type_info *, int64_t) noexcept)              \
    F(ndarray_export, PyObject *,                                              \
      (nb_internals *, ndarray_handle *, int, rv_policy, cleanup_list *) noexcept) \
    F(nb_type_new, PyObject *, (nb_internals *, const type_data_init *) noexcept) \
    F(nb_type_get, bool, (nb_internals *, const std::type_info *, PyObject *,   \
                          uint8_t, cleanup_list *, void **) noexcept)           \
    F(nb_type_put, PyObject *, (nb_internals *, const std::type_info *, void *, \
                               rv_policy, cleanup_list *, bool *) noexcept)     \
    F(nb_type_put_p, PyObject *, (nb_internals *, const std::type_info *,       \
                                 const std::type_info *, void *, rv_policy,     \
                                 cleanup_list *, bool *) noexcept)              \
    F(nb_type_put_unique, PyObject *, (nb_internals *, const std::type_info *,  \
                                      void *, cleanup_list *, bool) noexcept)   \
    F(nb_type_put_unique_p, PyObject *, (nb_internals *, const std::type_info *,\
                                        const std::type_info *, void *,         \
                                        cleanup_list *, bool) noexcept)         \
    F(nb_type_check, bool, (nb_internals *, PyObject *) noexcept)               \
    F(nb_type_isinstance, bool,                                                \
      (nb_internals *, PyObject *, const std::type_info *) noexcept)            \
    F(nb_type_lookup, PyObject *, (nb_internals *, const std::type_info *) noexcept) \
    F(keep_alive, void, (nb_internals *, PyObject *, PyObject *))               \
    F(keep_alive_cb, void,                                                     \
      (nb_internals *, PyObject *, void *, void (*)(void *) noexcept) noexcept) \
    F(property_install, void,                                                  \
      (nb_internals *, PyObject *, const char *, PyObject *, PyObject *) noexcept) \
    F(property_install_static, void,                                           \
      (nb_internals *, PyObject *, const char *, PyObject *, PyObject *) noexcept) \
    F(trampoline_new, void, (nb_internals *, void **, size_t, void *) noexcept) \
    F(trampoline_enter, void,                                                  \
      (nb_internals *, void **, size_t, const char *, bool, ticket *))         \
    F(load_i8,  bool, (PyObject *, uint8_t, int8_t *) noexcept)                 \
    F(load_u8,  bool, (PyObject *, uint8_t, uint8_t *) noexcept)                \
    F(load_i16, bool, (PyObject *, uint8_t, int16_t *) noexcept)                \
    F(load_u16, bool, (PyObject *, uint8_t, uint16_t *) noexcept)               \
    F(load_i32, bool, (PyObject *, uint8_t, int32_t *) noexcept)                \
    F(load_u32, bool, (PyObject *, uint8_t, uint32_t *) noexcept)               \
    F(load_i64, bool, (PyObject *, uint8_t, int64_t *) noexcept)                \
    F(load_u64, bool, (PyObject *, uint8_t, uint64_t *) noexcept)               \
    F(load_f32, bool, (PyObject *, uint8_t, float *) noexcept)                  \
    F(load_f64, bool, (PyObject *, uint8_t, double *) noexcept)                 \
    F(load_cmplx, bool, (PyObject *, uint8_t, double *, double *) noexcept)     \
    F(incref_checked, void, (PyObject *) noexcept)                              \
    F(decref_checked, void, (PyObject *) noexcept)                              \
    F(obj_len_hint, size_t, (PyObject *) noexcept)                              \
    F(obj_vectorcall, PyObject *, (PyObject *, PyObject *const *, size_t,       \
                                   PyObject *, bool))                           \
    F(tuple_check, void, (PyObject *, size_t))                                  \
    F(capsule_new, PyObject *, (const void *, const char *,                     \
                                void (*)(void *) noexcept) noexcept)            \
    F(print, void, (PyObject *, PyObject *, PyObject *))                        \
    F(exception_new, PyObject *, (PyObject *, const char *, PyObject *))        \
    F(iterable_check, bool, (PyObject *) noexcept)                              \
    F(try_iter, PyObject *, (PyObject *) noexcept)                              \
    F(repr_list, PyObject *, (PyObject *))                                      \
    F(repr_map, PyObject *, (PyObject *))                                       \
    F(seq_get_with_size, PyObject **, (PyObject *, size_t, PyObject **) noexcept)\
    F(seq_get, PyObject **, (PyObject *, size_t *, PyObject **) noexcept)       \
    F(module_new_submodule, PyObject *, (PyObject *, const char *,              \
                                         const char *) noexcept)                \
    F(dict_getitem_or_raise, void, (PyObject *, PyObject *, PyObject **))       \
    F(dict_getitem_or_default, PyObject *, (PyObject *, PyObject *, PyObject *))\
    F(nb_type_relinquish_ownership, bool, (PyObject *, bool) noexcept)          \
    F(nb_type_restore_ownership, void, (PyObject *, bool) noexcept)             \
    F(nb_type_supplement, void *, (PyObject *) noexcept)                        \
    F(nb_type_size, size_t, (PyObject *) noexcept)                              \
    F(nb_type_align, size_t, (PyObject *) noexcept)                             \
    F(nb_type_name, PyObject *, (PyObject *) noexcept)                          \
    F(nb_inst_name, PyObject *, (PyObject *) noexcept)                          \
    F(nb_type_info, const std::type_info *, (PyObject *) noexcept)              \
    F(type_get_slot_impl, void *, (PyTypeObject *, int))                        \
    F(nb_inst_ptr, void *, (PyObject *) noexcept)                               \
    F(nb_inst_alloc, PyObject *, (PyTypeObject *))                              \
    F(nb_inst_alloc_zero, PyObject *, (PyTypeObject *))                         \
    F(nb_inst_reference, PyObject *, (PyTypeObject *, void *, PyObject *))      \
    F(nb_inst_take_ownership, PyObject *, (PyTypeObject *, void *))             \
    F(nb_inst_destruct, void, (PyObject *) noexcept)                            \
    F(nb_inst_zero, void, (PyObject *) noexcept)                                \
    F(nb_inst_copy, void, (PyObject *, const PyObject *) noexcept)              \
    F(nb_inst_move, void, (PyObject *, const PyObject *) noexcept)              \
    F(nb_inst_replace_copy, void, (PyObject *, const PyObject *) noexcept)      \
    F(nb_inst_replace_move, void, (PyObject *, const PyObject *) noexcept)      \
    F(nb_inst_python_derived, bool, (PyObject *) noexcept)                      \
    F(nb_inst_set_state, void, (PyObject *, bool, bool) noexcept)               \
    F(nb_inst_state_read, uint8_t, (PyObject *) noexcept)                       \
    F(getattr_str_def, PyObject *, (PyObject *, const char *, PyObject *) noexcept) \
    F(getattr_obj_def, PyObject *, (PyObject *, PyObject *, PyObject *) noexcept) \
    F(delattr_str, void, (PyObject *, const char *))                            \
    F(delattr_obj, void, (PyObject *, PyObject *))                              \
    F(enum_append, void, (PyObject *, const char *, int64_t, const char *,      \
                          const char *) noexcept)                               \
    F(enum_export, void, (PyObject *))                                          \
    F(ndarray_import, ndarray_handle *, (PyObject *, const ndarray_config *,    \
                                         bool, cleanup_list *) noexcept)        \
    F(ndarray_create, ndarray_handle *, (void *, size_t, const size_t *,        \
                                         PyObject *, const int64_t *,           \
                                         dlpack::dtype, bool, int, int, char,   \
                                         uint64_t))                             \
    F(ndarray_inc_ref, dlpack::dltensor *, (ndarray_handle *) noexcept)         \
    F(ndarray_dec_ref, void, (ndarray_handle *) noexcept)                       \
    F(ndarray_check, bool, (PyObject *) noexcept)                               \
    F(is_alive, bool, () noexcept)                                              \
    F(abi_tag, const char *, ())                                               \
    F(raise_v, void, (const char *, va_list))                                  \
    F(raise_type_error_v, void, (const char *, va_list))                       \
    F(fail_v, void, (const char *, va_list) noexcept)                          \
    F(raise_python_error_impl, void, ())                                       \
    F(raise_python_or_cast_error_impl, void, ())                               \
    F(chain_error_v, void, (PyObject *, const char *, va_list) noexcept)       \
    F(python_error_init, void, (python_error *))                               \
    F(python_error_copy, void, (python_error *, const python_error *))         \
    F(python_error_move, void, (python_error *, python_error *) noexcept)      \
    F(python_error_destroy, void, (python_error *) noexcept)                   \
    F(python_error_restore, void, (python_error *) noexcept)                   \
    F(python_error_what, const char *, (const python_error *) noexcept)        \
    F(cleanup_list_expand, void, (cleanup_list *) noexcept)                     \
    F(cleanup_list_release, void, (cleanup_list *) noexcept)                   \
    F(trampoline_release_impl, void, (void **, size_t) noexcept)               \
    F(trampoline_leave_impl, void, (ticket *) noexcept)

struct nb_abi {
    uint32_t struct_size;   // sizeof(nb_abi) as the backend built it
    uint32_t version;       // highest revision the backend implements
#define NB_ABI_SLOT(name, ret, args) ret (*name) args;
    NB_ABI_FUNCTIONS(NB_ABI_SLOT)
#undef NB_ABI_SLOT
};

// One per extension shared object (NB_NAMESPACE is hidden-visibility, so this
// inline variable merges within a DSO but is not shared across DSOs). Set at
// module init; thereafter every detail::nb_* call routes through it.
inline const nb_abi *nb_abi = nullptr;

// This extension's per-domain backend state, resolved once at module init.
inline nb_internals *nb_abi_internals = nullptr;

/**
 * Helper class to clean temporaries created by function dispatch.
 * The first element serves a special role: it stores the 'self'
 * object of method calls (for rv_policy::reference_internal).
 */
struct cleanup_list {
public:
    static constexpr uint32_t Small = 6;

    cleanup_list(PyObject *self) :
        m_size{1},
        m_capacity{Small},
        m_data{m_local} {
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

    /// Decrease the reference count of all appended objects
    void release() noexcept { nb_abi->cleanup_list_release(this); }

    /// Does the list contain any entries? (besides the 'self' argument)
    bool used() { return m_size != 1; }

    /// Return the size of the cleanup stack
    size_t size() const { return m_size; }

    /// Subscript operator
    PyObject *operator[](size_t index) const { return m_data[index]; }

protected:
    /// Out of memory, expand.. (the real work lives in the backend slot, which
    /// touches the frozen fields below and is therefore a friend)
    void expand() noexcept { nb_abi->cleanup_list_expand(this); }

    friend void cleanup_list_expand(cleanup_list *) noexcept;
    friend void cleanup_list_release(cleanup_list *) noexcept;

protected:
    uint32_t m_size;
    uint32_t m_capacity;
    PyObject **m_data;
    PyObject *m_local[Small];
};

// Freeze guard: cleanup_list::append() is inlined into compiled extensions and
// touches the fields above directly, so its layout is frozen. Pin the 64-bit
// size against accidental field additions (other ABIs are not constrained).
static_assert(sizeof(void *) != 8 || sizeof(cleanup_list) == 64,
              "frozen ABI size of cleanup_list changed");

// ========================================================================

/// Raise a runtime error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
inline void raise(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    nb_abi->raise_v(fmt, args);
    va_end(args);
    NB_UNREACHABLE();
}

/// Raise a type error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
inline void raise_type_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    nb_abi->raise_type_error_v(fmt, args);
    va_end(args);
    NB_UNREACHABLE();
}

/// Abort the process with a fatal error
#if defined(__GNUC__)
    __attribute__((noreturn, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]]
#endif
inline void fail(const char *fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    nb_abi->fail_v(fmt, args);
    va_end(args);
    NB_UNREACHABLE();
}

/// Raise nanobind::python_error after an error condition was found. Kept out of
/// line (NB_NOINLINE): it is the cold target of the many inlined raise_if_*
/// checks, so each call site emits just a call rather than the load+indirect+trap.
[[noreturn]] NB_NOINLINE inline void raise_python_error() {
    nb_abi->raise_python_error_impl();
    NB_UNREACHABLE();
}

/// Raise nanobind::cast_error
[[noreturn]] inline void raise_python_or_cast_error() {
    nb_abi->raise_python_or_cast_error_impl();
    NB_UNREACHABLE();
}

// Shared error-check helpers. The CPython calls themselves are inlined at the
// call sites; these only fold in the cold raise path (raise_python_error is a
// table slot), so no 1:1 CPython wrapper needs to live in detail::.
inline PyObject *raise_if_null(PyObject *p) {
    if (NB_UNLIKELY(!p))
        raise_python_error();
    return p;
}

inline void raise_if_nonzero(int rc) {
    if (NB_UNLIKELY(rc))
        raise_python_error();
}

// Helpers that are NOT 1:1 CPython wrappers (each folds a non-pass-through
// check or a second call), so they stay inline here rather than at call sites.
inline bool obj_comp(PyObject *a, PyObject *b, int op) {
    int rv = PyObject_RichCompareBool(a, b, op);
    if (NB_UNLIKELY(rv == -1))
        raise_python_error();
    return rv == 1;
}

inline size_t obj_len(PyObject *o) {
    Py_ssize_t res = PyObject_Size(o);
    if (NB_UNLIKELY(res < 0))
        raise_python_error();
    return (size_t) res;
}

// object -> Python bool: PyObject_IsTrue gives truthiness, PyBool_FromLong the
// singleton (no single 'object -> bool object' C-API exists).
inline PyObject *bool_from_obj(PyObject *o) {
    int rv = PyObject_IsTrue(o);
    if (NB_UNLIKELY(rv == -1))
        raise_python_error();
    return PyBool_FromLong(rv);
}

// PyIter_Next returns null both on exhaustion (no error set) and on error, so
// this is not a raise_if_null wrapper; it stays inline as a small shared helper.
inline PyObject *obj_iter_next(PyObject *o) {
    PyObject *r = PyIter_Next(o);
    if (NB_UNLIKELY(!r && PyErr_Occurred())) raise_python_error();
    return r;
}

// ========================================================================

NB_CORE void nb_module_exec(const char *domain, PyObject *m);
NB_CORE void nb_module_free(void *m);

// ========================================================================

/// Create a Python function object for the given function record. Forwards to
/// the backend through the table, supplying this domain's internals handle.
inline PyObject *nb_func_new(const func_data_init_base *f) noexcept {
    return nb_abi->nb_func_new(nb_abi_internals, f);
}

// ========================================================================

/// Create a Python type object for the given type record
inline PyObject *nb_type_new(const type_data_init *c) noexcept {
    return nb_abi->nb_type_new(nb_abi_internals, c);
}

/// Extract a pointer to a C++ type underlying a Python object, if possible
inline bool nb_type_get(const std::type_info *t, PyObject *o, uint8_t flags,
                        cleanup_list *cleanup, void **out) noexcept {
    return nb_abi->nb_type_get(nb_abi_internals, t, o, flags, cleanup, out);
}

/// Cast a C++ type instance into a Python object
inline PyObject *nb_type_put(const std::type_info *cpp_type, void *value,
                             rv_policy rvp, cleanup_list *cleanup,
                             bool *is_new = nullptr) noexcept {
    return nb_abi->nb_type_put(nb_abi_internals, cpp_type, value, rvp, cleanup, is_new);
}

// Special version of nb_type_put for polymorphic classes
inline PyObject *nb_type_put_p(const std::type_info *cpp_type,
                               const std::type_info *cpp_type_p, void *value,
                               rv_policy rvp, cleanup_list *cleanup,
                               bool *is_new = nullptr) noexcept {
    return nb_abi->nb_type_put_p(nb_abi_internals, cpp_type, cpp_type_p, value,
                                 rvp, cleanup, is_new);
}

// Special version of 'nb_type_put' for unique pointers and ownership transfer
inline PyObject *nb_type_put_unique(const std::type_info *cpp_type,
                                    void *value, cleanup_list *cleanup,
                                    bool cpp_delete) noexcept {
    return nb_abi->nb_type_put_unique(nb_abi_internals, cpp_type, value, cleanup,
                                      cpp_delete);
}

// Special version of 'nb_type_put_unique' for polymorphic classes
inline PyObject *nb_type_put_unique_p(const std::type_info *cpp_type,
                                      const std::type_info *cpp_type_p,
                                      void *value, cleanup_list *cleanup,
                                      bool cpp_delete) noexcept {
    return nb_abi->nb_type_put_unique_p(nb_abi_internals, cpp_type, cpp_type_p,
                                        value, cleanup, cpp_delete);
}

/// Check if the given python object represents a nanobind type
inline bool nb_type_check(PyObject *t) noexcept {
    return nb_abi->nb_type_check(nb_abi_internals, t);
}

/// Check if a Python type object wraps an instance of a specific C++ type
inline bool nb_type_isinstance(PyObject *obj, const std::type_info *t) noexcept {
    return nb_abi->nb_type_isinstance(nb_abi_internals, obj, t);
}

/// Search for the Python type object associated with a C++ type
inline PyObject *nb_type_lookup(const std::type_info *t) noexcept {
    return nb_abi->nb_type_lookup(nb_abi_internals, t);
}

// ========================================================================

// Create and install a Python property object
inline void property_install(PyObject *scope, const char *name,
                             PyObject *getter, PyObject *setter) noexcept {
    nb_abi->property_install(nb_abi_internals, scope, name, getter, setter);
}

inline void property_install_static(PyObject *scope, const char *name,
                                    PyObject *getter, PyObject *setter) noexcept {
    nb_abi->property_install_static(nb_abi_internals, scope, name, getter, setter);
}

// ========================================================================

// Ensure that 'patient' cannot be GCed while 'nurse' is alive
inline void keep_alive(PyObject *nurse, PyObject *patient) {
    nb_abi->keep_alive(nb_abi_internals, nurse, patient);
}

// Keep 'payload' alive until 'nurse' is GCed
inline void keep_alive(PyObject *nurse, void *payload,
                       void (*deleter)(void *) noexcept) noexcept {
    nb_abi->keep_alive_cb(nb_abi_internals, nurse, payload, deleter);
}


// ========================================================================

/// Indicate to nanobind that an implicit constructor can convert 'src' -> 'dst'
inline void implicitly_convertible(const std::type_info *src,
                                   const std::type_info *dst) noexcept {
    nb_abi->implicitly_convertible_cpp(nb_abi_internals, src, dst);
}

/// Register a callback to check if implicit conversion to 'dst' is possible
inline void implicitly_convertible(bool (*predicate)(nb_internals *,
                                                     PyTypeObject *,
                                                     PyObject *,
                                                     cleanup_list *),
                                   const std::type_info *dst) noexcept {
    nb_abi->implicitly_convertible_py(nb_abi_internals, predicate, dst);
}

// ========================================================================

/// Create a new enumeration type
inline PyObject *enum_create(enum_data_init *ed) noexcept {
    return nb_abi->enum_create(nb_abi_internals, ed);
}

// Query an enumeration's Python object -> integer value map
inline bool enum_from_python(const std::type_info *tp, PyObject *o, int64_t *out,
                             uint8_t flags) noexcept {
    return nb_abi->enum_from_python(nb_abi_internals, tp, o, out, flags);
}

// Query an enumeration's integer value -> Python object map
inline PyObject *enum_from_cpp(const std::type_info *tp, int64_t value) noexcept {
    return nb_abi->enum_from_cpp(nb_abi_internals, tp, value);
}

// ========================================================================

/// Wrap a ndarray_handle* into a PyCapsule
inline PyObject *ndarray_export(ndarray_handle *h, int framework,
                                rv_policy policy, cleanup_list *cleanup) noexcept {
    return nb_abi->ndarray_export(nb_abi_internals, h, framework, policy, cleanup);
}

// ========================================================================

typedef void (*exception_translator)(const std::exception_ptr &, void *);

inline void register_exception_translator(exception_translator translator,
                                          void *payload) {
    nb_abi->register_exception_translator(nb_abi_internals, translator, payload);
}

// ========================================================================

inline bool leak_warnings() noexcept {
    return nb_abi->leak_warnings(nb_abi_internals);
}
inline bool implicit_cast_warnings() noexcept {
    return nb_abi->implicit_cast_warnings(nb_abi_internals);
}
inline void set_leak_warnings(bool value) noexcept {
    nb_abi->set_leak_warnings(nb_abi_internals, value);
}
inline void set_implicit_cast_warnings(bool value) noexcept {
    nb_abi->set_implicit_cast_warnings(nb_abi_internals, value);
}

// ========================================================================

#if NB_TYPE_GET_SLOT_IMPL
inline void *type_get_slot(PyTypeObject *t, int slot_id) {
    return nb_abi->type_get_slot_impl(t, slot_id);
}
#endif

NB_INLINE PyObject *none_ref() noexcept { Py_RETURN_NONE; }
NB_INLINE PyObject *true_ref() noexcept { Py_RETURN_TRUE; }
NB_INLINE PyObject *false_ref() noexcept { Py_RETURN_FALSE; }

NAMESPACE_END(detail)

using detail::raise;
using detail::raise_type_error;
using detail::raise_python_error;

NAMESPACE_END(NB_NAMESPACE)
