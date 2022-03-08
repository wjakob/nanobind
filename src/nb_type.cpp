/*
    src/nb_type.cpp: libnanobind functionality for binding classes

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include "internals.h"

#if defined(_MSC_VER)
#  pragma warning(disable: 4706) // assignment within conditional expression
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

static int nb_type_setattro(PyObject*, PyObject*, PyObject*);

PyTypeObject nb_type_type = {
    .tp_name = "nb_type",
    .tp_basicsize = sizeof(nb_type),
    .tp_dealloc = nb_type_dealloc,
    .tp_setattro = nb_type_setattro,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "nanobind metaclass",
    .tp_init = nb_type_init
};

static int inst_init(PyObject *self, PyObject *, PyObject *) {
    PyErr_Format(PyExc_TypeError, "%s: no constructor defined!",
                 Py_TYPE(self)->tp_name);
    return -1;
}

/// Allocate memory for a nb_type instance with internal or external storage
PyObject *inst_new_impl(PyTypeObject *tp, void *value) {
    const type_data *t = &((nb_type *) tp)->t;
    const bool has_gc = tp->tp_flags & Py_TPFLAGS_HAVE_GC;

    size_t gc_size = has_gc ? sizeof(uintptr_t) * 2 : 0,
           basic_size = (size_t) tp->tp_basicsize,
           nb_inst_size = basic_size + gc_size,
           align = (size_t) t->align;

    size_t size = nb_inst_size;
    if (!value) {
        // Internal storage: space for the object and padding for alignment
        size += t->size;
        if (align > sizeof(void *))
            size += align - sizeof(void *);
    }

    uint8_t *alloc = (uint8_t *) PyObject_Malloc(size);
    if (!alloc)
        return PyErr_NoMemory();

    // Clear only the initial part of the object (GC head, nb_inst contents)
    memset(alloc, 0, nb_inst_size);

    nb_inst *self = (nb_inst *) (alloc + gc_size);
    if (!value) {
        // Address of instance payload area (aligned)
        uintptr_t payload = (uintptr_t) alloc + nb_inst_size;
        payload = (payload + align - 1) / align * align;

        // Encode offset to aligned payload
        self->offset = (int32_t) ((intptr_t) payload - (intptr_t) self);
        self->direct = true;
        self->internal = true;

        value = (void *) payload;
    } else {
        // Compute offset to instance value
        int32_t offset = (int32_t) ((intptr_t) value - (intptr_t) self);

        if ((intptr_t) self + offset == (intptr_t) value) {
            // Offset *is* representable as 32 bit value
            self->offset = offset;
            self->direct = true;
        } else {
            // Offset *not* representable, allocate extra memory for a pointer
            uint8_t *new_alloc = (uint8_t *)
                PyObject_Realloc(alloc, nb_inst_size + sizeof(void *));

            if (!new_alloc) {
                PyObject_Free(alloc);
                return PyErr_NoMemory();
            }

            *(void **) (new_alloc + nb_inst_size) = value;
            self = (nb_inst *) (new_alloc + gc_size);
            self->offset = (uint32_t) basic_size;
            self->direct = false;
        }

        self->internal = false;
    }

    // Update hash table that maps from C++ to Python instance
    auto [it, success] = internals_get().inst_c2p.try_emplace(
        std::pair<void *, std::type_index>(value, *t->type),
        self);

    if (!success)
        fail("nanobind::detail::inst_new(): duplicate object!");

    PyObject_Init((PyObject *) self, tp);
    if (has_gc)
        PyObject_GC_Track(self);

    return (PyObject *) self;
}

// Allocate a new instance with co-located storage
PyObject *inst_new(PyTypeObject *type, PyObject *, PyObject *) {
    return (PyObject *) inst_new_impl(type, nullptr);
}

static void inst_dealloc(PyObject *self) {
    nb_type *type = (nb_type *) Py_TYPE(self);
    if (type->ht.ht_type.tp_flags & Py_TPFLAGS_HAVE_GC)
        PyObject_GC_UnTrack(self);

    nb_inst *inst = (nb_inst *) self;
    void *p = inst_data(inst);

    if (inst->destruct) {
        if (type->t.flags & (int32_t) type_flags::is_destructible) {
            if (type->t.flags & (int32_t) type_flags::has_destruct)
                type->t.destruct(p);
        } else {
            fail("nanobind::detail::inst_dealloc(\"%s\"): attempted to call "
                 "the destructor of a non-destructible type!",
                 type->ht.ht_type.tp_name);
        }
    }

    if (inst->cpp_delete) {
        if (type->t.align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            operator delete(p);
        else
            operator delete(p, std::align_val_t(type->t.align));
    }

    internals &internals = internals_get();
    if (inst->clear_keep_alive) {
        auto it = internals.keep_alive.find(self);
        if (it == internals.keep_alive.end())
            fail("nanobind::detail::inst_dealloc(\"%s\"): inconsistent "
                 "keep_alive information", type->ht.ht_type.tp_name);

        keep_alive_set ref_set = std::move(it.value());
        internals.keep_alive.erase(it);

        for (keep_alive_entry e: ref_set) {
            if (!e.deleter)
                Py_DECREF((PyObject *) e.data);
            else
                e.deleter(e.data);
        }
    }

    // Update hash table that maps from C++ to Python instance
    auto it = internals.inst_c2p.find(
        std::pair<void *, std::type_index>(p, *type->t.type));
    if (it == internals.inst_c2p.end())
        fail("nanobind::detail::inst_dealloc(\"%s\"): attempted to delete "
             "an unknown instance (%p)!", type->ht.ht_type.tp_name, p);
    internals.inst_c2p.erase(it);

    type->ht.ht_type.tp_free(self);
    Py_DECREF(type);
}

void nb_type_dealloc(PyObject *o) {
    nb_type *nbt = (nb_type *) o;

    if ((nbt->t.flags & (uint32_t) type_flags::is_python_type) == 0) {
        // Try to find type in data structure
        internals &internals = internals_get();
        auto it = internals.type_c2p.find(std::type_index(*nbt->t.type));
        if (it == internals.type_c2p.end())
            fail("nanobind::detail::nb_type_dealloc(\"%s\"): could not find type!",
                 ((PyTypeObject *) o)->tp_name);
        internals.type_c2p.erase(it);
    }

    if (nbt->t.flags & (uint32_t) type_flags::has_implicit_conversions) {
        free(nbt->t.implicit);
        free(nbt->t.implicit_py);
    }

    free((char *) nbt->t.name);

    PyType_Type.tp_dealloc(o);
}

/// Called when a C++ type is extended from within Python
int nb_type_init(PyObject *self, PyObject *args, PyObject *kwds) {
    if (PyTuple_GET_SIZE(args) != 3)
        return -1;

    int rv = PyType_Type.tp_init(self, args, kwds);
    if (rv)
        return rv;

    nb_type *type = (nb_type *) self;
    nb_type *parent = (nb_type *) type->ht.ht_type.tp_base;

    type->t = parent->t;
    type->t.flags |=  (uint32_t) type_flags::is_python_type;
    type->t.flags &= ~(uint32_t) type_flags::has_implicit_conversions;
    type->t.name = strdup(type->ht.ht_type.tp_name);
    type->t.type_py = &type->ht.ht_type;
    type->t.base = parent->t.type;
    type->t.base_py = &parent->ht.ht_type;
    type->t.implicit = nullptr;
    type->t.implicit_py = nullptr;

    return 0;
}

/// Called when a C++ type is bound via nb::class_<>
PyObject *nb_type_new(const type_data *t) noexcept {
    const bool is_signed_enum    = t->flags & (uint32_t) type_flags::is_signed_enum,
               is_unsigned_enum  = t->flags & (uint32_t) type_flags::is_unsigned_enum,
               is_arithmetic     = t->flags & (uint32_t) type_flags::is_arithmetic,
               is_enum           = is_signed_enum || is_unsigned_enum,
               has_scope         = t->flags & (uint32_t) type_flags::has_scope,
               has_doc           = t->flags & (uint32_t) type_flags::has_doc,
               has_base          = t->flags & (uint32_t) type_flags::has_base,
               has_base_py       = t->flags & (uint32_t) type_flags::has_base_py,
               has_type_callback = t->flags & (uint32_t) type_flags::has_type_callback;

    str name(t->name), qualname = name, fullname = name;

    if (has_scope && !PyModule_Check(t->scope)) {
        object scope_qualname = borrow(getattr(t->scope, "__qualname__", nullptr));
        if (scope_qualname.is_valid())
            qualname = steal<str>(PyUnicode_FromFormat(
                "%U.%U", scope_qualname.ptr(), name.ptr()));
    }

    object scope_name;
    if (has_scope) {
        scope_name = getattr(t->scope, "__module__", handle());
        if (!scope_name.is_valid())
            scope_name = getattr(t->scope, "__name__", handle());

        if (scope_name.is_valid())
            fullname = steal<str>(
                PyUnicode_FromFormat("%U.%U", scope_name.ptr(), name.ptr()));
    }

    char *doc = nullptr;
    if (has_doc) {
        size_t len = strlen(t->doc) + 1;
        doc = (char *) PyObject_Malloc(len);
        memcpy(doc, t->doc, len);
    }

    internals &internals = internals_get();
    PyTypeObject *tp =
        is_enum ? internals.nb_enum : internals.nb_type;

    const bool has_gc = tp->tp_flags & Py_TPFLAGS_HAVE_GC;

    size_t gc_size = has_gc ? sizeof(uintptr_t) * 2 : 0,
           size = gc_size + (size_t) (tp->tp_basicsize + tp->tp_itemsize) +
                  t->supplement;
    uint8_t *alloc = (uint8_t *) PyObject_Malloc(size);
    if (!alloc)
        return PyErr_NoMemory();
    memset(alloc, 0, size);

    nb_type *nbt = (nb_type *) (alloc + gc_size);

    PyObject_InitVar((PyVarObject *) nbt, tp, 0);
    if (has_gc)
        PyObject_GC_Track(nbt);

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */

    PyTypeObject *type = &nbt->ht.ht_type;

    memcpy(&nbt->t, t, sizeof(type_data));
    nbt->t.name = strdup(t->name);

    nbt->ht.ht_name = name.release().ptr();
    nbt->ht.ht_qualname = qualname.release().ptr();

    PyTypeObject *base = nullptr;
    if (has_base_py) {
        if (has_base)
            fail("nanobind::detail::nb_type_new(\"%s\"): multiple base types "
                 "specified!", t->name);
        base = t->base_py;
    } else if (has_base) {
        auto it = internals.type_c2p.find(std::type_index(*t->base));
        if (it == internals.type_c2p.end())
            fail("nanobind::detail::nb_type_new(\"%s\"): base type "
                 "\"%s\" not found!", t->name, type_name(t->base));
        base = it->second->type_py;
    }

    type->tp_name = nbt->t.name;
    type->tp_basicsize = (Py_ssize_t) sizeof(nb_inst);

    type->tp_dealloc = inst_dealloc;
    type->tp_as_async = &nbt->ht.as_async;
    type->tp_as_number = &nbt->ht.as_number;
    type->tp_as_sequence = &nbt->ht.as_sequence;
    type->tp_as_mapping = &nbt->ht.as_mapping;
    type->tp_flags |=
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_BASETYPE;
    if (has_doc)
        type->tp_doc = doc;
    type->tp_base = base;
    type->tp_init = inst_init;
    type->tp_new = inst_new;

    if (is_enum) // last step before PyType_Ready
        nb_enum_prepare(type, is_arithmetic);

    if (has_type_callback)
        nbt->t.type_callback(type);

    if (PyType_Ready(type) < 0)
        fail("nanobind::detail::nb_type_new(\"%s\"): PyType_Ready() failed!", t->name);

    Py_INCREF(type->tp_base);

    if (scope_name.is_valid())
        setattr((PyObject *) type, "__module__", scope_name);

    if (has_scope)
        setattr(t->scope, t->name, (PyObject *) type);

    nbt->t.type_py = type;

    // Update hash table that maps from std::type_info to Python type
    auto [it, success] =
        internals.type_c2p.try_emplace(std::type_index(*t->type), &nbt->t);
    if (!success)
        fail("nanobind::detail::nb_type_new(\"%s\"): type was already registered!",
             t->name);

    return (PyObject *) type;
}


