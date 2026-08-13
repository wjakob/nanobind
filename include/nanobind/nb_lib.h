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

/// Tag distinguishing the exception kinds that a 'builtin_exception' (see
/// nb_error.h) carries, and the first parameter of 'raise_v' below. The
/// inline namespace keeps the mangled names of two coexisting majors
/// distinct.
inline namespace NB_BACKEND_ABI_NS {
enum class exception_type {
    runtime_error, stop_iteration, index_error, key_error, value_error,
    type_error, buffer_error, import_error, attribute_error, next_overload
};
}

NAMESPACE_BEGIN(detail)

// Forward declarations for types in ndarray.h (2)
struct ndarray_handle;
struct ndarray_config;
struct ndarray_create_args;

/// Backend configuration flags accessed via read_flag/write_flag below
enum class nb_flag : uint32_t {
    leak_warnings = 0,
    implicit_cast_warnings = 1
};

/**
 * Helper class to clean temporaries created by function dispatch.
 * The first element serves a special role: it stores the 'self'
 * object of method calls (for rv_policy::reference_internal).
 */
struct NB_CORE cleanup_list {
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
    void release() noexcept;

    /// Does the list contain any entries? (besides the 'self' argument)
    bool used() { return m_size != 1; }

    /// Return the size of the cleanup stack
    size_t size() const { return m_size; }

    /// Subscript operator
    PyObject *operator[](size_t index) const { return m_data[index]; }

protected:
    /// Out of memory, expand..
    void expand() noexcept;

protected:
    uint32_t m_size;
    uint32_t m_capacity;
    PyObject **m_data;
    PyObject *m_local[Small];
};

/* Both sides of the header/backend boundary create cleanup lists on the
   stack and operate on their fields inline, so the layout (including the
   inline array length) is frozen: any change is a major version bump. The
   overflow buffer must be allocated with PyMem_Malloc and freed with
   PyMem_Free, never malloc/free: a list may be grown in one binary and
   released in another, and only the libpython allocator is shared by every
   binary in the process. */
static_assert(cleanup_list::Small == 6 &&
              (sizeof(void *) != 8 || sizeof(cleanup_list) == 64),
              "frozen ABI layout of cleanup_list changed");

// ========================================================================

/// Raise a builtin_exception of the given kind (va_list core). C varargs
/// cannot be forwarded, so the variadic wrappers below hand off a va_list
/// and carry the [[noreturn]] and format attributes themselves.
NB_CORE void raise_v(exception_type type, const char *fmt, va_list args);

/// Raise a runtime error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, noinline, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]] NB_NOINLINE
#endif
inline void raise(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    raise_v(exception_type::runtime_error, fmt, args);
    va_end(args);
    NB_UNREACHABLE();
}

/// Raise a type error with the given message
#if defined(__GNUC__)
    __attribute__((noreturn, noinline, __format__ (__printf__, 1, 2)))
#else
    [[noreturn]] NB_NOINLINE
#endif
inline void raise_type_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    raise_v(exception_type::type_error, fmt, args);
    va_end(args);
    NB_UNREACHABLE();
}

/* Raise nanobind::python_error, resp. nanobind::cast_error when no Python
   error is pending. Defined in nb_error.h, which has the complete
   python_error type. */
[[noreturn]] NB_NOINLINE inline void raise_python_error();
[[noreturn]] NB_NOINLINE inline void raise_python_or_cast_error();

/// Raise an exception if 'p' is null (shared cold path of inline helpers)
NB_INLINE PyObject *raise_if_null(PyObject *p) {
    if (NB_UNLIKELY(!p))
        raise_python_error();
    return p;
}

/// Raise an exception if 'rv' is nonzero (ditto, for int-returning C API)
NB_INLINE void raise_if_nonzero(int rv) {
    if (NB_UNLIKELY(rv))
        raise_python_error();
}

