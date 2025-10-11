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
#include <atomic>
#endif
#include <cstring>
#include <string_view>
#include <functional>
#include "hash.h"
#include "pymetabind.h"

#if TSL_RH_VERSION_MAJOR != 1 || TSL_RH_VERSION_MINOR < 3
#  error nanobind depends on tsl::robin_map, in particular version >= 1.3.0, <2.0.0
#endif

#if defined(_MSC_VER)
#  define NB_THREAD_LOCAL __declspec(thread)
#else
#  define NB_THREAD_LOCAL __thread
#endif

#if defined(PY_BIG_ENDIAN)
#  define NB_BIG_ENDIAN PY_BIG_ENDIAN
#else // pypy doesn't define PY_BIG_ENDIAN
#  if defined(_MSC_VER)
#    define NB_BIG_ENDIAN 0 // All Windows platforms are little-endian
#  else // GCC and Clang define the following macros
#    define NB_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  endif
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
struct func_data : func_data_prelim_base {
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

/// nanobind maintains several maps where there is usually a single entry,
/// but sometimes a list of them. To avoid allocating linked list nodes in
/// the common case, the map value is a pointer whose lowest bit is a type
/// discriminant: 0 if pointing to a single T, and 1 if pointing to a nb_seq<T>.
template <class T>
struct nb_seq {
    T *value;
    nb_seq *next;
};
using nb_inst_seq = nb_seq<PyObject>;
using nb_alias_seq = nb_seq<const std::type_info>;
using nb_foreign_seq = nb_seq<pymb_binding>;

/// Convenience functions to deal with such encoded pointers

/// Does this entry store a linked list of instances?
NB_INLINE bool nb_is_seq(void *p) { return ((uintptr_t) p) & 1; }

/// Tag a nb_seq* pointer as such
template <class T>
NB_INLINE void* nb_mark_seq(nb_seq<T> *p) { return (void *) (((uintptr_t) p) | 1); }

/// Retrieve the nb_seq* pointer from an 'inst_c2p' value, assuming nb_is_seq(p)
template <class T>
NB_INLINE nb_seq<T>* nb_get_seq(void *p) { return (nb_seq<T> *) (((uintptr_t) p) ^ 1); }

template <class T>
nb_seq<T>* nb_ensure_seq(void **p) {
    if (nb_is_seq(*p))
        return nb_get_seq<T>(*p);
    nb_seq<T> *node = (nb_seq<T> *) PyMem_Malloc(sizeof(nb_seq<T>));
    node->value = (T *) *p;
    node->next = nullptr;
    *p = nb_mark_seq(node);
    return node;
}

/// Analogous convenience functions for nanobind vs foreign type disambiguation
/// in type_c2p_* map values. These store one of type_data*, pymb_binding* | 2,
/// or nb_foreign_seq* | 3. So, if !nb_is_foreign(p), cast to type_data*
/// directly; otherwise use either nb_get_seq<pymb_binding>(nb_get_foreign(p))
/// or (pymb_binding*) nb_get_foreign(p) depending on the value of
/// nb_is_seq(nb_get_foreign(p)).

#if defined(NB_DISABLE_INTEROP)
NB_INLINE bool nb_is_foreign(void *) { return false; }
#else
NB_INLINE bool nb_is_foreign(void *p) { return ((uintptr_t) p) & 2; }
NB_INLINE void* nb_mark_foreign(void *p) { return (void *) (((uintptr_t) p) | 2); }
NB_INLINE void* nb_get_foreign(void *p) { return (void *) (((uintptr_t) p) ^ 2); }
static_assert(alignof(type_data) >= 4 && alignof(pymb_binding) >= 4 &&
              alignof(nb_foreign_seq) >= 4,
              "not enough alignment bits for discriminant scheme");
#endif

// Entry in a list of keep_alive weak references. This does not use the
// low-order bit discriminator because the payload is not pointer-sized;
// even if an object has a single weak reference, it will use the seq.
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

/*
 * TYPE MAPPING
 *
 * nanobind has two primary maps indexed by C++ type, `type_c2p_slow` and
 * `type_c2p_fast`. The values in these maps point to information that
 * nanobind needs to convert Python objects to/from the C++ type represented
 * by the key. In both maps, the two lowest-order bits of the pointer value
 * indicate the type of the pointee, with associated semantics:
 *
 * - 0b00: type_data*: This C++ type is bound in the current nanobind domain.
 *                     Any foreign bindings that it has (whether provided by
 *                     this nanobind domain or provided by other frameworks)
 *                     are accessible via type_data::foreign_bindings, with the
 *                     discriminant semantics of the output of nb_get_foreign()
 *                     (low 0b00 for a single binding or 0b01 for a list).
 *
 * - 0b10: pymb_binding*: This C++ type is not bound in the current nanobind
 *                        domain but is bound by a single other framework.
 *
 * - 0b11: nb_foreign_seq*: This C++ type is not bound in the current nanobind
 *                          domain but is bound by one or more other frameworks.
 *                          (Once we have a list, we don't switch back to a
 *                          singleton; see nb_type_try_foreign() for why.)
 *
 * The `slow` map is the source of truth. Its lookups perform string comparisons
 * on the type name, so `std::type_info` objects for the same type from
 * different shared libraries will use the same map entry. Every known
 * type is in this map, and types that are not known have no entry.
 * The entries in this map correspond to C++ types, considered abstractly.
 *
 * The `fast` map is a cache of the `slow` map. Its lookups perform pointer
 * comparisons, so equivalent but distinct `std::type_info` objects have
 * different entries. Only type_info pointers that have been looked up are
 * in this map, and if the lookup failed, an entry will be stored mapping the
 * type_info pointer to null (so we don't keep checking the slow map every
 * time). If lookup of a given type_info pointer succeeds in the fast map,
 * it is guaranteed to produce the same value as it would in the slow map
 * (or if it produces a value of nullptr, the lookup in the slow map would
 * have failed). The entries in this map correspond to `std::type_info`
 * pointers, which have a many-to-one relationship with C++ types.
 *
 * In free-threaded builds, each thread gets its own fast map, encapsulated
 * in `nb_type_map_per_thread`. All the fast maps are linked together in a
 * singly linked list so that they can be updated as needed when changes
 * are made to the slow map. Accesses to each fast map are protected by a mutex
 * in order to deconflict lookups from updates; since most of the time
 * there are no updates to already-cached types, the mutex is effectively
 * thread-local and uncontended.
 *
 * The `nb_internals::types_in_c2p_fast` map keeps a record of which type_info
 * pointers are stored in the fast map for each C++ type. If a type is not in
 * the fast map at all, it has no entry in `types_in_c2p_fast`. If it is in the
 * fast map under a single `type_info` pointer, its entry in `types_in_c2p_fast`
 * has that `type_info` pointer as its key and null as its value. If it is in
 * the fast map under multiple `type_info` pointers, its entry in
 * `types_in_c2p_fast` has one of them as its key, and the corresponding value
 * is a `nb_alias_seq*` containing all the others (but not the key). When a type
 * is added or removed from the slow map, we check the `types_in_c2p_fast`
 * map to see which keys in the fast map might need to be updated.
 *
 * Note that despite this scheme, nanobind stil immortalizes all of its
 * bound types when running in free-threaded mode, because we don't yet
 * handle the possibility of thread A destroying a pytype while thread B is
 * trying to convert an object to it. Fixing this would probably require
 * holding a strong reference to a binding's type object while doing anything
 * with the binding, and obtaining the strong reference before dropping the
 * thread-local fast type map mutex. We're doing something similar for foreign
 * types already, but those aren't as performance-critical, so further research
 * is needed here.
 */

/// A map from std::type_info to pointer-sized data, where lookups use string
/// comparisons. This is instantiated as both `type_c2p_slow` where the value
/// is a pointer to type-specific data as explained in TYPE MAPPING above, and
/// `types_in_c2p_fast` where the value is an `nb_alias_seq` or null.
/// (String comparison is needed because the same type may have multiple
/// std::type_info objects in our address space: one per shared library
/// in which it is used.)
using nb_type_map_slow = tsl::robin_map<const std::type_info *, void *,
                                        std_typeinfo_hash, std_typeinfo_eq>;

/// A simple pointer-to-pointer map that is reused a few times below (even if
/// not 100% ideal) to avoid template code generation bloat.
using nb_ptr_map = tsl::robin_map<void *, void*, ptr_hash>;

/// A map from std::type_info to type data pointer, where lookups use
/// pointer comparisons. The values stored here can be NULL (if a negative
/// lookup has been cached) or else point to any of three types, discriminated
/// using the two lowest-order bits of the pointer; see TYPE MAPPING above.
struct nb_type_map_fast {
    /// Look up a type. If not present in the map, add it with value `dflt`;
    /// then return a reference to the stored value, which the caller may
    /// modify.
    void*& lookup_or_set(const std::type_info *ti, void *dflt) {
        ++update_count;
        return data.try_emplace((void *) ti, dflt).first.value();
    }

