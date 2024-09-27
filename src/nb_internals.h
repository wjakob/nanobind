#pragma once

#if defined(__GNUC__)
// Don't warn about missing fields in PyTypeObject declarations
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(_MSC_VER)
// Silence warnings that MSVC reports in robin_*.h
#  pragma warning(disable: 4127) // conditional expression is constant
#  pragma warning(disable: 4324) // structure was padded due to alignment specifier
#  pragma warning(disable: 4293) // shift count negative or too big  <-- erroneously raised in a constexpr-disabled block
#  pragma warning(disable: 4310) // cast truncates constant value <-- erroneously raised in a constexpr-disabled block
#endif

#include <nanobind/nanobind.h>
#include <tsl/robin_map.h>
#include <cstring>
#include <string_view>
#include <functional>
#include "hash.h"

#if TSL_RH_VERSION_MAJOR != 1 || TSL_RH_VERSION_MINOR < 3
#  error nanobind depends on tsl::robin_map, in particular version >= 1.3.0, <2.0.0
#endif

#if defined(_MSC_VER)
#  define NB_THREAD_LOCAL __declspec(thread)
#else
#  define NB_THREAD_LOCAL __thread
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#if defined(NB_COMPACT_ASSERTIONS)
[[noreturn]] extern void fail_unspecified() noexcept;
#  define check(cond, ...) if (NB_UNLIKELY(!(cond))) nanobind::detail::fail_unspecified()
#else
#  define check(cond, ...) if (NB_UNLIKELY(!(cond))) nanobind::detail::fail(__VA_ARGS__)
#endif

/// Nanobind function metadata (overloads, etc.)
struct func_data : func_data_prelim<0> {
    arg_data *args;
    char *signature;
};

/// Python object representing an instance of a bound C++ type
struct nb_inst { // usually: 24 bytes
    PyObject_HEAD

    /// Offset to the actual instance data
    int32_t offset;

    /// State of the C++ object this instance points to: is it constructed?
    /// can we use it?
    uint32_t state : 2;

    // Values for `state`. Note that the numeric values of these are relied upon
    // for an optimization in `nb_type_get()`.
    static constexpr uint32_t state_uninitialized = 0; // not constructed
    static constexpr uint32_t state_relinquished = 1; // owned by C++, don't touch
    static constexpr uint32_t state_ready = 2; // constructed and usable

    /**
     * The variable 'offset' can either encode an offset relative to the
     * nb_inst address that leads to the instance data, or it can encode a
     * relative offset to a pointer that must be dereferenced to get to the
     * instance data. 'direct' is 'true' in the former case.
     */
    uint32_t direct : 1;

    /// Is the instance data co-located with the Python object?
    uint32_t internal : 1;

    /// Should the destructor be called when this instance is GCed?
    uint32_t destruct : 1;

    /// Should nanobind call 'operator delete' when this instance is GCed?
    uint32_t cpp_delete : 1;

    /// Does this instance hold references to others? (via internals.keep_alive)
    uint32_t clear_keep_alive : 1;

    /// Does this instance use intrusive reference counting?
    uint32_t intrusive : 1;

    // That's a lot of unused space. I wonder if there is a good use for it..
    uint32_t unused : 24;
};

static_assert(sizeof(nb_inst) == sizeof(PyObject) + sizeof(uint32_t) * 2);

/// Python object representing a bound C++ function
struct nb_func {
    PyObject_VAR_HEAD
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
    uint32_t max_nargs; // maximum value of func_data::nargs for any overload
    bool complex_call;
    bool doc_uniform;
};

/// Python object representing a `nb_ndarray` (which wraps a DLPack ndarray)
struct nb_ndarray {
    PyObject_HEAD
    ndarray_handle *th;
};

/// Python object representing an `nb_method` bound to an instance (analogous to non-public PyMethod_Type)
struct nb_bound_method {
    PyObject_HEAD
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
    nb_func *func;
    PyObject *self;
};

/// Pointers require a good hash function to randomize the mapping to buckets
struct ptr_hash {
    size_t operator()(const void *p) const {
        // fmix32/64 from MurmurHash by Austin Appleby (public domain)
        if constexpr (sizeof(void *) == 4)
            return (size_t) fmix32((uint32_t) (uintptr_t) p);
        else
            return (size_t) fmix64((uint64_t) (uintptr_t) p);
    }
};

