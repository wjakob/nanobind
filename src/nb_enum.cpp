/*
    src/nb_enum.cpp: nanobind enumeration type

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include "internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

PyTypeObject nb_enum_type = {
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

static PyObject *nb_enum_int(PyObject *o);

/// Map to unique representative enum instance
static nb_enum *nb_enum_get_unique(PyObject *self) {
    nb_enum *e = (nb_enum *) self;
    if (e->name)
        return e;

    PyObject *int_val = nb_enum_int(self);
    PyObject *dict = PyObject_GetAttrString((PyObject *) Py_TYPE(self), "__entries");
    nb_enum *e2 = nullptr;
    if (int_val && dict)
        e2 = (nb_enum *) PyDict_GetItem(dict, int_val);

    bool found = e2 && e2->name;

    Py_XDECREF(int_val);
    Py_XDECREF(dict);

    if (found) {
        return e2;
    } else {
        PyErr_Clear();
        PyErr_SetString(PyExc_RuntimeError, "nb_enum: could not find entry!");
        return nullptr;
    }
}

static PyObject *nb_enum_repr(PyObject *self) {
    nb_enum *e = nb_enum_get_unique(self);
    if (!e)
        return nullptr;

    return PyUnicode_FromFormat(
        "%U.%U", ((PyHeapTypeObject *) Py_TYPE(self))->ht_qualname, e->name);
}

static PyObject *nb_enum_get_name(PyObject *self, void *) {
    nb_enum *e = nb_enum_get_unique(self);
    if (!e)
        return nullptr;
    Py_INCREF(e->name);
    return e->name;
}

static PyObject *nb_enum_get_doc(PyObject *self, void *) {
    nb_enum *e = nb_enum_get_unique(self);
    if (!e)
        return nullptr;

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
    if (t->t.flags & (uint32_t) type_flags::is_unsigned_enum) {
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
    } else if (t->t.flags & (uint32_t) type_flags::is_signed_enum) {
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

PyObject *nb_enum_richcompare(PyObject *a, PyObject *b, int op) {
    PyObject *ia = PyNumber_Long(a);
    PyObject *ib = PyNumber_Long(b);
    if (!ia || !ib)
        return nullptr;
    PyObject *result = PyObject_RichCompare(ia, ib, op);
    Py_DECREF(ia);
    Py_DECREF(ib);
    return result;
}

#define NB_ENUM_UNOP(name, op)                                                 \
    PyObject *nb_enum_##name(PyObject *a) {                                    \
        PyObject *ia = PyNumber_Long(a);                                       \
        if (!ia)                                                               \
            return nullptr;                                                    \
        PyObject *result = op(ia);                                             \
        Py_DECREF(ia);                                                         \
        return result;                                                         \
    }

#define NB_ENUM_BINOP(name, op)                                                \
    PyObject *nb_enum_##name(PyObject *a, PyObject *b) {                       \
        PyObject *ia = PyNumber_Long(a);                                       \
        PyObject *ib = PyNumber_Long(b);                                       \
        if (!ia || !ib)                                                        \
            return nullptr;                                                    \
        PyObject *result = op(ia, ib);                                         \
        Py_DECREF(ia);                                                         \
        Py_DECREF(ib);                                                         \
        return result;                                                         \
    }

NB_ENUM_BINOP(add, PyNumber_Add)
NB_ENUM_BINOP(sub, PyNumber_Subtract)
NB_ENUM_BINOP(mul, PyNumber_Multiply)
NB_ENUM_BINOP(div, PyNumber_FloorDivide)
NB_ENUM_BINOP(and, PyNumber_And)
NB_ENUM_BINOP(or, PyNumber_Or)
NB_ENUM_BINOP(xor, PyNumber_Xor)
NB_ENUM_BINOP(lshift, PyNumber_Lshift)
NB_ENUM_BINOP(rshift, PyNumber_Rshift)
NB_ENUM_UNOP(neg, PyNumber_Negative)
NB_ENUM_UNOP(inv, PyNumber_Invert)
NB_ENUM_UNOP(abs, PyNumber_Absolute)

void nb_enum_prepare(PyTypeObject *tp, bool is_arithmetic) {
    tp->tp_flags |= Py_TPFLAGS_HAVE_GC;
    tp->tp_traverse = [](PyObject *o, visitproc visit, void *arg) {
        Py_VISIT(Py_TYPE(o));
        return 0;
    };
    tp->tp_new = nb_enum_init;
    tp->tp_init = nullptr;
    tp->tp_clear = [](PyObject *) { return 0; };
    tp->tp_repr = nb_enum_repr;
    tp->tp_richcompare = nb_enum_richcompare;
    tp->tp_as_number->nb_int = nb_enum_int;
    if (is_arithmetic) {
        tp->tp_as_number->nb_add = nb_enum_add;
        tp->tp_as_number->nb_subtract = nb_enum_sub;
        tp->tp_as_number->nb_multiply = nb_enum_sub;
        tp->tp_as_number->nb_floor_divide = nb_enum_div;
        tp->tp_as_number->nb_or = nb_enum_or;
        tp->tp_as_number->nb_xor = nb_enum_xor;
        tp->tp_as_number->nb_and = nb_enum_and;
        tp->tp_as_number->nb_rshift = nb_enum_rshift;
        tp->tp_as_number->nb_lshift = nb_enum_lshift;
        tp->tp_as_number->nb_negative = nb_enum_neg;
        tp->tp_as_number->nb_invert = nb_enum_inv;
        tp->tp_as_number->nb_absolute = nb_enum_abs;
    }
    tp->tp_basicsize = sizeof(nb_enum);
    tp->tp_getset = nb_enum_getset;
}

void nb_enum_put(PyObject *type, const char *name, const void *value,
                 const char *doc) noexcept {
    PyObject *name_py, *int_val, *dict;
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

    int_val = nb_enum_int((PyObject *) inst);
    if (!int_val)
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

    if (PyDict_SetItem(dict, int_val, (PyObject *) inst))
        goto error;

    Py_DECREF(int_val);
    Py_DECREF(dict);
    Py_DECREF(inst);

    return;

error:
    fail("nanobind::detail::nb_enum_add(): could not create enum entry!");
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
