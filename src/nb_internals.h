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
#if defined(NB_FREE_THREADED)
#  include <atomic>
#if !defined(_WIN32)
#  include <pthread.h>
#endif
#endif
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

// When forwarding vector calls between functions that are known to be implemented by
// nanobind, it uses an extended ABI that may set one additional bit to communicate
// that the implicit 'self' argument is trusted and does not need to be type-checked.
#define NB_VECTORCALL_TRUSTED_SELF (PY_VECTORCALL_ARGUMENTS_OFFSET >> 1)

// Decodes the call argument count to avoid all use of ``PyVectorcall_NARGS()``
// in nanobind. The an official function requires a (costly) indirect PLT call
// in the stable ABI, which is unnecessary as its behavior is fully frozen by
// the stable ABI contract.
#define NB_VECTORCALL_NARGS(n)                                                  \
    ((Py_ssize_t) ((n) & ~(PY_VECTORCALL_ARGUMENTS_OFFSET |                     \
                           NB_VECTORCALL_TRUSTED_SELF)))

#if PY_VERSION_HEX >= 0x030A0000
#  define NB_TPFLAGS_IMMUTABLETYPE Py_TPFLAGS_IMMUTABLETYPE
#else
#  define NB_TPFLAGS_IMMUTABLETYPE 0
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#if defined(NB_COMPACT_ASSERTIONS)
[[noreturn]] extern void fail_unspecified() noexcept;
#  define check(cond, ...) if (NB_UNLIKELY(!(cond))) nanobind::detail::fail_unspecified()
#else
#  define check(cond, ...) if (NB_UNLIKELY(!(cond))) nanobind::detail::fail(__VA_ARGS__)
#endif

struct nb_alias_chain;
struct nb_inst;
struct nb_internals;

/// LIFO Instance pool
struct nb_inst_pool {
    nb_inst **slots;
    uint32_t count;
    uint32_t capacity;
};

// Implicit conversions for C++ type bindings, used in type_data below
struct implicit_t {
    const std::type_info **cpp;
    bool (**py)(PyTypeObject *, PyObject *, cleanup_list *) noexcept;
};

// Forward and reverse mappings for enumerations, used in type_data below
struct enum_tbl_t {
    void *fwd;
    void *rev;
};

/// Backend-private storage describing a type throughout its lifetime. The
/// construction inputs arrive via the frozen 'type_data_init' record; the
/// remaining fields are filled in by nb_type_new() and friends.
struct type_data {
    uint32_t size;
    uint32_t align;
    uint32_t flags;
    const char *name;
    const std::type_info *type;
    PyTypeObject *type_py;
    nb_alias_chain *alias_chain;
#if defined(Py_LIMITED_API)
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
#endif
    void *init; // Constructor nb_func
    void (*destruct)(void *);
    void (*copy)(void *, const void *);
    void (*move)(void *, void *) noexcept;
    union {
        implicit_t implicit;  // for C++ type bindings
        enum_tbl_t enum_tbl;  // for enumerations
    };
    void (*set_self_py)(void *, PyObject *) noexcept;
    bool (*keep_shared_from_this_alive)(PyObject *) noexcept;
    uint32_t dictoffset;
    uint32_t weaklistoffset;
    /// Out-of-line heap storage for an optional nb::supplement<T>
    void *supplement;
    /// Owning domain. Lets interpreter-initiated callbacks (tp_dealloc, ...)
    /// reach the internals without a global, via nb_type_data(type)->internals.
    nb_internals *internals;
    /// Instance pool capacity
    uint32_t pool_capacity;
#if defined(NB_FREE_THREADED)
    /// Slot of this type's pool in the packed per-thread pool array
    uint32_t pool_index;
#else
    /// Per-type instance pool for non-FT builds
    nb_inst_pool pool;
#endif
};

/// Low bits copied from public construction/enum records. For type bindings,
/// init-only type_init_flags are stripped before storing the flags here.
static constexpr uint32_t type_data_public_flag_mask = (1u << 24) - 1u;
static constexpr uint32_t type_data_construction_flag_mask =
    type_data_public_flag_mask & ~(uint32_t) type_init_flags::all_init_flags;