NB_INLINE PyObject *none_ref() noexcept { Py_RETURN_NONE; }
NB_INLINE PyObject *true_ref() noexcept { Py_RETURN_TRUE; }
NB_INLINE PyObject *false_ref() noexcept { Py_RETURN_FALSE; }

// Backend implementation of the 'python_error' exception class
NB_CORE PyObject *error_fetch() noexcept;
NB_CORE PyObject *error_copy(PyObject *value, char **what) noexcept;
NB_CORE void error_release(PyObject *value, char *what) noexcept;
NB_CORE void error_restore(PyObject *value) noexcept;
NB_CORE const char *error_what(PyObject *value, char **what) noexcept;

// ========================================================================

NB_CORE PyObject *module_new(const char *name, const char *doc, void *exec,
                             uint32_t reserved) noexcept;
NB_CORE void nb_module_exec(const char *domain, PyObject *m);
NB_CORE void nb_module_free(void *m);

// ========================================================================

/// Convert a Python object into a Python unicode string
NB_CORE PyObject *str_from_obj(PyObject *o);

/// Convert an UTF8 null-terminated C string into a Python unicode string
NB_CORE PyObject *str_from_cstr(const char *c);

/// Convert an UTF8 C string + size into a Python unicode string
NB_CORE PyObject *str_from_cstr_and_size(const char *c, size_t n);

// ========================================================================

/// Convert a Python object into a Python byte string
NB_CORE PyObject *bytes_from_obj(PyObject *o);

/// Convert an UTF8 null-terminated C string into a Python byte string
NB_CORE PyObject *bytes_from_cstr(const char *c);

/// Convert a memory region into a Python byte string
NB_CORE PyObject *bytes_from_cstr_and_size(const void *c, size_t n);

// ========================================================================

/// Convert a Python object into a Python byte array
NB_CORE PyObject *bytearray_from_obj(PyObject *o);

/// Convert a memory region into a Python byte array
NB_CORE PyObject *bytearray_from_cstr_and_size(const void *c, size_t n);

// ========================================================================

/// Convert a Python object into a Python boolean object
inline PyObject *bool_from_obj(PyObject *o) {
    int rv = PyObject_IsTrue(o);
    if (NB_UNLIKELY(rv < 0))
        raise_python_error();
    return rv ? true_ref() : false_ref();
}

/// Convert a Python object into a Python integer object
NB_CORE PyObject *int_from_obj(PyObject *o);

/// Convert a Python object into a Python floating point object
NB_CORE PyObject *float_from_obj(PyObject *o);

// ========================================================================

/// Convert a Python object into a Python list
NB_CORE PyObject *list_from_obj(PyObject *o);

/// Convert a Python object into a Python tuple
NB_CORE PyObject *tuple_from_obj(PyObject *o);

/// Convert a Python object into a Python set
NB_CORE PyObject *set_from_obj(PyObject *o);

/// Convert a Python object into a Python frozenset
NB_CORE PyObject *frozenset_from_obj(PyObject *o);

/// Convert a Python object into a Python memoryview
NB_CORE PyObject *memoryview_from_obj(PyObject *o);

// ========================================================================

/// Get an object attribute or raise an exception
NB_CORE PyObject *getattr(PyObject *obj, const char *key);
NB_CORE PyObject *getattr(PyObject *obj, PyObject *key);

/// Get an object attribute or return a default value (never raises)
NB_CORE PyObject *getattr(PyObject *obj, const char *key, PyObject *def) noexcept;
NB_CORE PyObject *getattr(PyObject *obj, PyObject *key, PyObject *def) noexcept;

/// Set an object attribute or raise an exception
NB_CORE void setattr(PyObject *obj, const char *key, PyObject *value);
NB_CORE void setattr(PyObject *obj, PyObject *key, PyObject *value);

/// Delete an object attribute or raise an exception
NB_CORE void delattr(PyObject *obj, const char *key);
NB_CORE void delattr(PyObject *obj, PyObject *key);

