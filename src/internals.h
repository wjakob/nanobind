#include <nanobind/nanobind.h>
#include "smallvec.h"
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

struct internals {
    /// Base type of all nanobind types
    PyTypeObject *nbtype;

    /// Base type of all nanobind functions
    PyTypeObject *nbfunc;

    /// Instance pointer -> Python object mapping
    tsl::robin_pg_map<void *, nb_inst *> inst_c2p;

    /// C++ type -> Python type mapping
    tsl::robin_pg_map<std::type_index, type_data *> type_c2p;

    /// List of all functions for docstring generation
    tsl::robin_pg_set<PyObject *> funcs;

    smallvec<void (*)(std::exception_ptr)> exception_translators;
};

extern internals &get_internals() noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