/// Runtime-only type flags owned by the backend. They start above the 24-bit
/// public 'type_data_init::flags' field so the public construction flag namespace
/// keeps its reserved headroom. They are set while creating/using a type; binding
/// code never supplies them, and they never appear in a 'type_data_init' record.
enum class type_flags_internal : uint32_t {
    /// Cached copy of Py_TPFLAGS_HAVE_GC
    has_gc                   = (1u << 24),

    /// Does the type maintain a list of implicit conversions?
    has_implicit_conversions = (1u << 25),

    /// Is this a Python type that extends a bound C++ type?
    is_python_type           = (1u << 26),

    /// Does the type implement a custom __new__ operator?
    has_new                  = (1u << 27),

    /// Does the type implement a custom __new__ operator that can take no args
    /// (except the type object)?
    has_nullary_new          = (1u << 28)
};

/// Nanobind function metadata (overloads, etc.)
struct func_data : func_data_init_base {
    arg_data_init *args;
    char *signature;
    /// Owning domain. Lets interpreter-initiated callbacks (tp_dealloc, the
    /// vectorcall path, ...) reach the internals via nb_func_data(self)->internals.
    nb_internals *internals;
};

/// Packed status of a nanobind type instance.
struct nb_inst_state {
    // Values for the 'state' field. Note that the numeric values of these are
    // relied upon for an optimization in `nb_type_get()`.
    static constexpr uint32_t state_uninitialized = 0; // not constructed
    static constexpr uint32_t state_relinquished = 1; // owned by C++, don't touch
    static constexpr uint32_t state_ready = 2; // constructed and usable

    /// State of the C++ object this instance points to: is it constructed?
    /// can we use it? (see the 'state_*' values below)
    uint8_t state : 2;

    /**
     * The variable 'offset' can either encode an offset relative to the
     * nb_inst address that leads to the instance data, or it can encode a
     * relative offset to a pointer that must be dereferenced to get to the
     * instance data. 'direct' is 'true' in the former case.
     */
    uint8_t direct : 1;

    /// Is the instance data co-located with the Python object?
    uint8_t internal : 1;

    /// Should the destructor be called when this instance is GCed?
    uint8_t destruct : 1;

    /// Should nanobind call 'operator delete' when this instance is GCed?
    uint8_t cpp_delete : 1;

    /// Does this instance use intrusive reference counting?
    uint8_t intrusive : 1;

    /// Currently not used (but needed to pad to 8 bit)
    uint8_t pad : 1;

    /// Does this instance hold references to others? (via internals.keep_alive)
    /// This may be accessed concurrently to the flag byte above, so it is kept
    /// in its own byte (never read-modify-written together with the flags).
    uint8_t clear_keep_alive;

    // That's a lot of unused space. I wonder if there is a good use for it..
    uint16_t unused;
};

static_assert(sizeof(nb_inst_state) == sizeof(uint32_t));

/// Python object representing an instance of a bound C++ type
struct nb_inst { // usually: 24 bytes
    PyObject_HEAD

    /// Offset to the actual instance data
    int32_t offset;

    /// Packed status flags (see nb_inst_state)
    nb_inst_state state;
};

static_assert(sizeof(nb_inst) == sizeof(PyObject) + sizeof(uint32_t) * 2);

/// Helper to ensure that nb_inst instance state updates produce one 4-byte store
inline void nb_inst_state_write(nb_inst *self, nb_inst_state state) noexcept {
    uint32_t w;
    std::memcpy(&w, &state, sizeof(w));
    std::memcpy(&self->state, &w, sizeof(w));
}

/// Dispatcher needed by an overload chain; chain merging takes the maximum
enum class call_complexity : uint8_t {
    /// No named/default/flagged arguments: nb_func_vectorcall_simple*
    simple = 0,

    /// Named/default/'none'-accepting args or arg-mutating annotations;
    /// keyword calls are forwarded to the complex dispatcher
    medium = 1,