// ========================================================================

/// Set an item or raise an exception
NB_CORE void setitem(PyObject *obj, Py_ssize_t, PyObject *value);
NB_CORE void setitem(PyObject *obj, const char *key, PyObject *value);
NB_CORE void setitem(PyObject *obj, PyObject *key, PyObject *value);

/// Delete an item or raise an exception
NB_CORE void delitem(PyObject *obj, Py_ssize_t);
NB_CORE void delitem(PyObject *obj, const char *key);
NB_CORE void delitem(PyObject *obj, PyObject *key);

/// Raise a KeyError for the given key (cold path of dict lookups)
[[noreturn]] NB_NOINLINE inline void raise_key_error(PyObject *key) {
    PyErr_SetObject(PyExc_KeyError, key);
    raise_python_error();
}

/// Look up 'k' in the dictionary 'd', returning a *new* reference
inline PyObject *dict_getitem_ref(PyObject *d, PyObject *k, bool *error) noexcept {
    PyObject *value;
#if NB_PYTHON_VERSION >= 0x030D0000
    *error = PyDict_GetItemRef(d, k, &value) == -1;
#else
    // GIL-protected borrowed-reference pattern; free-threaded builds never
    // land here (NB_FREE_THREADED implies 3.13+ headers)
    value = PyDict_GetItemWithError(d, k);
    if (value)
        Py_INCREF(value);
    *error = !value && PyErr_Occurred() != nullptr;
#endif
    return value;
}

/// Dict lookup that returns a default value for missing keys
inline PyObject *dict_getitem_or_default(PyObject *d, PyObject *k, PyObject *def) {
    bool error;
    PyObject *value = dict_getitem_ref(d, k, &error);
    if (NB_UNLIKELY(error))
        raise_python_error();
    if (!value) {
        Py_XINCREF(def);
        value = def;
    }
    return value;
}

/// Dict-specialized item access
NB_CORE void dict_setitem(PyObject *obj, PyObject *key, PyObject *value);
NB_CORE void dict_delitem(PyObject *obj, PyObject *key);

// ========================================================================

/// Try to roughly determine the length of a Python object
NB_CORE size_t obj_len_hint(PyObject *o) noexcept;

/// Obtain a string representation of a Python object
NB_CORE PyObject* obj_repr(PyObject *o);

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_1(PyObject *a, PyObject* (*op)(PyObject*));

/// Perform an unary operation on a Python object with error handling
NB_CORE PyObject *obj_op_2(PyObject *a, PyObject *b,
                           PyObject *(*op)(PyObject *, PyObject *));

// Perform a vector function call
NB_CORE PyObject *obj_vectorcall(PyObject *base, PyObject *const *args,
                                 size_t nargsf, PyObject *kwnames,
                                 bool method_call);

/// Create an iterator from 'o', raise an exception in case of errors
NB_CORE PyObject *obj_iter(PyObject *o);

/// Advance the iterator 'o', raise an exception in case of errors. A null
/// return without a pending error indicates exhaustion.
inline PyObject *obj_iter_next(PyObject *o) {
    PyObject *result = PyIter_Next(o);
    if (NB_UNLIKELY(!result && PyErr_Occurred()))
        raise_python_error();
    return result;
}

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

// If the given sequence has the size 'size', return a pointer to its contents.
// May produce a temporary.
NB_CORE PyObject **seq_get_with_size(PyObject *seq, size_t size,
                                     PyObject **temp) noexcept;

// Like the above, but return the size instead of checking it.
NB_CORE PyObject **seq_get(PyObject *seq, size_t *size,
                           PyObject **temp) noexcept;

// ========================================================================

// Forward declaration for type in nb_attr.h
struct func_data_init_base;

/// Create a Python function object for the given function record
NB_CORE PyObject *nb_func_new(const func_data_init_base *f) noexcept;

// ========================================================================