/// Encapsulates the implicit conversion part of nb_type_get()
static NB_NOINLINE bool nb_type_get_implicit(PyObject *src,
                                             const std::type_info *cpp_type_src,
                                             const type_data *dst_type,
                                             internals &internals,
                                             cleanup_list *cleanup, void **out) {
    if (dst_type->implicit && cpp_type_src) {
        const std::type_info **it = dst_type->implicit;
        const std::type_info *v;

        while ((v = *it++)) {
            if (v == cpp_type_src || *v == *cpp_type_src)
                goto found;
        }

        it = dst_type->implicit;
        while ((v = *it++)) {
            auto it2 = internals.type_c2p.find(std::type_index(*v));
            if (it2 != internals.type_c2p.end() &&
                PyType_IsSubtype(Py_TYPE(src), it2->second->type_py))
                goto found;
        }
    }

    if (dst_type->implicit_py) {
        bool (**it)(PyObject *, cleanup_list *) noexcept =
            dst_type->implicit_py;
        bool (*v2)(PyObject *, cleanup_list *) noexcept;

        while ((v2 = *it++)) {
            if (v2(src, cleanup))
                goto found;
        }
    }

    return false;

found:
    PyObject *args[2] = { nullptr, src };
    PyObject *result =
        NB_VECTORCALL((PyObject *) dst_type->type_py, args + 1,
                      PY_VECTORCALL_ARGUMENTS_OFFSET + 1, nullptr);

    if (result) {
        cleanup->append(result);
        *out = inst_data((nb_inst *) result);
        return true;
    } else {
        PyErr_Clear();
        PyErr_WarnFormat(PyExc_RuntimeWarning, 1,
                         "nanobind: implicit conversion from type '%s' "
                         "to type '%s' failed!",
                         Py_TYPE(src)->tp_name, dst_type->name);

        return false;
    }
}