    /// Look up a type. Return its associated value, or nullptr if not present.
    /// This method can't distinguish cached negative lookups from entries
    /// that aren't in the map.
    void* lookup(const std::type_info *ti) {
        auto it = data.find((void *) ti);
        return it == data.end() ? nullptr : it->second;
    }

    /// Override the stored value for a type, if present. Return true if
    /// anything was changed.
    bool update(const std::type_info* ti, void *value) {
        auto it = data.find((void *) ti);
        if (it != data.end()) {
            it.value() = value;
            ++update_count;
            return true;
        }
        return false;
    }

    /// Number of times the map has been modified. Used in nb_type_try_foreign()
    /// to detect cases where attempting to use one foreign binding for a type
    /// may have invalidated the iterator needed to advance to the next one.
    uint32_t update_count = 0;

#if defined(NB_FREE_THREADED)
    /// Mutex used by `nb_type_map_per_thread`, stored here because it fits
    /// in padding this way.
    PyMutex mutex{};
#endif

  private:
    // Use a generic ptr->ptr map to avoid needing another instantiation of
    // robin_map. Keys are const std::type_info*. See TYPE MAPPING above for
    // the interpretation of the values.
    nb_ptr_map data;
};

#if defined(NB_FREE_THREADED)
struct nb_internals;

/**
 * Wrapper for nb_type_map_fast in free-threaded mode. Each extension module
 * in a nanobind domain has its own instance of this in thread-local storage
 * for each thread that has used nanobind bindings exposed by that extension
 * module. When the slow map is modified in a way that would invalidate the
 * fast map (removing a cached entry or adding an entry for which a negative
 * lookup has been cached), the linked list is used to update all the caches.
 * Outside of such actions, which occur infrequently, this is a thread-local
 * structure so the mutex accesses are never contended.
 */
struct nb_type_map_per_thread {
    explicit nb_type_map_per_thread(nb_internals &internals_);
    ~nb_type_map_per_thread();

