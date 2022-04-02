#if defined(__GNUC__)
/// Don't warn about missing fields in PyTypeObject declarations
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(_MSC_VER)
#  pragma warning(disable: 4127) // conditional expression is constant (in robin_*.h)
#endif
#include <nanobind/nanobind.h>
#include <tsl/robin_map.h>
#include <tsl/robin_set.h>
#include <typeindex>

#if defined(_MSC_VER)
#  define NB_THREAD_LOCAL __declspec(thread)
#else
#  define NB_THREAD_LOCAL __thread
#endif


NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Metaclass of nb_type
extern PyTypeObject nb_type_type;
/// Metaclass of nb_enum
extern PyTypeObject nb_enum_type;

extern PyTypeObject nb_func_type;
extern PyTypeObject nb_meth_type;
extern PyTypeObject nb_tensor_type;

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

    /// Is the instance data co-located with the Python object?
    bool internal : 1;

    /// Is the instance properly initialized?
    bool ready : 1;

    /// Should the destructor be called when this instance is GCed?
    bool destruct : 1;

    /// Should nanobind call 'operator delete' when this instance is GCed?
    bool cpp_delete : 1;

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

/// Pointers require a good hash function to randomize the mapping to buckets
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

/// Equality operator for keep_alive_entry (only targets data field)
struct keep_alive_eq {
    NB_INLINE bool operator()(const keep_alive_entry &a,
                              const keep_alive_entry &b) const {
        return a.data == b.data;
    }
};

/// Hash operator for keep_alive_entry (only targets data field)
struct keep_alive_hash {
    NB_INLINE size_t operator()(const keep_alive_entry &entry) const {
        return ptr_hash()(entry.data);
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

template <class T1, class T2>
bool operator==(const py_allocator<T1> &, const py_allocator<T2> &) noexcept {
    return true;
}

template <typename key, typename hash = std::hash<key>,
          typename eq = std::equal_to<key>>
using py_set = tsl::robin_set<key, hash, eq, py_allocator<key>>;

template <typename key, typename value, typename hash = std::hash<key>,
          typename eq = std::equal_to<key>>
using py_map =
    tsl::robin_map<key, value, hash, eq, py_allocator<std::pair<key, value>>>;

using keep_alive_set =
    py_set<keep_alive_entry, keep_alive_hash, keep_alive_eq>;

struct internals {
    /// Registered metaclasses for nanobind classes and enumerations
    PyTypeObject *nb_type, *nb_enum;

    /// Types of nanobind functions and methods
    PyTypeObject *nb_func, *nb_meth;

    /// Property variant for static attributes
    PyTypeObject *nb_static_property;

    /// Tensor wrpaper
    PyTypeObject *nb_tensor;

    /// Instance pointer -> Python object mapping
    py_map<std::pair<void *, std::type_index>, nb_inst *, ptr_type_hash>
        inst_c2p;

    /// C++ type -> Python type mapping
    py_map<std::type_index, type_data *> type_c2p;

    /// Dictionary of sets storing keep_alive references
    py_map<void *, keep_alive_set, ptr_hash> keep_alive;

    /// nb_func/meth instance list for leak reporting
    py_set<void *, ptr_hash> funcs;

    /// Registered C++ -> Python exception translators
    std::vector<void (*)(const std::exception_ptr &)> exception_translators;
};

struct current_method {
    const char *name;
    PyObject *self;
};

extern NB_THREAD_LOCAL current_method current_method_data;

extern internals &internals_get() noexcept;
extern char *type_name(const std::type_info *t);

// Forward declarations
extern int nb_type_init(PyObject *, PyObject *, PyObject *);
extern void nb_type_dealloc(PyObject *o);
extern PyObject *inst_new_impl(PyTypeObject *tp, void *value);
extern void nb_enum_prepare(PyTypeObject *tp, bool is_arithmetic);

/// Fetch the nanobind function record from a 'nb_func' instance
inline func_record *nb_func_get(void *o) {
    return (func_record *) (((char *) o) + sizeof(nb_func));
}

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

#if PY_VERSION_HEX < 0x03090000
extern PyObject *nb_vectorcall_method(PyObject *name, PyObject *const *args,
                                      size_t nargsf, PyObject *kwnames);
#endif

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