// Attempt to retrieve a pointer to a C++ instance
bool nb_type_get(const std::type_info *cpp_type, PyObject *src, uint8_t flags,
                 cleanup_list *cleanup, void **out) noexcept {
    // Convert None -> nullptr
    if (src == Py_None) {
        *out = nullptr;
        return true;
    }

    internals &internals = internals_get();
    PyTypeObject *src_type = Py_TYPE(src);
    const std::type_info *cpp_type_src = nullptr;
    const PyTypeObject *metaclass = Py_TYPE(src_type);
    const bool src_is_nb_type =
        metaclass == internals.nb_type ||
        metaclass == internals.nb_enum;
    type_data *dst_type = nullptr;

    // If 'src' is a nanobind-bound type
    if (src_is_nb_type) {
        nb_type *nbt = (nb_type *) src_type;
        cpp_type_src = nbt->t.type;

        // Check if the source / destination typeid are an exact match
        bool valid = cpp_type == cpp_type_src || *cpp_type == *cpp_type_src;

        // If not, look up the Python type and check the inheritance chain
        if (!valid) {
            auto it = internals.type_c2p.find(std::type_index(*cpp_type));
            if (it != internals.type_c2p.end()) {
                dst_type = it->second;
                valid = PyType_IsSubtype(src_type, dst_type->type_py);
            }
        }

        // Success, return the pointer if the instance is correctly initialized
        if (valid) {
            nb_inst *inst = (nb_inst *) src;

            if (!inst->ready &&
                (flags & (uint8_t) cast_flags::construct) == 0) {
                PyErr_WarnFormat(PyExc_RuntimeWarning, 1,
                                 "nanobind: attempted to access an "
                                 "uninitialized instance of type '%s'!\n",
                                 nbt->t.name);
                return false;
            }

            *out = inst_data(inst);

            return true;
        }
    }

    // Try an implicit conversion as last resort (if possible & requested)
    if ((flags & (uint16_t) cast_flags::convert) && cleanup) {
        if (!src_is_nb_type) {
            auto it = internals.type_c2p.find(std::type_index(*cpp_type));
            if (it != internals.type_c2p.end())
                dst_type = it->second;
        }

        if (dst_type &&
            (dst_type->flags & (uint32_t) type_flags::has_implicit_conversions))
            return nb_type_get_implicit(src, cpp_type_src, dst_type, internals,
                                        cleanup, out);
    }
    return false;
}