// Minimal allocator definition, contains only the parts needed by tsl::*
template <typename T> class py_allocator {
public:
    using value_type = T;
    using pointer = T *;
    using size_type = std::size_t;

    py_allocator() = default;
    py_allocator(const py_allocator &) = default;

    template <typename U> py_allocator(const py_allocator<U> &) { }

    pointer allocate(size_type n, const void * /*hint*/ = nullptr) noexcept {
        void *p = PyMem_Malloc(n * sizeof(T));
        if (!p)
            fail("PyMem_Malloc(): out of memory!");
        return static_cast<pointer>(p);
    }

    void deallocate(T *p, size_type /*n*/) noexcept { PyMem_Free(p); }
};

// Linked list of instances with the same pointer address. Usually just 1.
struct nb_inst_seq {
    PyObject *inst;
    nb_inst_seq *next;
};

// Linked list of type aliases when there are multiple shared libraries with duplicate RTTI data
struct nb_alias_chain {
    const std::type_info *value;
    nb_alias_chain *next;
};

// Weak reference list. Usually, there is just one entry
struct nb_weakref_seq {
    void (*callback)(void *) noexcept;
    void *payload;
    nb_weakref_seq *next;
};

struct std_typeinfo_hash {
    size_t operator()(const std::type_info *a) const {
        const char *name = a->name();
        return std::hash<std::string_view>()({name, strlen(name)});
    }
};

struct std_typeinfo_eq {
    bool operator()(const std::type_info *a, const std::type_info *b) const {
        return a->name() == b->name() || strcmp(a->name(), b->name()) == 0;
    }
};

using nb_type_map_fast = tsl::robin_map<const std::type_info *, type_data *, ptr_hash>;
using nb_type_map_slow = tsl::robin_map<const std::type_info *, type_data *,
                                        std_typeinfo_hash, std_typeinfo_eq>;

/// A simple pointer-to-pointer map that is reused a few times below (even if
/// not 100% ideal) to avoid template code generation bloat.
using nb_ptr_map  = tsl::robin_map<void *, void*, ptr_hash>;

/// Convenience functions to deal with the pointer encoding in 'internals.inst_c2p'

/// Does this entry store a linked list of instances?
NB_INLINE bool         nb_is_seq(void *p)   { return ((uintptr_t) p) & 1; }

/// Tag a nb_inst_seq* pointer as such
NB_INLINE void*        nb_mark_seq(void *p) { return (void *) (((uintptr_t) p) | 1); }

/// Retrieve the nb_inst_seq* pointer from an 'inst_c2p' value
NB_INLINE nb_inst_seq* nb_get_seq(void *p)  { return (nb_inst_seq *) (((uintptr_t) p) ^ 1); }

struct nb_translator_seq {
    exception_translator translator;
    void *payload;
    nb_translator_seq *next = nullptr;
};

#if defined(NB_FREE_THREADED)
#  define NB_SHARD_ALIGNMENT alignas(64)
#else
#  define NB_SHARD_ALIGNMENT
#endif

/**
 * The following data structure stores information associated with individual
 * instances. In free-threaded builds, it is split into multiple shards to avoid
 * lock contention.
 */
struct NB_SHARD_ALIGNMENT nb_shard {
    /**
     * C++ -> Python instance map
     *
     * This associative data structure maps a C++ instance pointer onto its
     * associated PyObject* (if bit 0 of the map value is zero) or a linked
     * list of type `nb_inst_seq*` (if bit 0 is set---it must be cleared before
     * interpreting the pointer in this case).
     *
     * The latter case occurs when several distinct Python objects reference
     * the same memory address (e.g. a struct and its first member).
     */
    nb_ptr_map inst_c2p;

    /// Dictionary storing keep_alive references
    nb_ptr_map keep_alive;

#if defined(NB_FREE_THREADED)
    PyMutex mutex { };
#endif
};