    struct guard;

    struct guard {
        guard() = default;
        guard(guard&& other) noexcept : parent(other.parent) {
            other.parent = nullptr;
        }
        guard& operator=(guard other) noexcept {
            std::swap(parent, other.parent);
            return *this;
        }
        ~guard() {
            if (parent)
                PyMutex_Unlock(&parent->map.mutex);
        }

        nb_type_map_fast& operator*() const { return parent->map; }
        nb_type_map_fast* operator->() const { return &parent->map; }

      private:
        friend nb_type_map_per_thread;
        explicit guard(nb_type_map_per_thread &parent_) : parent(&parent_) {
            PyMutex_Lock(&parent->map.mutex);
        }
        nb_type_map_per_thread *parent = nullptr;
    };
    guard lock() { return guard{*this}; }

    nb_internals &internals;

  private:
    // Access to the map is only possible via `guard`, which holds a lock
    nb_type_map_fast map;

  public:
    // In order to access or modify `next`, you must hold the nb_internals mutex
    // (this->map.mutex is not needed for iteration)
    nb_type_map_per_thread *next = nullptr;
};
#endif

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
     * list of type `nb_inst_seq*` (if bit 0 is set---it must be cleared
     * before interpreting the pointer in this case).
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
  nb_maybe_atomic(T v) : value(v) {}

  std::atomic<T> value;
  NB_INLINE T load_acquire() { return value.load(std::memory_order_acquire); }
  NB_INLINE T load_relaxed() { return value.load(std::memory_order_relaxed); }
  NB_INLINE void store_release(T w) { value.store(w, std::memory_order_release); }
  NB_INLINE bool compare_exchange_weak(T& expected, T desired) {
    return value.compare_exchange_weak(expected, desired);
  }
};
#else
template<typename T>
struct nb_maybe_atomic {
  nb_maybe_atomic(T v) : value(v) {}

  T value;
  NB_INLINE T load_acquire() { return value; }
  NB_INLINE T load_relaxed() { return value; }
  NB_INLINE void store_release(T w) { value = w; }
  NB_INLINE bool compare_exchange_weak(T& expected, T desired) {
    check(value == expected, "compare-exchange would deadlock");
    value = desired;
    return true;
  }
};
#endif