    /// nb::args/nb::kwargs or more than NB_MAXARGS_SIMPLE arguments
    complex = 2
};

/// Python object representing a bound C++ function
struct nb_func {
    PyObject_VAR_HEAD
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
    uint32_t max_nargs; // maximum value of func_data::nargs for any overload
    call_complexity complexity;
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
        if (NB_UNLIKELY(n > SIZE_MAX / sizeof(T)))
            fail("py_allocator::allocate(): integer overflow!");
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

/// A simple pointer-to-pointer map that is reused a few times below (even if
/// not 100% ideal) to avoid template code generation bloat.
using nb_ptr_map  = tsl::robin_map<void *, void*, ptr_hash>;

using nb_type_map_fast = nb_ptr_map;
using nb_type_map_slow = tsl::robin_map<const std::type_info *, type_data *,
                                        std_typeinfo_hash, std_typeinfo_eq>;

#if defined(NB_FREE_THREADED)
// Per-thread state
struct nb_thread_state {
    // C++ -> Python type cache
    nb_type_map_fast type_c2p_fast;

    /// Per-thread instance pools indexed by ``type_data::pool_index``
    /// Grown lazily by nb_pool_ensure() and freed when the thread exists
    nb_inst_pool *pools = nullptr;

    /// Number of entries currently allocated in ``pools``
    uint32_t pools_size = 0;
};

extern NB_THREAD_LOCAL nb_thread_state *nb_thread_state_tls;

/// Slow path: allocate this thread's state and register a cleanup routine
extern nb_thread_state *nb_thread_state_alloc() noexcept;

NB_INLINE nb_thread_state *nb_thread_state_get() noexcept {
    nb_thread_state *ts = nb_thread_state_tls;
    if (NB_UNLIKELY(!ts))
        ts = nb_thread_state_alloc();
    return ts;
}
#endif

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
 * Wraps a std::atomic if free-threading is enabled, otherwise a raw value.
 */
#if defined(NB_FREE_THREADED)
template<typename T>
struct nb_maybe_atomic {
  nb_maybe_atomic(T v = T()) : value(v) {}

  std::atomic<T> value;
  T load_acquire() { return value.load(std::memory_order_acquire); }
  T load_relaxed() { return value.load(std::memory_order_relaxed); }
  void store_release(T w) { value.store(w, std::memory_order_release); }
};
#else
template<typename T>
struct nb_maybe_atomic {
  nb_maybe_atomic(T v = T()) : value(v) {}

  T value;
  T load_acquire() { return value; }
  T load_relaxed() { return value; }
  void store_release(T w) { value = w; }
};
#endif

/// Cache slots for `nb_internals::ndarray_export`: cached callables that build a
/// framework's array from nanobind's DLPack/buffer wrapper.
enum ndarray_export_slot {
    nd_export_numpy_view, // numpy.asarray
    nd_export_numpy_copy, // numpy.copy
    nd_export_pytorch,    // torch.utils.dlpack.from_dlpack
    nd_export_tensorflow, // tensorflow.experimental.dlpack.from_dlpack
    nd_export_jax,        // jax.dlpack.from_dlpack
    nd_export_cupy,       // cupy.from_dlpack
    nd_export_mlx,        // mlx.core.array (constructor, not from_dlpack)
    nd_export_count
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
 * - `nb_module`, `nb_type`, `nb_func`, `nb_method`, `nb_bound_method`,
 *   `*_Type_tp_*`, `shard_count`, `is_alive_ptr`: these are initialized when
 *   loading the first nanobind extension within a domain, which happens within
 *   a critical section. They do not require locking.
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

    /// The metaclass shared by every bound type
    PyTypeObject *nb_type;

    /// Types of nanobind functions and methods
    PyTypeObject *nb_func, *nb_method, *nb_bound_method;

    /// Property variant for static attributes (created on demand)
    nb_maybe_atomic<PyTypeObject *> nb_static_property = nullptr;
    descrsetfunc nb_static_property_descr_set = nullptr;

#if defined(NB_FREE_THREADED)
    Py_tss_t *nb_static_property_disabled = nullptr;
#else
    bool nb_static_property_disabled = false;
#endif

