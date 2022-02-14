#include <nanobind/nanobind.h>
#include <tsl/robin_map.h>
#include <tsl/robin_set.h>
#include <typeindex>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Nanobind function metadata (signature, overloads, etc.)
struct func_record : func_data<0> {
    arg_data *args;

    /// Function signature in string format
    char *signature;
};

/// Python object representing an instance of a bound C++ type
struct nb_inst {
    PyObject_HEAD
    void *value;
    bool destruct : 1;
    bool free : 1;
};

/// Python object representing a bound C++ function
struct nb_func {
    PyObject_VAR_HEAD
    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t , PyObject *);
    size_t max_nargs_pos;
};

struct internals {
    /// Base type of all nanobind types
    PyTypeObject *nb_type;

    /// Base type of all nanobind functions
    PyTypeObject *nb_func;

    /// Instance pointer -> Python object mapping
    tsl::robin_pg_map<void *, nb_inst *> inst_c2p;

    /// C++ type -> Python type mapping
    tsl::robin_pg_map<std::type_index, type_data *> type_c2p;

    /// List of all functions for docstring generation
    tsl::robin_pg_set<nanobind::detail::nb_func *> funcs;

    std::vector<void (*)(std::exception_ptr)> exception_translators;
};

extern internals &get_internals() noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

