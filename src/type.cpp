#include "internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

static int nb_type_init(PyObject *self, PyObject *, PyObject *) {
    PyTypeObject *type = Py_TYPE(self);
    PyObject *msg = PyUnicode_FromFormat("%s: no constructor defined!", type->tp_name);
    PyErr_SetObject(PyExc_TypeError, msg);
    Py_DECREF(msg);
    return -1;
}

// Default alloation routine, co-locates Python object and C++ instance
static PyObject *nb_type_new(PyTypeObject *type, PyObject *, PyObject *) {
    instance *self = (instance *) type->tp_alloc(type, 1);

    uintptr_t offset = (uintptr_t) (self + 1),
              align = (uintptr_t) (type->tp_basicsize - sizeof(instance));

    if (align) {
        align += alignof(instance);
        offset = (offset + align - 1) / align * align;
    }

    self->value = (void *) offset;

    auto [it, success] =
        get_internals().inst_c2p.try_emplace(self->value, self);

    if (!success)
        fail("nanobind::detail::nb_type_new(): duplicate object!");

    return (PyObject *) self;
}

static void nb_type_dealloc(PyObject *self) {
    instance *inst = (instance *) self;
    if (inst->weakrefs)
        PyObject_ClearWeakRefs(self);
    PyTypeObject *type = Py_TYPE(self);
    type->tp_free(self);
    Py_DECREF(type);
}

void type_free(PyObject *o) {
    PyTypeObject *type = (PyTypeObject *) o;
    char *tmp = (char *) type->tp_name - sizeof(void *);

    // Recover pointer to C++ type_data entry
    type_data *t;
    memcpy(&t, tmp, sizeof(void *));

    // Try to find type in data structure
    internals &internals = get_internals();
    auto it = internals.types_c2p.find(std::type_index(*t->type));
    if (it == internals.types_c2p.end())
        fail("nanobind::detail::type_free(\"%s\"): could not find type!",
             type->tp_name);
    internals.types_c2p.erase(it);

    // Free type_data entry
    delete t;

    // Free Python type object
    PyType_Type.tp_dealloc(o);

    // Free previously allocated field combining type_data ptr + name
    free(tmp);
}

PyObject *type_new(const type_data *t_) noexcept {
    if (t_->base && t_->base_py)
        fail("nanobind::detail::type_new(\"%s\"): multiple base types "
             "specified!", t_->name);

    internals &internals = get_internals();

    type_data *t = new type_data(*t_);

    auto [it, success] =
        internals.types_c2p.try_emplace(std::type_index(*t->type), t);
    if (!success)
        fail("nanobind::detail::type_new(\"%s\"): type was already registered!",
             t->name);

    str name(t->name), qualname = name, fullname = name;

    if (t->scope && !PyModule_Check(t->scope)) {
        object scope_qualname = borrow(getattr(t->scope, "__qualname__", nullptr));
        if (scope_qualname.is_valid())
            qualname = steal<str>(PyUnicode_FromFormat(
                "%U.%U", scope_qualname.ptr(), name.ptr()));
    }

    object scope_name;
    if (t->scope) {
        scope_name = getattr(t->scope, "__module__", handle());
        if (!scope_name.is_valid())
            scope_name = getattr(t->scope, "__name__", handle());

        if (scope_name.is_valid())
            fullname = steal<str>(
                PyUnicode_FromFormat("%U.%U", scope_name.ptr(), name.ptr()));
    }

    char *doc = nullptr;
    if (t->doc) {
        size_t size = strlen(t->doc) + 1;
        doc = (char *) PyObject_MALLOC(size);
        memcpy(doc, t->doc, size);
    }

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    PyTypeObject *metaclass = internals.metaclass;
    PyHeapTypeObject *ht = (PyHeapTypeObject *) metaclass->tp_alloc(metaclass, 0);
    if (!ht)
        fail("nanobind::detail::type_new(\"%s\"): type creation failed!", t->name);

    ht->ht_name = name.release().ptr();
    ht->ht_qualname = qualname.release().ptr();

    PyTypeObject *type = &ht->ht_type;

    /* To be able to quickly map from Python type object to nanobind
       record, we (ab)use the tp_name field to store one more pointer.. */
    const char *fullname_cstr = fullname.c_str();
    size_t fullname_size = strlen(fullname_cstr);
    char *tmp = (char *) malloc(fullname_size + 1 + sizeof(void *));
    memcpy(tmp, &t, sizeof(void *)); tmp += sizeof(void *);
    memcpy(tmp, fullname_cstr, fullname_size + 1);
    type->tp_name = tmp;

    type->tp_doc = doc;

    type->tp_basicsize = (Py_ssize_t) sizeof(instance);

    if (t->align > alignof(instance))
        type->tp_basicsize += (Py_ssize_t) (t->align - alignof(instance));

    type->tp_itemsize = (Py_ssize_t) t->size;

    type->tp_init = nb_type_init;
    type->tp_new = nb_type_new;
    type->tp_weaklistoffset = offsetof(instance, weakrefs);
    type->tp_as_number = &ht->as_number;
    type->tp_as_sequence = &ht->as_sequence;
    type->tp_as_mapping = &ht->as_mapping;
    type->tp_flags |= Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_BASETYPE;

    if (PyType_Ready(type) < 0)
        fail("nanobind::detail::type_new(\"%s\"): PyType_Ready() failed!", t->name);

    if (scope_name.is_valid())
        setattr((PyObject *) type, "__module__", scope_name);

    if (t->scope)
        setattr(t->scope, t->name, (PyObject *) type);

    t->type_py = type;
    type->tp_name = tmp;

    return (PyObject *) type;
}

bool type_get(PyObject *o, const std::type_info *cpp_type, bool convert,
              void **out) noexcept {
    if (o == nullptr || o == Py_None) {
        *out = nullptr;
        return o != nullptr;
    }

    internals &internals = get_internals();
    PyTypeObject *type = Py_TYPE(o);

    // Reject if this object doesn't have the nanobind metaclass
    if (Py_TYPE(type) != internals.metaclass)
        return false;

    // Recover pointer to C++ type_data entry
    type_data *t;
    memcpy(&t, type->tp_name - sizeof(void *), sizeof(void *));

    // Fast path
    if (t->type == cpp_type || *t->type == *cpp_type) {
        *out = ((instance *) o)->value;
        return true;
    } else {
        return false;
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