/// Create a Python type object for the given type record
struct type_data_init;
NB_CORE PyObject *nb_type_new(const type_data_init *c) noexcept;

/// Extract a pointer to a C++ type underlying a Python object, if possible
NB_CORE bool nb_type_get(const std::type_info *t, PyObject *o, uint32_t flags,
                         cleanup_list *cleanup, void **out) noexcept;

/// Cast a C++ type instance into a Python object. 'cpp_type_p' optionally
/// names the dynamic (most derived) type of polymorphic instances
NB_CORE PyObject *nb_type_put(const std::type_info *cpp_type,
                              const std::type_info *cpp_type_p, void *value,
                              rv_policy rvp, cleanup_list *cleanup,
                              bool *is_new = nullptr) noexcept;

// Special version of 'nb_type_put' for unique pointers and ownership transfer
NB_CORE PyObject *nb_type_put_unique(const std::type_info *cpp_type,
                                     const std::type_info *cpp_type_p,
                                     void *value, cleanup_list *cleanup,
                                     bool cpp_delete) noexcept;

/// Try to relinquish ownership from Python object to a unique_ptr;
/// return true if successful, false if not. (Failure is only
/// possible if `cpp_delete` is true.)
NB_CORE bool nb_type_relinquish_ownership(PyObject *o, bool cpp_delete) noexcept;

/// Reverse the effects of nb_type_relinquish_ownership().
NB_CORE void nb_type_restore_ownership(PyObject *o, bool cpp_delete) noexcept;

/// Get a pointer to a user-defined 'extra' value associated with the nb_type t.
NB_CORE void *nb_type_supplement(PyObject *t) noexcept;

/// Check if the given python object represents a nanobind type
NB_CORE bool nb_type_check(PyObject *t) noexcept;

/// Return the size of the type wrapped by the given nanobind type object
NB_CORE size_t nb_type_size(PyObject *t) noexcept;

/// Return the alignment of the type wrapped by the given nanobind type object
NB_CORE size_t nb_type_align(PyObject *t) noexcept;

/// Return a unicode string representing the long-form name of the given type
NB_CORE PyObject *nb_type_name(PyObject *t) noexcept;

/// Return a unicode string representing the long-form name of object's type
NB_CORE PyObject *nb_inst_name(PyObject *o) noexcept;

/// Return the C++ type_info wrapped by the given nanobind type object
NB_CORE const std::type_info *nb_type_info(PyObject *t) noexcept;

/// Get a pointer to the instance data of a nanobind instance (nb_inst)
NB_CORE void *nb_inst_ptr(PyObject *o) noexcept;

/// Check if a Python type object wraps an instance of a specific C++ type
NB_CORE bool nb_type_isinstance(PyObject *obj, const std::type_info *t) noexcept;

/// Search for the Python type object associated with a C++ type
NB_CORE PyObject *nb_type_lookup(const std::type_info *t) noexcept;

/// Allocate an instance of type 't'
NB_CORE PyObject *nb_inst_alloc(PyTypeObject *t);

/// Allocate an zero-initialized instance of type 't'
NB_CORE PyObject *nb_inst_alloc_zero(PyTypeObject *t);

/// Allocate an instance of type 't' referencing the existing 'ptr'
NB_CORE PyObject *nb_inst_reference(PyTypeObject *t, void *ptr,
                                    PyObject *parent);

/// Allocate an instance of type 't' taking ownership of the existing 'ptr'
NB_CORE PyObject *nb_inst_take_ownership(PyTypeObject *t, void *ptr);

/// Call the destructor of the given python object
NB_CORE void nb_inst_destruct(PyObject *o) noexcept;

/// Zero-initialize a POD type and mark it as ready + to be destructed upon GC
NB_CORE void nb_inst_zero(PyObject *o) noexcept;