/**
 * Access a non-std::atomic using atomics if we're free-threading --
 * for type_data::foreign_bindings (so we don't have to #include <atomic>
 * in nanobind.h) and nb_foreign_seq::next (so that nb_seq<T> can be generic)
 */
#if !defined(NB_FREE_THREADED)
template <typename T> NB_INLINE T nb_load_acquire(T& loc) { return loc; }
template <typename T> NB_INLINE void nb_store_release(T& loc, T val) { loc = val; }
#elif __cplusplus >= 202002L
// Use std::atomic_ref if available
template <typename T>
NB_INLINE T nb_load_acquire(T& loc) {
    return std::atomic_ref(loc).load(std::memory_order_acquire);
}
template <typename T>
NB_INLINE void nb_store_release(T& loc, T val) {
    return std::atomic_ref(loc).store(val, std::memory_order_release);
}
#else
// Fallback to type punning if not
template <typename T>
NB_INLINE T nb_load_acquire(T& loc) {
    return std::atomic_load_explicit((std::atomic<T> *) &loc,
                                     std::memory_order_acquire);
}
template <typename T>
NB_INLINE void nb_store_release(T& loc, T val) {
    return std::atomic_store_explicit((std::atomic<T> *) &loc, val,
                                      std::memory_order_release);
}
#endif

// Entry in a list of exception translators
struct nb_translator_seq {
    exception_translator translator;
    void *payload;
    nb_maybe_atomic<nb_translator_seq *> next = nullptr;
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
 *   to type data mapping. Unrelated to free-threading, lookups into this
 *   data struture are generally costly because they use a string comparison on
 *   some platforms. Because it is only used as a fallback for 'type_c2p_fast',
 *   protecting this member via the global `mutex` is sufficient.
 *
 * - `types_in_c2p_fast`: Used only when accessing or updating `type_c2p_slow`, so
 *   protecting it with the global `mutex` adds no additional overhead.
 *
 * - `type_c2p_fast`: this data structure is *hot* and mostly read. It serves
 *   as a cache of `type_c2p_slow`, mapping `std::type_info` to type data using
 *   pointer-based comparisons. On free-threaded builds, each thread gets its
 *   own mostly-local instance inside `nb_type_data_per_thread`, which is
 *   protected by an internal mutex in order to safely handle the rare need for
 *   cache invalidations. The head of the linked list of these instances,
 *   `type_c2p_per_thread_head`, is protected by `mutex`; it is only accessed
 *   rarely (when a new thread first uses nanobind, when a thread exits, and
 *   when a type is created or destroyed that has previously been cached).
 *
 * - `translators`: This is a singly linked list traversed while raising
 *   exceptions, from which no element is ever removed. The rare insertions use
 *   compare-and-swap on the head or prev->next pointer.
 *
 * - `funcs`: data structure for function leak tracking. Not used in
 *   free-threaded mode.
 *
 * - `foreign_self`, `foreign_exception_translator`: created only once on
 *   demand, protected by `mutex`; often OK to read without locking since
 *   they never change once set
 *
 * - `foreign_manual_imports`: accessed and modified only during binding import
 *   and removal, which are rare; protected by internals lock
 *
 * - `print_leak_warnings`, `print_implicit_cast_warnings`,
 *   `foreign_export`, `foreign_import`: simple configuration flags.
 *   No protection against concurrent conflicting updates.
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
    nb_maybe_atomic<PyTypeObject *> nb_static_property = nullptr;
    descrsetfunc nb_static_property_descr_set = nullptr;

#if defined(NB_FREE_THREADED)
    Py_tss_t *nb_static_property_disabled = nullptr;
#else
    bool nb_static_property_disabled = false;
#endif

    /// N-dimensional array wrapper (created on demand)
    nb_maybe_atomic<PyTypeObject *> nb_ndarray = nullptr;

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

    /* See TYPE MAPPING above for much more detail on the interplay of
       type_c2p_fast, type_c2p_slow, and types_in_c2p_fast */

#if !defined(NB_FREE_THREADED)
    /// C++ -> Python type map -- fast version based on std::type_info pointer equality
    nb_type_map_fast type_c2p_fast;
#else
    /// Head of the list of per-thread fast C++ -> Python type maps
    nb_type_map_per_thread *type_c2p_per_thread_head = nullptr;
#endif

    /// C++ -> Python type map -- slow fallback version based on hashed strings
    nb_type_map_slow type_c2p_slow;

