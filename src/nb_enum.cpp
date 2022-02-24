#include <nanobind/nanobind.h>
#include "internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

PyTypeObject nb_enum_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nb_enum",
    .tp_basicsize = sizeof(PyHeapTypeObject) + sizeof(type_data),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "nanobind enumeration metaclass"
};

struct nb_enum {
    nb_inst inst;
    PyObject *name;
    const char *doc;
};

static PyObject *nb_enum_repr(PyObject *self) {
    nb_enum *e = (nb_enum *) self;
    return PyUnicode_FromFormat(
        "%U.%U", ((PyHeapTypeObject *) Py_TYPE(self))->ht_qualname, e->name);
}


static PyObject *nb_enum_get_name(PyObject *self, void *) {
    return ((nb_enum *) self)->name;
}

static PyObject *nb_enum_get_doc(PyObject *self, void *) {
    nb_enum *e = (nb_enum *) self;
    if (e->doc) {
        return PyUnicode_FromString(e->doc);
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *nb_enum_int(PyObject *o) {
    nb_type *t = (nb_type *) Py_TYPE(o);
    nb_enum *e = (nb_enum *) o;

    const void *p = inst_data(&e->inst);
    if (t->t.flags & (uint16_t) type_flags::is_unsigned_enum) {
        unsigned long long value;
        switch (t->t.size) {
            case 1: value = (unsigned long long) *(const uint8_t *)  p; break;
            case 2: value = (unsigned long long) *(const uint16_t *) p; break;
            case 4: value = (unsigned long long) *(const uint32_t *) p; break;
            case 8: value = (unsigned long long) *(const uint64_t *) p; break;
            default: PyErr_SetString(PyExc_TypeError, "nb_enum: invalid type size!");
                     return nullptr;
        }
        return PyLong_FromUnsignedLongLong(value);
    } else if (t->t.flags & (uint16_t) type_flags::is_signed_enum) {
        long long value;
        switch (t->t.size) {
            case 1: value = (long long) *(const int8_t *)  p; break;
            case 2: value = (long long) *(const int16_t *) p; break;
            case 4: value = (long long) *(const int32_t *) p; break;
            case 8: value = (long long) *(const int64_t *) p; break;
            default: PyErr_SetString(PyExc_TypeError, "nb_enum: invalid type size!");
                     return nullptr;
        }
        return PyLong_FromLongLong(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "nb_enum: input is not an enumeration!");
        return nullptr;
    }
}

static PyObject *nb_enum_init(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    PyObject *arg;

    if (kwds || PyTuple_GET_SIZE(args) != 1)
        goto error;

    arg = PyTuple_GET_ITEM(args, 0);
    if (PyLong_Check(arg)) {
        PyObject *entries =
            PyObject_GetAttrString((PyObject *) subtype, "__entries");
        if (!entries)
            goto error;
        PyObject *item = PyDict_GetItem(entries, arg);
        Py_DECREF(entries);
        if (item) {
            Py_INCREF(item);
            return item;
        }
    } else if (Py_TYPE(arg) == subtype) {
        Py_INCREF(arg);
        return arg;
    }

error:
    PyErr_Clear();
    PyErr_Format(PyExc_TypeError,
                 "%s(): could not convert the input into an enumeration value!",
                 subtype->tp_name);
    return nullptr;
}

static PyGetSetDef nb_enum_getset[] = {
    { "__doc__", nb_enum_get_doc, nullptr, nullptr, nullptr },
    { "__name__", nb_enum_get_name, nullptr, nullptr, nullptr },
    { nullptr, nullptr, nullptr, nullptr, nullptr }
};

void nb_enum_prepare(PyTypeObject *tp) {
    tp->tp_flags |= Py_TPFLAGS_HAVE_GC;
    tp->tp_traverse = [](PyObject *o, visitproc visit, void *arg) {
        Py_VISIT(Py_TYPE(o));
        return 0;
    };
    tp->tp_new = nb_enum_init;
    tp->tp_init = nullptr;
    tp->tp_clear = [](PyObject *) { return 0; };
    tp->tp_repr = nb_enum_repr;
    tp->tp_as_number->nb_int = nb_enum_int;
    tp->tp_basicsize = sizeof(nb_enum);
    tp->tp_getset = nb_enum_getset;
}

void nb_enum_add(PyObject *type, const char *name, const void *value,
                 const char *doc) noexcept {
    PyObject *name_py, *intval, *dict;
    nb_enum *inst = (nb_enum *) inst_new_impl((PyTypeObject *) type, nullptr);
    if (!inst)
        goto error;

    name_py = PyUnicode_InternFromString(name);
    if (!name_py)
        goto error;

    memcpy(inst_data(&inst->inst), value, ((nb_type *) type)->t.size);
    inst->inst.destruct = false;
    inst->inst.cpp_delete = false;
    inst->inst.ready = true;
    inst->name = name_py;
    inst->doc = doc;

    if (PyObject_SetAttr(type, name_py, (PyObject *) inst))
        goto error;

    intval = nb_enum_int((PyObject *) inst);
    if (!intval)
        goto error;

    dict = PyObject_GetAttrString(type, "__entries");
    if (!dict) {
        PyErr_Clear();
        dict = PyDict_New();
        if (!dict)
            goto error;

        if (PyObject_SetAttrString(type, "__entries", dict))
            goto error;
    }

    if (PyDict_SetItem(dict, intval, (PyObject *) inst))
        goto error;

    Py_DECREF(intval);
    Py_DECREF(dict);
    Py_DECREF(inst);

    return;

error:
    fail("nanobind::detail::nb_enum_add(): could not create enum entry!");
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