/// Copy-construct 'dst' from 'src' and mark it as ready (both must share
/// one nb_type). An uninitialized 'dst' afterwards has its 'destruct' flag
/// set; a live 'dst' is destructed first and keeps its previous flag value
NB_CORE void nb_inst_copy(PyObject *dst, const PyObject *src) noexcept;

/// Analogous to 'nb_inst_copy', using the move constructor
NB_CORE void nb_inst_move(PyObject *dst, const PyObject *src) noexcept;

/// Check if a particular instance uses a Python-derived type
NB_CORE bool nb_inst_python_derived(PyObject *o) noexcept;

/// Overwrite the instance's ready/destruct flags
NB_CORE void nb_inst_set_state(PyObject *o, bool ready, bool destruct) noexcept;

/// Query the 'ready' and 'destruct' flags of an instance
NB_CORE std::pair<bool, bool> nb_inst_state_read(PyObject *o) noexcept;

// ========================================================================

// Create and install a Python property object
NB_CORE void property_install(PyObject *scope, const char *name,
                              PyObject *getter, PyObject *setter,
                              bool is_static) noexcept;

// ========================================================================

NB_CORE PyObject *get_override(void *ptr, const std::type_info *type,
                               const char *name, bool pure);

// ========================================================================

// Ensure that 'patient' cannot be GCed while 'nurse' is alive
NB_CORE void keep_alive(PyObject *nurse, PyObject *patient);

// Keep 'payload' alive until 'nurse' is GCed
NB_CORE void keep_alive(PyObject *nurse, void *payload,
                        void (*deleter)(void *) noexcept) noexcept;


// ========================================================================

/// Register an implicit conversion to 'dst'. 'src' is either a
/// 'const std::type_info *' naming the source type (is_predicate == false)
/// or a 'bool (*)(PyTypeObject *, PyObject *, cleanup_list *)' callback
/// that decides convertibility at runtime (is_predicate == true)
NB_CORE void implicitly_convertible(const std::type_info *dst, void *src,
                                    bool is_predicate) noexcept;

// ========================================================================

struct enum_data_init;

/// Create a new enumeration type
NB_CORE PyObject *enum_create(const enum_data_init *) noexcept;

/// Append an entry to an enumeration. For StrEnum members, 'str_value' carries
/// the string value; for all other enumerations it must be nullptr.
NB_CORE void enum_append(PyObject *tp, const char *name, int64_t value,
                         const char *str_value, const char *doc) noexcept;

// Query an enumeration's Python object -> integer value map
NB_CORE bool enum_from_python(const std::type_info *, PyObject *, int64_t *,
                              uint32_t flags) noexcept;

// Query an enumeration's integer value -> Python object map
NB_CORE PyObject *enum_from_cpp(const std::type_info *, int64_t) noexcept;

/// Export enum entries to the parent scope
NB_CORE void enum_export(PyObject *tp);

// ========================================================================

/// Try to import a Python extension module, raises an exception upon failure
NB_CORE PyObject *module_import(const char *name);

/// Try to import a Python extension module, raises an exception upon failure
NB_CORE PyObject *module_import(PyObject *name);

/// Create a submodule of an existing module
NB_CORE PyObject *module_new_submodule(PyObject *base, const char *name,
                                       const char *doc) noexcept;


// ========================================================================

// Try to import a reference-counted ndarray object via DLPack. The caller
// passes sizeof(ndarray_config) so that record can grow (see ndarray.h).
NB_CORE ndarray_handle *ndarray_import(PyObject *o,
                                       const ndarray_config *c,
                                       size_t config_size,
                                       bool convert,
                                       cleanup_list *cleanup) noexcept;

// Describe a local ndarray object using a DLPack capsule. The caller
// passes sizeof(ndarray_create_args) for the same reason.
NB_CORE ndarray_handle *ndarray_create(const ndarray_create_args *a,
                                       size_t args_size);

/// Increase the reference count of the given ndarray object; returns a pointer
/// to the underlying DLTensor
NB_CORE dlpack::dltensor *ndarray_inc_ref(ndarray_handle *) noexcept;