    /// N-dimensional array wrapper (created on demand)
    nb_maybe_atomic<PyTypeObject *> nb_ndarray = nullptr;

    /// Cached callables used to export an ndarray to a framework, indexed by
    /// `ndarray_export_slot`.
    nb_maybe_atomic<PyObject *> ndarray_export[nd_export_count] {};

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

#if defined(NB_FREE_THREADED)
    // Per-domain key for reclaiming nb_thread_state at thread exit
#  if defined(_WIN32)
    unsigned long thread_state_key;
#  else
    pthread_key_t thread_state_key;
#  endif

    // Current index into the per-thread object pool. Grows proportional
    // to the number of pooled object types that are used across extensions
    std::atomic<uint32_t> pool_index_counter{0};
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
    nb_maybe_atomic<nb_translator_seq *> translators = nullptr;

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
    ptrdiff_t type_data_offset;
#endif

#if defined(NB_FREE_THREADED)
    PyMutex mutex { };
#endif

    // Size of the 'shards' data structure. Only rarely accessed, hence at the end
    size_t shard_count = 1;

    /// Reference count tracking modules + types + functions using shared state
    nb_maybe_atomic<uint32_t> shared_ref_count = 0;

    /// PyList keeping managed PyObjects alive. Cleared when shared_ref_count
    /// reaches 0.
    PyObject *lifeline = nullptr;

    /// Incremented whenever 'lifeline' is destroyed; used to detect stale
    /// per-library 'static_pyobjects' arrays (see init_pyobjects())
    uint32_t lifeline_generation = 0;
};

// Pre-interned strings in the per-module state array, alphabetically
// sorted. Use NB_INTERNED(name) below to access an entry.
#define NB_INTERNED_STRINGS(X)                                                 \
    X(__complex__)                                                             \
    X(__dlpack__)                                                              \
    X(__init__)                                                                \
    X(__length_hint__)                                                         \
    X(__module__)                                                              \
    X(__name__)                                                                \
    X(__new__)                                                                 \
    X(__qualname__)                                                            \
    X(astype)                                                                  \
    X(cast)                                                                    \
    X(clone)                                                                   \
    X(contiguous)                                                              \
    X(copy)                                                                    \
    X(dl_device)                                                               \
    X(max_version)                                                             \
    X(stream)                                                                  \
    X(to)                                                                      \
    X(value)

// Names for the PyObject* entries in the per-module state array.
// These names are scoped, but will implicitly convert to int.
struct pyobj_name {
    enum : int {
        #define NB_INTERNED_ENTRY(name) interned_##name,
        NB_INTERNED_STRINGS(NB_INTERNED_ENTRY)
        #undef NB_INTERNED_ENTRY
        string_count,