/**
 * `nb_internals` is the central data structure storing information related to
 * function/type bindings and instances. Separate nanobind extensions within the
 * same NB_DOMAIN furthermore share `nb_internals` to communicate with each
 * other, hence any changes here generally require an ABI version bump.
 *
 * The GIL protects the elements of this data structure from concurrent
 * modification. In free-threaded builds, a combination of locking schemes is
 * needed to achieve good performance.
 *
 * In particular, `inst_c2p` and `type_c2p_fast` are very hot and potentially
 * accessed several times while dispatching a single function call. The other
 * elements are accessed much less frequently and easier to deal with.
 *
 * The following list clarifies locking semantics for each member.
 *
 * - `nb_module`, `nb_meta`, `nb_func`, `nb_method`, `nb_bound_method`,
 *   `*_Type_tp_*`, `shard_count`, `is_alive_ptr`: these are initialized when
 *   loading the first nanobind extension within a domain, which happens within
 *   a critical section. They do not require locking.
 *
 * - `nb_type_dict`: created when the loading the first nanobind extension
 *   within a domain. While the dictionary itself is protected by its own
 *   lock, additional locking is needed to avoid races that create redundant
 *   entries. The `mutex` member is used for this.
 *
 * - `nb_static_property` and `nb_static_propert_descr_set`: created only once
 *   on demand, protected by `mutex`.
 *
 * - `nb_static_property_disabled`: needed to correctly implement assignments to
 *   static properties. Free-threaded builds store this flag using TLS to avoid
 *   concurrent modification.
 *
 * - `nb_static_property` and `nb_static_propert_descr_set`: created only once
 *   on demand, protected by `mutex`.
 *
 * - `nb_ndarray`: created only once on demand, protected by `mutex`.
 *
 * - `inst_c2p`: stores the C++ instance to Python object mapping. This
 *   data struture is *hot* and uses a sharded locking scheme to reduce
 *   lock contention.
 *
 * - `keep_alive`: stores lifetime dependencies (e.g., from the
 *   reference_internal return value policy). This data structure is
 *   potentially hot and shares the sharding scheme of `inst_c2p`.
 *
 * - `type_c2p_slow`: This is the ground-truth source of the `std::type_info`
 *   to `type_info *` mapping. Unrelated to free-threading, lookups into this
 *   data struture are generally costly because they use a string comparison on
 *   some platforms. Because it is only used as a fallback for 'type_c2p_fast',
 *   protecting this member via the global `mutex` is sufficient.
 *
 * - `type_c2p_fast`: this data structure is *hot* and mostly read. It maps
 *   `std::type_info` to `type_info *` but uses pointer-based comparisons.
 *   The implementation depends on the Python build.
 *
 * - `translators`: This is an append-to-front-only singly linked list traversed
 *    while raising exceptions. The main concern is losing elements during
 *    concurrent append operations. We assume that this data structure is only
 *    written during module initialization and don't use locking.
 *
 * - `funcs`: data structure for function leak tracking. Not used in
 *   free-threaded mode .
 *
 * - `print_leak_warnings`, `print_implicit_cast_warnings`: simple boolean
 *   flags. No protection against concurrent conflicting updates.
 */

struct nb_internals {
    /// Internal nanobind module
    PyObject *nb_module;

    /// Meta-metaclass of nanobind instances
    PyTypeObject *nb_meta;

    /// Dictionary with nanobind metaclass(es) for different payload sizes
    PyObject *nb_type_dict;

    /// Types of nanobind functions and methods
    PyTypeObject *nb_func, *nb_method, *nb_bound_method;

    /// Property variant for static attributes (created on demand)
    PyTypeObject *nb_static_property = nullptr;
    descrsetfunc nb_static_property_descr_set = nullptr;

#if defined(NB_FREE_THREADED)
    Py_tss_t *nb_static_property_disabled = nullptr;
#else
    bool nb_static_property_disabled = false;
#endif

    /// N-dimensional array wrapper (created on demand)
    PyTypeObject *nb_ndarray = nullptr;

#if defined(NB_FREE_THREADED)
    nb_shard *shards = nullptr;
    size_t shard_mask = 0;

    // Heuristic shard selection (from pybind11 PR #5148 by @colesbury), uses
    // high pointer bits to group allocations by individual threads/cores.
    inline nb_shard &shard(void *p) {
        uintptr_t highbits = ((uintptr_t) p) >> 20;
        size_t index = ((size_t) fmix64((uint64_t) highbits)) & shard_mask;
        return shards[index];
    }
#else
    nb_shard shards[1];
    inline nb_shard &shard(void *) { return shards[0]; }
#endif

#if !defined(NB_FREE_THREADED)
    /// C++ -> Python type map -- fast version based on std::type_info pointer equality
    nb_type_map_fast type_c2p_fast;
#endif

    /// C++ -> Python type map -- slow fallback version based on hashed strings
    nb_type_map_slow type_c2p_slow;

#if !defined(NB_FREE_THREADED)
    /// nb_func/meth instance map for leak reporting (used as set, the value is unused)
    /// In free-threaded mode, functions are immortal and don't require this data structure.
    nb_ptr_map funcs;
#endif

    /// Registered C++ -> Python exception translators
    nb_translator_seq translators;

    /// Should nanobind print leak warnings on exit?
    bool print_leak_warnings = true;