    /// Each std::type_info that is a key in any `nb_type_map_fast` is
    /// equivalent to some key in this map. If (by pointer equality) there is
    /// only one such std::type_info, the value is null; otherwise, the value
    /// is a `nb_alias_seq*` that heads a list of types that are
    /// equivalent to the key but have distinct `std::type_info` pointers.
    nb_type_map_slow types_in_c2p_fast;

#if !defined(NB_FREE_THREADED)
    /// nb_func/meth instance map for leak reporting (used as set, the value is unused)
    /// In free-threaded mode, functions are immortal and don't require this data structure.
    nb_ptr_map funcs;
#endif

    /// Registered C++ -> Python exception translators. The default exception
    /// translator is the last one in this list.
    nb_maybe_atomic<nb_translator_seq *> translators = nullptr;

    /// Should nanobind print leak warnings on exit?
    bool print_leak_warnings = true;

    /// Should nanobind print warnings after implicit cast failures?
    bool print_implicit_cast_warnings = true;

    /// Should this nanobind domain advertise all of its own types to other
    /// binding frameworks (including other nanobind domains) for use by other
    /// extension modules loaded in this interpreter? Even if this is disabled,
    /// you can export individual types using nb::export_type_to_foreign().
    bool foreign_export_all = false;

    /// Should this nanobind domain make use of all C++ types advertised by
    /// other binding frameworks (including other nanobind domains) from other
    /// extension modules loaded in this interpreter? Even if this is disabled,
    /// you can import individual types using nb::import_foreign_type().
    bool foreign_import_all = false;

    /// Have there ever been any foreign types in `type_c2p_slow`? If not,
    /// we can skip some logic in nb_type_get/put.
    bool foreign_imported_any = false;

    /// Pointer to our own framework object in pymetabind, if enabled
    pymb_framework *foreign_self = nullptr;

    /// Map from pymb_binding* to std::type_info*, reflecting types exported by
    /// (typically) non-C++ extension modules that have been associated with
    /// C++ types via nb::import_foreign_type()
    nb_ptr_map foreign_manual_imports;

    /// Pointer to the canonical copy of `foreign_exception_translator()`
    /// from nb_foreign.cpp. Each DSO may have a different copy, but all will
    /// use the implementation from the first DSO to need it, so that
    /// `nb_foreign_translate_exception()` (translating our exceptions for
    /// a foreign framework's benefit) can skip foreign translators.
    void (*foreign_exception_translator)(const std::exception_ptr&, void*);

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

    // NB_DOMAIN string used to initialize this nanobind domain
    const char *domain;
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
                              const std::type_info *type,
                              bool *has_foreign = nullptr);
extern bool nb_type_register(type_data *t, type_data **conflict) noexcept;
extern void nb_type_unregister(type_data *t) noexcept;
#if defined(NB_FREE_THREADED)
extern nb_type_map_per_thread::guard nb_type_lock_c2p_fast(
    nb_internals *internals_) noexcept;
#endif
extern void nb_type_update_c2p_fast(const std::type_info *type,
                                    void *value) noexcept;

#if !defined(NB_DISABLE_INTEROP)
extern void *nb_type_try_foreign(nb_internals *internals_,
                                 const std::type_info *type,
                                 void* (*attempt)(void *closure,
                                                  pymb_binding *binding),
                                 void *closure) noexcept;
extern void *nb_type_get_foreign(nb_internals *internals_,
                                 const std::type_info *cpp_type,
                                 PyObject *src,
                                 uint8_t flags,
                                 cleanup_list *cleanup) noexcept;
extern PyObject *nb_type_put_foreign(nb_internals *internals_,
                                     const std::type_info *cpp_type,
                                     const std::type_info *cpp_type_p,
                                     void *value,
                                     rv_policy rvp,
                                     cleanup_list *cleanup,
                                     bool *is_new) noexcept;
extern void nb_type_import_impl(PyObject *pytype,
                                const std::type_info *cpptype);
extern void nb_type_export_impl(type_data *td);
extern void nb_type_enable_import_all();
extern void nb_type_enable_export_all();
#endif

extern bool set_builtin_exception_status(builtin_exception &e);
extern void default_exception_translator(const std::exception_ptr &, void *);

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

// Fetch the type record from an enum created by nanobind
extern type_init_data *enum_get_type_data(handle tp);

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

extern char *extract_name(const char *cmd, const char *prefix, const char *s);


NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
