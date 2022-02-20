#include <nanobind/nanobind.h>
#include <tsl/robin_map.h>
#include <tsl/robin_set.h>
#include <typeindex>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Nanobind function metadata (overloads, etc.)
struct func_record : func_data<0> {
    arg_data *args;
};

/// Python object representing a bound C++ type
struct nb_type {
    PyHeapTypeObject ht;
    type_data t;
};

/// Python object representing an instance of a bound C++ type
struct nb_inst { // usually: 24 bytes
    PyObject_HEAD

    /// Offset to the actual instance data
    int32_t offset;

    /**
     * The variable 'offset' can either encode an offset relative to the
     * nb_inst address that leads to the instance data, or it can encode a
     * relative offset to a pointer that must be dereferenced to get to the
     * instance data. 'direct' is 'true' in the former case.
     */
    bool direct : 1;

    /// Is the instance properly initialized?
    bool ready : 1;

    /// Should the destructor be called when this instance is GCed?
    bool destruct : 1;

    /// Should the instance pointer be freed when this instance is GCed?
    bool free : 1;

    /// Does this instance hold reference to others? (via internals.keep_alive)
    bool clear_keep_alive : 1;
};

static_assert(sizeof(nb_inst) == sizeof(PyObject) + sizeof(void *));

/// Python object representing a bound C++ function
struct nb_func {
    PyObject_VAR_HEAD
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
    uint32_t max_nargs_pos;
    bool complex_call;
};

struct ptr_hash {
    size_t operator()(const void *p) const {
        uintptr_t v = (uintptr_t) p;
        // fmix64 from MurmurHash by Austin Appleby (public domain)
        v ^= v >> 33;
        v *= (uintptr_t) 0xff51afd7ed558ccdull;
        v ^= v >> 33;
        v *= (uintptr_t) 0xc4ceb9fe1a85ec53ull;
        v ^= v >> 33;
        return (size_t) v;
    }
};

struct ptr_type_hash {
    NB_INLINE size_t
    operator()(const std::pair<const void *, std::type_index> &value) const {
        return ptr_hash()(value.first) ^ value.second.hash_code();
    }
};

struct keep_alive_entry {
    void *data; // unique data pointer
    void (*deleter)(void *) noexcept; // custom deleter, excluded from hashing/equality

    keep_alive_entry(void *data, void (*deleter)(void *) noexcept = nullptr)
        : data(data), deleter(deleter) { }
};

struct keep_alive_eq {
    NB_INLINE bool operator()(const keep_alive_entry &a,
                              const keep_alive_entry &b) const {
        return a.data == b.data;
    }
};

struct keep_alive_hash {
    NB_INLINE size_t operator()(const keep_alive_entry &entry) const {
        return ptr_hash()(entry.data);
    }
};

using keep_alive_set =
    tsl::robin_set<keep_alive_entry, keep_alive_hash, keep_alive_eq>;

struct internals {
    /// Base type of all nanobind types
    PyTypeObject *nb_type;

    /// Base type of all nanobind functions
    PyTypeObject *nb_func;

    /// Base type of all nanobind methods
    PyTypeObject *nb_meth;

    /// Instance pointer -> Python object mapping
    tsl::robin_map<std::pair<void *, std::type_index>, nb_inst *, ptr_type_hash>
        inst_c2p;

    /// C++ type -> Python type mapping
    tsl::robin_map<std::type_index, type_data *> type_c2p;

    /// Dictionary of sets storing keep_alive references
    tsl::robin_map<void *, keep_alive_set, ptr_hash> keep_alive;

    /// nb_func/meth instance list for leak reporting
    tsl::robin_set<void *, ptr_hash> funcs;

    std::vector<void (*)(std::exception_ptr)> exception_translators;
};

extern internals &internals_get() noexcept;
extern char *type_name(const std::type_info *t);

/// Fetch the nanobind function record from a 'nb_func' instance
inline func_record *nb_func_get(void *o) {
    return (func_record *) (((char *) o) + sizeof(nb_func));
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