    /// Should nanobind print warnings after implicit cast failures?
    bool print_implicit_cast_warnings = true;

    /// Pointer to a boolean that denotes if nanobind is fully initialized.
    bool *is_alive_ptr = nullptr;

#if defined(Py_LIMITED_API)
    // Cache important functions from PyType_Type and PyProperty_Type
    freefunc PyType_Type_tp_free;
    initproc PyType_Type_tp_init;
    destructor PyType_Type_tp_dealloc;
    setattrofunc PyType_Type_tp_setattro;
    descrgetfunc PyProperty_Type_tp_descr_get;
    descrsetfunc PyProperty_Type_tp_descr_set;
    size_t type_data_offset;
#endif

#if defined(NB_FREE_THREADED)
    PyMutex mutex { };
#endif

    // Size of the 'shards' data structure. Only rarely accessed, hence at the end
    size_t shard_count = 1;
};

/// Convenience macro to potentially access cached functions
#if defined(Py_LIMITED_API)
#  define NB_SLOT(type, name) internals->type##_##name
#else
#  define NB_SLOT(type, name) type.name
#endif

extern nb_internals *internals;
extern PyTypeObject *nb_meta_cache;

extern char *type_name(const std::type_info *t);

// Forward declarations
extern PyObject *inst_new_ext(PyTypeObject *tp, void *value);
extern PyObject *inst_new_int(PyTypeObject *tp, PyObject *args, PyObject *kwds);
extern PyTypeObject *nb_static_property_tp() noexcept;
extern type_data *nb_type_c2p(nb_internals *internals,
                              const std::type_info *type);
extern void nb_type_unregister(type_data *t) noexcept;

extern PyObject *call_one_arg(PyObject *fn, PyObject *arg) noexcept;

/// Fetch the nanobind function record from a 'nb_func' instance
NB_INLINE func_data *nb_func_data(void *o) {
    return (func_data *) (((char *) o) + sizeof(nb_func));
}

#if defined(Py_LIMITED_API)
extern type_data *nb_type_data_static(PyTypeObject *o) noexcept;
#endif

/// Fetch the nanobind type record from a 'nb_type' instance
NB_INLINE type_data *nb_type_data(PyTypeObject *o) noexcept{
    #if !defined(Py_LIMITED_API)
        return (type_data *) (((char *) o) + sizeof(PyHeapTypeObject));
    #else
        return nb_type_data_static(o);
    #endif
}

extern PyObject *nb_type_name(PyObject *o) noexcept;

inline void *inst_ptr(nb_inst *self) {
    void *ptr = (void *) ((intptr_t) self + self->offset);
    return self->direct ? ptr : *(void **) ptr;
}

template <typename T> struct scoped_pymalloc {
    scoped_pymalloc(size_t size = 1) {
        ptr = (T *) PyMem_Malloc(size * sizeof(T));
        if (!ptr)
            fail("scoped_pymalloc(): could not allocate %zu bytes of memory!", size);
    }
    ~scoped_pymalloc() { PyMem_Free(ptr); }
    T *release() {
        T *temp = ptr;
        ptr = nullptr;
        return temp;
    }
    T *get() const { return ptr; }
    T &operator[](size_t i) { return ptr[i]; }
    T *operator->() { return ptr; }
private:
    T *ptr{ nullptr };
};


/// RAII lock/unlock guards for free-threaded builds
#if defined(NB_FREE_THREADED)
struct lock_shard {
    nb_shard &s;
    lock_shard(nb_shard &s) : s(s) { PyMutex_Lock(&s.mutex); }
    ~lock_shard() { PyMutex_Unlock(&s.mutex); }
};
struct lock_internals {
    nb_internals *i;
    lock_internals(nb_internals *i) : i(i) { PyMutex_Lock(&i->mutex); }
    ~lock_internals() { PyMutex_Unlock(&i->mutex); }
};
struct unlock_internals {
    nb_internals *i;
    unlock_internals(nb_internals *i) : i(i) { PyMutex_Unlock(&i->mutex); }
    ~unlock_internals() { PyMutex_Lock(&i->mutex); }
};
#else
struct lock_shard { lock_shard(nb_shard &) { } };
struct lock_internals { lock_internals(nb_internals *) { } };
struct unlock_internals { unlock_internals(nb_internals *) { } };
struct lock_obj { lock_obj(PyObject *) { } };
#endif

extern char *strdup_check(const char *);
extern void *malloc_check(size_t size);
extern void maybe_make_immortal(PyObject *op);

extern char *extract_name(const char *cmd, const char *prefix, const char *s);


NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