        // Cached constant tuples using the same interning machinery
        interned_max_version_tpl = string_count, // tuple ("max_version")
        interned_dl_cpu_tpl,              // tuple (1, 0) == nb::device::cpu
        interned_dl_version_tpl,          // tuple (dlpack major, minor)
        total_count
    };
};

extern PyObject *static_pyobjects[];

/// Access a cached static PyObject (interned string or constant tuple) by name,
/// e.g. NB_INTERNED(__name__) or NB_INTERNED(copy_tpl)
#define NB_INTERNED(name) static_pyobjects[pyobj_name::interned_##name]

extern void internals_inc_ref();
extern void internals_dec_ref();

/// Append 'o' to the lifeline and transfer ownership to it
inline void new_object(nb_internals *p, PyObject *o) {
    PyList_Append(p->lifeline, o);
    Py_DECREF(o);
}

/// Create a type via PyType_FromSpec and transfer ownership to the lifeline
inline PyTypeObject *new_type(nb_internals *p, PyType_Spec *spec) {
    PyTypeObject *tp = (PyTypeObject *) PyType_FromSpec(spec);
    if (tp)
        new_object(p, (PyObject *) tp);
    return tp;
}

/// Convenience macro to potentially access cached functions
#if defined(Py_LIMITED_API)
#  define NB_SLOT(type, name) nb_abi_internals->type##_##name
#else
#  define NB_SLOT(type, name) type.name
#endif


extern char *type_name(const std::type_info *t);

/// Construct 'nb_type' as an instance of the meta-metaclass 'nb_meta'
extern PyTypeObject *nb_type_create_metaclass(nb_internals *p,
                                              PyTypeObject *nb_meta) noexcept;

// Forward declarations
extern PyObject *inst_new_ext(PyTypeObject *tp, void *value);
extern PyObject *inst_new_int(PyTypeObject *tp, PyObject *args, PyObject *kwds);
extern PyTypeObject *nb_static_property_tp(nb_internals *internals) noexcept;
extern type_data *nb_type_c2p(nb_internals *internals,
                              const std::type_info *type);

// Backend implementations behind the nb_abi table (the matching public names in
// nb_lib.h are inline forwarders). Declared here so the table can be populated.
extern PyObject *nb_func_new(nb_internals *internals,
                             const func_data_init_base *f) noexcept;
extern bool leak_warnings(nb_internals *internals) noexcept;
extern bool implicit_cast_warnings(nb_internals *internals) noexcept;
extern void set_leak_warnings(nb_internals *internals, bool value) noexcept;
extern void set_implicit_cast_warnings(nb_internals *internals, bool value) noexcept;
extern void register_exception_translator(nb_internals *internals,
                                          exception_translator t, void *payload);
extern void implicitly_convertible_cpp(nb_internals *internals,
                                       const std::type_info *src,
                                       const std::type_info *dst) noexcept;
extern void implicitly_convertible_py(nb_internals *internals,
                                      bool (*predicate)(PyTypeObject *, PyObject *,
                                                        cleanup_list *),
                                      const std::type_info *dst) noexcept;
extern PyObject *enum_create(nb_internals *internals, enum_data_init *ed) noexcept;
extern bool enum_from_python(nb_internals *internals, const std::type_info *tp,
                             PyObject *o, int64_t *out, uint8_t flags) noexcept;
extern PyObject *enum_from_cpp(nb_internals *internals, const std::type_info *tp,
                               int64_t value) noexcept;
extern PyObject *ndarray_export(nb_internals *internals, ndarray_handle *h,
                                int framework, rv_policy policy,
                                cleanup_list *cleanup) noexcept;
extern PyObject *nb_type_new(nb_internals *internals, const type_data_init *c) noexcept;
extern bool nb_type_get(nb_internals *internals, const std::type_info *t,
                        PyObject *o, uint8_t flags, cleanup_list *cleanup,
                        void **out) noexcept;
extern PyObject *nb_type_put(nb_internals *internals, const std::type_info *cpp_type,
                             void *value, rv_policy rvp, cleanup_list *cleanup,
                             bool *is_new) noexcept;
extern PyObject *nb_type_put_p(nb_internals *internals, const std::type_info *cpp_type,
                               const std::type_info *cpp_type_p, void *value,
                               rv_policy rvp, cleanup_list *cleanup, bool *is_new) noexcept;
extern PyObject *nb_type_put_unique(nb_internals *internals, const std::type_info *cpp_type,
                                    void *value, cleanup_list *cleanup, bool cpp_delete) noexcept;
extern PyObject *nb_type_put_unique_p(nb_internals *internals, const std::type_info *cpp_type,
                                      const std::type_info *cpp_type_p, void *value,
                                      cleanup_list *cleanup, bool cpp_delete) noexcept;
extern bool nb_type_check(nb_internals *internals, PyObject *t) noexcept;
extern bool nb_type_isinstance(nb_internals *internals, PyObject *obj,
                               const std::type_info *t) noexcept;
extern PyObject *nb_type_lookup(nb_internals *internals, const std::type_info *t) noexcept;
extern void keep_alive(nb_internals *internals, PyObject *nurse, PyObject *patient);
extern void keep_alive_cb(nb_internals *internals, PyObject *nurse, void *payload,
                          void (*deleter)(void *) noexcept) noexcept;
extern void property_install(nb_internals *internals, PyObject *scope,
                             const char *name, PyObject *getter,
                             PyObject *setter) noexcept;
extern void property_install_static(nb_internals *internals, PyObject *scope,
                                    const char *name, PyObject *getter,
                                    PyObject *setter) noexcept;
extern void trampoline_new(nb_internals *internals, void **data, size_t size,
                           void *ptr) noexcept;
extern void trampoline_enter(nb_internals *internals, void **data, size_t size,
                             const char *name, bool pure, ticket *t);
extern void nb_type_unregister(type_data *t) noexcept;

// Stateless table entries: extensions reach these only through nb_abi-> (no
// per-domain handle needed), so the backend declares them here rather than in the
// public header. Their addresses populate the matching nb_abi slots.
extern bool load_i8 (PyObject *, uint8_t, int8_t *) noexcept;
extern bool load_u8 (PyObject *, uint8_t, uint8_t *) noexcept;
extern bool load_i16(PyObject *, uint8_t, int16_t *) noexcept;
extern bool load_u16(PyObject *, uint8_t, uint16_t *) noexcept;
extern bool load_i32(PyObject *, uint8_t, int32_t *) noexcept;
extern bool load_u32(PyObject *, uint8_t, uint32_t *) noexcept;
extern bool load_i64(PyObject *, uint8_t, int64_t *) noexcept;
extern bool load_u64(PyObject *, uint8_t, uint64_t *) noexcept;
extern bool load_f32(PyObject *, uint8_t, float *) noexcept;
extern bool load_f64(PyObject *, uint8_t, double *) noexcept;
extern bool load_cmplx(PyObject *, uint8_t, double *, double *) noexcept;
extern void incref_checked(PyObject *) noexcept;
extern void decref_checked(PyObject *) noexcept;
extern PyObject *str_from_obj(PyObject *);
extern PyObject *str_from_cstr(const char *);
extern PyObject *str_from_cstr_and_size(const char *, size_t);
extern PyObject *bytes_from_obj(PyObject *);
extern PyObject *bytes_from_cstr(const char *);
extern PyObject *bytes_from_cstr_and_size(const void *, size_t);
extern PyObject *bytearray_from_obj(PyObject *);
extern PyObject *bytearray_from_cstr_and_size(const void *, size_t);
extern PyObject *bool_from_obj(PyObject *);
extern PyObject *int_from_obj(PyObject *);
extern PyObject *float_from_obj(PyObject *);
extern PyObject *list_from_obj(PyObject *);
extern PyObject *tuple_from_obj(PyObject *);
extern PyObject *set_from_obj(PyObject *);
extern PyObject *frozenset_from_obj(PyObject *);
extern PyObject *memoryview_from_obj(PyObject *);
extern size_t obj_len(PyObject *);
extern size_t obj_len_hint(PyObject *) noexcept;
extern PyObject *obj_repr(PyObject *);
extern bool obj_comp(PyObject *, PyObject *, int);
extern PyObject *obj_op_1(PyObject *, PyObject *(*)(PyObject *));
extern PyObject *obj_op_2(PyObject *, PyObject *, PyObject *(*)(PyObject *, PyObject *));
extern PyObject *obj_vectorcall(PyObject *, PyObject *const *, size_t, PyObject *, bool);
extern PyObject *obj_iter(PyObject *);
extern PyObject *obj_iter_next(PyObject *);
extern void tuple_check(PyObject *, size_t);
extern PyObject *capsule_new(const void *, const char *, void (*)(void *) noexcept) noexcept;
extern void print(PyObject *, PyObject *, PyObject *);
extern PyObject *exception_new(PyObject *, const char *, PyObject *);
extern bool iterable_check(PyObject *) noexcept;
extern PyObject *try_iter(PyObject *) noexcept;
extern bool issubclass(PyObject *, PyObject *);
extern PyObject *repr_list(PyObject *);
extern PyObject *repr_map(PyObject *);
extern void slice_compute(PyObject *, Py_ssize_t, Py_ssize_t &, Py_ssize_t &, Py_ssize_t &, size_t &);
extern PyObject **seq_get_with_size(PyObject *, size_t, PyObject **) noexcept;
extern PyObject **seq_get(PyObject *, size_t *, PyObject **) noexcept;
extern PyObject *module_new_submodule(PyObject *, const char *, const char *) noexcept;
extern void dict_getitem_or_raise(PyObject *, PyObject *, PyObject **);
extern PyObject *dict_getitem_or_default(PyObject *, PyObject *, PyObject *);
extern void dict_setitem(PyObject *, PyObject *, PyObject *);
extern void dict_delitem(PyObject *, PyObject *);
extern bool nb_type_relinquish_ownership(PyObject *, bool) noexcept;
extern void nb_type_restore_ownership(PyObject *, bool) noexcept;
extern void *nb_type_supplement(PyObject *) noexcept;
extern size_t nb_type_size(PyObject *) noexcept;
extern size_t nb_type_align(PyObject *) noexcept;
extern PyObject *nb_type_name(PyObject *) noexcept;
extern PyObject *nb_inst_name(PyObject *) noexcept;
extern const std::type_info *nb_type_info(PyObject *) noexcept;
extern void *type_get_slot_impl(PyTypeObject *, int);
extern void *nb_inst_ptr(PyObject *) noexcept;
extern PyObject *nb_inst_alloc(PyTypeObject *);
extern PyObject *nb_inst_alloc_zero(PyTypeObject *);
extern PyObject *nb_inst_reference(PyTypeObject *, void *, PyObject *);
extern PyObject *nb_inst_take_ownership(PyTypeObject *, void *);
extern void nb_inst_destruct(PyObject *) noexcept;
extern void nb_inst_zero(PyObject *) noexcept;
extern void nb_inst_copy(PyObject *, const PyObject *) noexcept;
extern void nb_inst_move(PyObject *, const PyObject *) noexcept;
extern void nb_inst_replace_copy(PyObject *, const PyObject *) noexcept;
extern void nb_inst_replace_move(PyObject *, const PyObject *) noexcept;
extern bool nb_inst_python_derived(PyObject *) noexcept;
extern void nb_inst_set_state(PyObject *, bool, bool) noexcept;
// Packed state: bit 0 = ready, bit 1 = destruct
extern uint8_t nb_inst_state_read(PyObject *) noexcept;
extern PyObject *getattr_str(PyObject *, const char *);
extern PyObject *getattr_obj(PyObject *, PyObject *);
extern PyObject *getattr_str_def(PyObject *, const char *, PyObject *) noexcept;
extern PyObject *getattr_obj_def(PyObject *, PyObject *, PyObject *) noexcept;
extern void getattr_or_raise_str(PyObject *, const char *, PyObject **);
extern void getattr_or_raise_obj(PyObject *, PyObject *, PyObject **);
extern void setattr_str(PyObject *, const char *, PyObject *);
extern void setattr_obj(PyObject *, PyObject *, PyObject *);
extern void delattr_str(PyObject *, const char *);
extern void delattr_obj(PyObject *, PyObject *);
extern void getitem_or_raise_index(PyObject *, Py_ssize_t, PyObject **);
extern void getitem_or_raise_str(PyObject *, const char *, PyObject **);
extern void getitem_or_raise_obj(PyObject *, PyObject *, PyObject **);
extern void setitem_index(PyObject *, Py_ssize_t, PyObject *);
extern void setitem_str(PyObject *, const char *, PyObject *);
extern void setitem_obj(PyObject *, PyObject *, PyObject *);
extern void delitem_index(PyObject *, Py_ssize_t);
extern void delitem_str(PyObject *, const char *);
extern void delitem_obj(PyObject *, PyObject *);
extern PyObject *module_import_cstr(const char *);
extern PyObject *module_import_obj(PyObject *);
extern void enum_append(PyObject *, const char *, int64_t, const char *, const char *) noexcept;
extern void enum_export(PyObject *);
extern ndarray_handle *ndarray_import(PyObject *, const ndarray_config *, bool, cleanup_list *) noexcept;
extern ndarray_handle *ndarray_create(void *, size_t, const size_t *, PyObject *, const int64_t *, dlpack::dtype, bool, int, int, char, uint64_t);
extern dlpack::dltensor *ndarray_inc_ref(ndarray_handle *) noexcept;
extern void ndarray_dec_ref(ndarray_handle *) noexcept;
extern bool ndarray_check(PyObject *) noexcept;
extern bool is_alive() noexcept;
extern const char *abi_tag();
[[noreturn]] extern void raise_v(const char *, va_list);
[[noreturn]] extern void raise_type_error_v(const char *, va_list);
[[noreturn]] extern void fail_v(const char *, va_list) noexcept;
[[noreturn]] extern void raise_python_error_impl();
[[noreturn]] extern void raise_python_or_cast_error_impl();
extern void chain_error_v(PyObject *, const char *, va_list) noexcept;
extern void python_error_init(python_error *);
extern void python_error_copy(python_error *, const python_error *);
extern void python_error_move(python_error *, python_error *) noexcept;
extern void python_error_destroy(python_error *) noexcept;
extern void python_error_restore(python_error *) noexcept;
extern const char *python_error_what(const python_error *) noexcept;
extern void cleanup_list_expand(cleanup_list *) noexcept;
extern void cleanup_list_release(cleanup_list *) noexcept;
extern void trampoline_release_impl(void **, size_t) noexcept;
extern void trampoline_leave_impl(ticket *) noexcept;

extern PyObject *call_one_arg(PyObject *fn, PyObject *arg) noexcept;

/// Fetch the nanobind function record from a 'nb_func' instance
NB_INLINE func_data *nb_func_data(void *o) {
    return (func_data *) (((char *) o) + sizeof(nb_func));
}

/// Fetch the nanobind type record from a 'nb_type' instance
NB_INLINE type_data *nb_type_data(PyTypeObject *o) noexcept{
    #if !defined(Py_LIMITED_API)
        return (type_data *) (((char *) o) + sizeof(PyHeapTypeObject));
    #else
        #if 1
            // Fast path that can be inlines without spilling registers
            return (type_data *) ((char *) o + nb_abi_internals->type_data_offset);
        #else
            // Equivalent non-inlined reference version:
            return (type_data *) PyObject_GetTypeData((PyObject *) o, Py_TYPE((PyObject *) o));
        #endif
    #endif
}

inline void *inst_ptr(nb_inst *self) {
    void *ptr = (void *) ((intptr_t) self + self->offset);
    return self->state.direct ? ptr : *(void **) ptr;
}

// Return the instance pool associated with type `td`
NB_INLINE nb_inst_pool *nb_pool_lookup(type_data *td) noexcept {
#if !defined(NB_FREE_THREADED)
    // In GIL-protected Python, global pool data structure is reachable via `td`
    return &td->pool;
#else
    // In FT builds, the pool is per thread and stored in a packed pointer array
    nb_thread_state *ts = nb_thread_state_tls;
    if (ts && td->pool_index < ts->pools_size)
        return ts->pools + td->pool_index;
    return nullptr;
#endif
}

// Return the instance pool associated with type `td` or allocate it on demand
extern nb_inst_pool *nb_pool_ensure(type_data *td) noexcept;

/// Release all objects kept in the given instance pool
extern void nb_pool_drain(nb_internals *internals, nb_inst_pool *pool, bool can_free) noexcept;

template <typename T> struct scoped_pymalloc {
    scoped_pymalloc(size_t size = 1, size_t extra_bytes = 0) {
        size_t total = size * sizeof(T);
        if (NB_UNLIKELY(size > SIZE_MAX / sizeof(T) ||
                        total > SIZE_MAX - extra_bytes))
            fail("scoped_pymalloc(): integer overflow!");
        total += extra_bytes;
        ptr = (T *) PyMem_Malloc(total);
        if (!ptr)
            fail("scoped_pymalloc(): could not allocate %llu bytes of memory!",
                 (unsigned long long) total);
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

extern char *extract_name(const char *cmd, const char *prefix, const char *s);


NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
