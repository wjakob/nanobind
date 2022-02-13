#include <nanobind/nanobind.h>
#include "smallvec.h"
#include <tsl/robin_map.h>
#include <typeindex>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

struct instance {
    PyObject_HEAD
    PyObject *weakrefs;
    void *value;
    bool destruct : 1;
    bool free : 1;
};

struct internals {
    smallvec<void (*)(std::exception_ptr)> exception_translators;
    tsl::robin_pg_map<void *, instance *> inst_c2p;
    tsl::robin_pg_map<std::type_index, type_data *> type_c2p;
    PyTypeObject *metaclass;
};

extern internals &get_internals() noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