/// Decrease the reference count of the given ndarray object
NB_CORE void ndarray_dec_ref(ndarray_handle *) noexcept;

/// Wrap a ndarray_handle* into a PyCapsule
NB_CORE PyObject *ndarray_export(ndarray_handle *, int framework,
                                 rv_policy policy, cleanup_list *cleanup) noexcept;

/// Check if an object represents an ndarray
NB_CORE bool ndarray_check(PyObject *o) noexcept;

// ========================================================================

typedef void (*exception_translator)(const std::exception_ptr &, void *);

NB_CORE void register_exception_translator(exception_translator translator,
                                           void *payload);

NB_CORE PyObject *exception_new(PyObject *mod, const char *name,
                                PyObject *base);

// ========================================================================

NB_CORE bool load_i8 (PyObject *o, uint32_t flags, int8_t *out) noexcept;
NB_CORE bool load_u8 (PyObject *o, uint32_t flags, uint8_t *out) noexcept;
NB_CORE bool load_i16(PyObject *o, uint32_t flags, int16_t *out) noexcept;
NB_CORE bool load_u16(PyObject *o, uint32_t flags, uint16_t *out) noexcept;
NB_CORE bool load_i32(PyObject *o, uint32_t flags, int32_t *out) noexcept;
NB_CORE bool load_u32(PyObject *o, uint32_t flags, uint32_t *out) noexcept;
NB_CORE bool load_i64(PyObject *o, uint32_t flags, int64_t *out) noexcept;
NB_CORE bool load_u64(PyObject *o, uint32_t flags, uint64_t *out) noexcept;
NB_CORE bool load_f32(PyObject *o, uint32_t flags, float *out) noexcept;
NB_CORE bool load_f64(PyObject *o, uint32_t flags, double *out) noexcept;

// ========================================================================

/// PyGILState_Check() for TUs that cannot call it (limited API)
NB_CORE bool gil_check() noexcept;

/// Cold path of the GIL assertion in handle::inc_ref/dec_ref (debug builds)
[[noreturn]] NB_NOINLINE inline void fail_gil() noexcept {
    fprintf(stderr, "Critical nanobind error: attempted to change the "
                    "reference count of a Python object while the GIL was "
                    "not held!\n");
    abort();
}

// ========================================================================

/// Read/write a backend configuration flag (see the 'nb_flag' enumeration)
NB_CORE uint32_t read_flag(nb_flag f) noexcept;
NB_CORE void write_flag(nb_flag f, uint32_t value) noexcept;

// ========================================================================

/// Check whether the object can be iterated over (see nb::iterable)
inline bool iterable_check(PyObject *o) noexcept {
    PyTypeObject *tp = Py_TYPE(o);
#if !defined(Py_LIMITED_API)
    bool has_iter = tp->tp_iter != nullptr;
#else
    bool has_iter = PyType_GetSlot(tp, Py_tp_iter) != nullptr;
#endif
    return has_iter || PySequence_Check(o);
}

// ========================================================================

NB_CORE void slice_compute(PyObject *slice, Py_ssize_t size,
                           Py_ssize_t &start, Py_ssize_t &stop,
                           Py_ssize_t &step, size_t &slice_length);

// ========================================================================

/// Python issubclass() check with error handling
inline bool issubclass(PyObject *a, PyObject *b) {
    int rv = PyObject_IsSubclass(a, b);
    if (NB_UNLIKELY(rv < 0))
        raise_python_error();
    return bool(rv);
}

// ========================================================================

NB_CORE bool is_alive() noexcept;

#if NB_TYPE_GET_SLOT_IMPL
NB_CORE void *type_get_slot(PyTypeObject *t, int slot_id);
#endif

NAMESPACE_END(detail)

using detail::raise;
using detail::raise_type_error;
using detail::raise_python_error;

NAMESPACE_END(NB_NAMESPACE)