static PyObject *keep_alive_callback(PyObject *self, PyObject *const *args,
                                     Py_ssize_t nargs) {
    if (nargs != 1 || !PyWeakref_CheckRefExact(args[0]))
        fail("nanobind::detail::keep_alive_callback(): invalid input!");
    Py_DECREF(args[0]); // self
    Py_DECREF(self); // patient
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef keep_alive_callback_def = {
    "keep_alive_callback",
    (PyCFunction) (void *) keep_alive_callback,
    METH_FASTCALL,
    "Implementation detail of nanobind::detail::keep_alive"
};


void keep_alive(PyObject *nurse, PyObject *patient) noexcept {
    if (!patient)
        return;

    if (!nurse)
        fail("nanobind::detail::keep_alive(): the 'nurse' argument must be "
             "provided!");

    internals &internals = internals_get();
    PyTypeObject *metaclass = Py_TYPE(Py_TYPE(nurse));

    if (metaclass == internals.nb_type || metaclass == internals.nb_enum) {
        // Populate nanobind-internal data structures
        keep_alive_set &keep_alive = internals.keep_alive[nurse];

        auto [it, success] = keep_alive.emplace(patient);
        if (success) {
            Py_INCREF(patient);
            ((nb_inst *) nurse)->clear_keep_alive = true;
        } else {
            if (it->deleter)
                fail("nanobind::detail::keep_alive(): internal error: entry "
                     "has a deletion callback!");
        }
    } else {
        PyObject *callback =
            PyCFunction_New(&keep_alive_callback_def, patient);
        if (!callback)
            fail("nanobind::detail::keep_alive(): callback creation failed!");
        PyObject *weakref = PyWeakref_NewRef(nurse, callback);
        if (!weakref)
            fail("nanobind::detail::keep_alive(): could not create a weak "
                 "reference! Likely, the 'nurse' argument you specified is not "
                 "a weak-referenceable type!");
        // Increase patient reference count, leak weak reference
        Py_INCREF(patient);
        Py_DECREF(callback);
    }
}

void keep_alive(PyObject *nurse, void *payload,
                void (*callback)(void *) noexcept) noexcept {
    if (!nurse)
        fail("nanobind::detail::keep_alive(): nurse==nullptr!");

    PyTypeObject *metaclass = Py_TYPE(Py_TYPE(nurse));

    internals &internals = internals_get();
    if (metaclass != internals.nb_type && metaclass != internals.nb_enum)
        fail("keep_alive(): expected a nb_type 'nurse' argument");

    keep_alive_set &keep_alive = internals.keep_alive[nurse];
    auto [it, success] = keep_alive.emplace(payload, callback);
    if (!success)
        raise("keep_alive(): the given 'payload' pointer was already registered!");
    ((nb_inst *) nurse)->clear_keep_alive = true;
}

PyObject *nb_type_put(const std::type_info *cpp_type, void *value,
                      rv_policy rvp, cleanup_list *cleanup,
                      bool *is_new) noexcept {
    // Convert nullptr -> None
    if (!value) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    // Check if the instance is already registered with nanobind
    internals &internals = internals_get();
    auto it = internals.inst_c2p.find(
        std::pair<void *, std::type_index>(value, *cpp_type));
    if (it != internals.inst_c2p.end()) {
        PyObject *result = (PyObject *) it->second;
        Py_INCREF(result);
        return result;
    } else if (rvp == rv_policy::none) {
        return nullptr;
    }

    // Look up the corresponding type
    auto it2 = internals.type_c2p.find(std::type_index(*cpp_type));
    if (it2 == internals.type_c2p.end())
        return nullptr;

    // The reference_internals RVP needs a self pointer, give up if unavailable
    if (rvp == rv_policy::reference_internal && (!cleanup || !cleanup->self()))
        return nullptr;

    bool store_in_obj = rvp == rv_policy::copy || rvp == rv_policy::move;

    type_data *t = it2->second;
    nb_inst *inst =
        (nb_inst *) inst_new_impl(t->type_py, store_in_obj ? nullptr : value);
    if (!inst)
        return nullptr;

    if (is_new)
        *is_new = true;

    void *new_value = inst_data(inst);
    if (rvp == rv_policy::move) {
        if (t->flags & (uint32_t) type_flags::is_move_constructible) {
            if (t->flags & (uint32_t) type_flags::has_move) {
                try {
                    t->move(new_value, value);
                } catch (...) {
                    Py_DECREF(inst);
                    return nullptr;
                }
            } else {
                memcpy(new_value, value, t->size);
            }
        } else {
            if (t->flags & (uint32_t) type_flags::is_copy_constructible) {
                rvp = rv_policy::copy;
            } else {
                fail("nanobind::detail::nb_type_put(\"%s\"): attempted to move "
                     "an instance that is neither copy- nor move-constructible!",
                     t->name);
            }
        }
    }

    if (rvp == rv_policy::copy) {
        if (t->flags & (uint32_t) type_flags::is_copy_constructible) {
            if (t->flags & (uint32_t) type_flags::has_copy) {
                try {
                    t->copy(new_value, value);
                } catch (...) {
                    Py_DECREF(inst);
                    return nullptr;
                }
            } else {
                memcpy(new_value, value, t->size);
            }
        } else {
            fail("nanobind::detail::nb_type_put(\"%s\"): attempted to copy "
                 "an instance that is not copy-constructible!", t->name);
        }
    }

    inst->destruct = rvp != rv_policy::reference && rvp != rv_policy::reference_internal;
    inst->cpp_delete = rvp == rv_policy::take_ownership;
    inst->ready = true;

    if (rvp == rv_policy::reference_internal)
        keep_alive((PyObject *) inst, cleanup->self());

    return (PyObject *) inst;
}

PyObject *nb_type_put_unique(const std::type_info *cpp_type, void *value,
                             cleanup_list *cleanup, bool cpp_delete) noexcept {
    rv_policy policy = cpp_delete ? rv_policy::take_ownership : rv_policy::none;

    bool is_new = false;
    PyObject *o = nb_type_put(cpp_type, value, policy, cleanup, &is_new);
    if (!o)
        return nullptr;

    if (!cpp_delete && is_new)
        fail("nanobind::detail::nb_type_put_unique(type='%s', cpp_delete=%i): "
             "ownership status has become corrupted.",
             Py_TYPE(o)->tp_name, cpp_delete);

    nb_inst *inst = (nb_inst *) o;

    if (cpp_delete) {
        if (inst->ready != is_new || inst->destruct != is_new ||
            inst->cpp_delete != is_new)
            fail("nanobind::detail::nb_type_put_unique(type='%s', "
                 "cpp_delete=%i): unexpected status flags! (ready=%i, "
                 "destruct=%i, cpp_delete=%i)",
                 Py_TYPE(o)->tp_name, cpp_delete, inst->ready,
                 inst->destruct, inst->cpp_delete);

        inst->ready = inst->destruct = inst->cpp_delete = true;
    } else {
        if (inst->ready)
            fail("nanobind::detail::nb_type_put_unique('%s'): ownership "
                 "status has become corrupted.",
                 Py_TYPE(o)->tp_name);
        inst->ready = true;
    }
    return o;
}

void nb_type_relinquish_ownership(PyObject *o, bool cpp_delete) {
    nb_inst *inst = (nb_inst *) o;

    // This function is called to indicate ownership *changes*
    if (!inst->ready)
        fail("nanobind::detail::nb_relinquish_ownership('%s'): ownership "
             "status has become corrupted.",
             Py_TYPE(o)->tp_name);

    if (cpp_delete) {
        if (!inst->cpp_delete || !inst->destruct || inst->internal) {
            PyErr_WarnFormat(
                PyExc_RuntimeWarning, 1,
                "nanobind::detail::nb_relinquish_ownership(): could not "
                "transfer ownership of a Python instance of type '%s' to C++. "
                "This is only possible when the instance was previously "
                "constructed on the C++ side and is now owned by Python, which "
                "was not the case here. You could change the unique pointer "
                "signature to std::unique_ptr<T, nb::deleter<T>> to work around "
                "this issue.", Py_TYPE(o)->tp_name);

            raise_next_overload();
        }

        inst->cpp_delete = false;
        inst->destruct = false;
    }

    inst->ready = false;
}

/// Special case to handle 'Class.property = value' assignments
static int nb_type_setattro(PyObject* obj, PyObject* name, PyObject* value) {
    PyObject *descr = _PyType_Lookup((PyTypeObject *) obj, name);
    PyTypeObject *descr_t = descr ? Py_TYPE(descr) : nullptr;

    if (descr_t == internals_get().nb_static_property)
        return descr_t->tp_descr_set(descr, obj, value);

    return PyType_Type.tp_setattro(obj, name, value);
}

bool nb_type_isinstance(PyObject *obj, const std::type_info *t) noexcept {
    internals &internals = internals_get();
    auto it = internals.type_c2p.find(std::type_index(*t));
    if (it == internals.type_c2p.end())
        return false;
    return PyType_IsSubtype(Py_TYPE(obj), it->second->type_py);
}

PyObject *nb_type_lookup(const std::type_info *t) noexcept {
    internals &internals = internals_get();
    auto it = internals.type_c2p.find(std::type_index(*t));
    if (it != internals.type_c2p.end())
        return (PyObject *) it->second->type_py;
    return nullptr;
}


void *nb_type_extra(PyObject *t) noexcept {
    PyTypeObject *tp = Py_TYPE(t);
    return (uint8_t *) t + tp->tp_basicsize + tp->tp_itemsize;
}

void *nb_inst_data(PyObject *o) noexcept {
    return inst_data((nb_inst *) o);
}

void nb_inst_zero(PyObject *o) noexcept {
    nb_inst *nbi = (nb_inst *) o;
    memset(inst_data(nbi), 0, ((nb_type *) Py_TYPE(o))->t.size);
    nbi->ready = nbi->destruct = true;
}

void nb_inst_copy(PyObject *dst, const PyObject *src) noexcept {
    nb_type *nbt = (nb_type *) Py_TYPE(src);
    if (Py_TYPE(src) != Py_TYPE(dst) ||
        (nbt->t.flags & (uint32_t) type_flags::is_copy_constructible) == 0)
        fail("nanobind::detail::nb_inst_copy(): invalid arguments!");

    nb_inst *nbi = (nb_inst *) dst;
    const void *src_data = inst_data((nb_inst *) src);
    void *dst_data = inst_data(nbi);

    if (nbt->t.flags & (uint32_t) type_flags::has_copy)
        nbt->t.copy(dst_data, src_data);
    else
        memcpy(dst_data, src_data, nbt->t.size);

    nbi->ready = nbi->destruct = true;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
