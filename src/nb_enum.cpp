/*
    src/nb_enum.cpp: nanobind enumeration type

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

NB_INLINE enum_supplement &nb_enum_supplement(PyTypeObject *type) {
    return type_supplement<enum_supplement>(type);
}

static PyObject *nb_enum_int_signed(PyObject *o);
static PyObject *nb_enum_int_unsigned(PyObject *o);

/// Map to unique representative enum instance, returns a borrowed reference
static PyObject *nb_enum_lookup(PyObject *self) {
    enum_supplement &supp = nb_enum_supplement(Py_TYPE(self));
    PyObject *int_val = supp.is_signed ? nb_enum_int_signed(self)
                                       : nb_enum_int_unsigned(self);
    PyObject *rec = nullptr;
    if (int_val && supp.entries)
        rec = (PyObject *) PyDict_GetItem(supp.entries, int_val);

    Py_XDECREF(int_val);

    if (rec && PyTuple_CheckExact(rec) && NB_TUPLE_GET_SIZE(rec) == 3) {
        return rec;
    } else {
        PyErr_Clear();
        PyErr_SetString(PyExc_RuntimeError, "nb_enum: could not find entry!");
        return nullptr;
    }
}

static PyObject *nb_enum_repr(PyObject *self) {
    PyObject *entry = nb_enum_lookup(self);
    if (!entry)
        return nullptr;

    PyObject *name = nb_inst_name(self);
    PyObject *result =
        PyUnicode_FromFormat("%U.%U", name, NB_TUPLE_GET_ITEM(entry, 0));
    Py_DECREF(name);

    return result;
}

static PyObject *nb_enum_get_name(PyObject *self, void *) {
    PyObject *entry = nb_enum_lookup(self);
    if (!entry)
        return nullptr;

    PyObject *result = NB_TUPLE_GET_ITEM(entry, 0);
    Py_INCREF(result);
    return result;
}

static PyObject *nb_enum_get_doc(PyObject *self, void *) {
    PyObject *entry = nb_enum_lookup(self);
    if (!entry)
        return nullptr;

    PyObject *result = NB_TUPLE_GET_ITEM(entry, 1);
    Py_INCREF(result);
    return result;
}

NB_NOINLINE static PyObject *nb_enum_int_signed(PyObject *o) {
    type_data *t = nb_type_data(Py_TYPE(o));
    const void *p = inst_ptr((nb_inst *) o);
    long long value;
    switch (t->size) {
        case 1: value = (long long) *(const int8_t *)  p; break;
        case 2: value = (long long) *(const int16_t *) p; break;
        case 4: value = (long long) *(const int32_t *) p; break;
        case 8: value = (long long) *(const int64_t *) p; break;
        default: PyErr_SetString(PyExc_TypeError, "nb_enum: invalid type size!");
                 return nullptr;
    }
    return PyLong_FromLongLong(value);
}

NB_NOINLINE static PyObject *nb_enum_int_unsigned(PyObject *o) {
    type_data *t = nb_type_data(Py_TYPE(o));
    const void *p = inst_ptr((nb_inst *) o);
    unsigned long long value;
    switch (t->size) {
        case 1: value = (unsigned long long) *(const uint8_t *)  p; break;
        case 2: value = (unsigned long long) *(const uint16_t *) p; break;
        case 4: value = (unsigned long long) *(const uint32_t *) p; break;
        case 8: value = (unsigned long long) *(const uint64_t *) p; break;
        default: PyErr_SetString(PyExc_TypeError, "nb_enum: invalid type size!");
                 return nullptr;
    }
    return PyLong_FromUnsignedLongLong(value);
}

static PyObject *nb_enum_init(PyObject *, PyObject *, PyObject *) {
    return 0;
}

static PyObject *nb_enum_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    PyObject *arg;

    if (kwds || NB_TUPLE_GET_SIZE(args) != 1)
        goto error;

    arg = NB_TUPLE_GET_ITEM(args, 0);
    if (PyLong_Check(arg)) {
        enum_supplement &supp = nb_enum_supplement(subtype);
        if (!supp.entries)
            goto error;

        PyObject *item = PyDict_GetItem(supp.entries, arg);
        if (item && PyTuple_CheckExact(item) && NB_TUPLE_GET_SIZE(item) == 3) {
            item = NB_TUPLE_GET_ITEM(item, 2);
            Py_INCREF(item);
            return item;
        }
    } else if (Py_TYPE(arg) == subtype) {
        Py_INCREF(arg);
        return arg;
    }

error:
    PyErr_Clear();
    PyErr_Format(PyExc_RuntimeError,
                 "%s(): could not convert the input into an enumeration value!",
                 nb_type_data(subtype)->name);
    return nullptr;
}

static PyGetSetDef nb_enum_getset[] = {
    { "__doc__", nb_enum_get_doc, nullptr, nullptr, nullptr },
    { "__name__", nb_enum_get_name, nullptr, nullptr, nullptr },
    { nullptr, nullptr, nullptr, nullptr, nullptr }
};

PyObject *nb_enum_richcompare(PyObject *a, PyObject *b, int op) {
    // SomeType.tp_richcompare(a, b, op) is always invoked with 'a'
    // having type SomeType. Note that this is different than binary
    // arithmetic operations because comparisons can be reversed;
    // Python will ask type(a) to check 'a > b' if type(b) doesn't
    // know how to check 'b < a'.

    if (op == Py_EQ || op == Py_NE) {
        // For equality/inequality comparisons, only allow enums to be
        // equal with their same enum type or with their underlying
        // value as an integer.  This is a little awkward (it breaks
        // transitivity of equality) but it's better than allowing
        // 'Shape.CIRCLE == Color.RED' to be true just because both
        // enumerators have the same underlying value (which would
        // also prevent putting both enumerators in the same set or as
        // keys in the same dictionary).
        if (Py_TYPE(a) != Py_TYPE(b) && !PyLong_Check(b)) {
            Py_RETURN_NOTIMPLEMENTED;
        }
    } else {
        // For ordering, allow comparison against any number,
        // including floats. Note that enums count as a number for
        // purposes of this check (it's anything that defines a __float__,
        // __int__, or __index__ slot).
        if (!PyNumber_Check(b)) {
            Py_RETURN_NOTIMPLEMENTED;
        }
    }

    PyObject *ia = PyNumber_Index(a); // must succeed since a is an enum
    PyObject *ib = nullptr;
    if (PyIndex_Check(b)) {
        // If b can be converted losslessly to an integer (which includes
        // the case where b is also an enum) then do that.
        ib = PyNumber_Index(b);
    } else {
        // Otherwise do the comparison against b as-is, which will probably
        // wind up calling b's tp_richcompare for the reversed operation.
        ib = b;
        Py_INCREF(ib);
    }
    PyObject *result = nullptr;
    if (ia && ib) {
        result = PyObject_RichCompare(ia, ib, op);
    }
    Py_XDECREF(ia);
    Py_XDECREF(ib);
    return result;
}

// Unary operands are easy because we know the argument will be this enum type
#define NB_ENUM_UNOP(name, op)                                                 \
    PyObject *nb_enum_##name(PyObject *a) {                                    \
        PyObject *ia = PyNumber_Index(a);                                      \
        if (!ia)                                                               \
            return nullptr;                                                    \
        PyObject *result = op(ia);                                             \
        Py_DECREF(ia);                                                         \
        return result;                                                         \
    }

// Binary operands are trickier due to the potential for reversed operations.
// We know either a or b is an enum object, but not which one.
NB_NOINLINE PyObject *nb_enum_binop(PyObject *a, PyObject *b,
                                    PyObject* (*op)(PyObject*, PyObject*)) {
    // Both operands should be numbers. (Enums count as numbers because they
    // define nb_int and nb_index slots.)
    if (!PyNumber_Check(a) || !PyNumber_Check(b)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    // Convert operands that support __index__ (lossless integer conversion),
    // including enums, to that integer. Leave other kinds of numbers (such
    // as floats and Decimals) alone. Then repeat the operation.
    // Note that we can assume at least one of the PyNumber_Index calls
    // succeeds, since one of our arguments is an enum.
    PyObject *ia = nullptr, *ib = nullptr, *result = nullptr;
    if (PyIndex_Check(a)) {
        ia = PyNumber_Index(a);
    } else {
        ia = a;
        Py_INCREF(ia);
    }
    if (PyIndex_Check(b)) {
        ib = PyNumber_Index(b);
    } else {
        ib = b;
        Py_INCREF(ib);
    }
    if (ia == a && ib == b) {
        PyErr_SetString(PyExc_SystemError,
                        "nanobind enum arithmetic invoked without an enum "
                        "as either operand");
    } else if (ia && ib) {
        result = op(ia, ib);
    }
    Py_XDECREF(ia);
    Py_XDECREF(ib);
    return result;
}

#define NB_ENUM_BINOP(name, op)                                                \
    PyObject *nb_enum_##name(PyObject *a, PyObject *b) {                       \
        return nb_enum_binop(a, b, op);                                        \
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

int nb_enum_clear(PyObject *) {
    return 0;
}

int nb_enum_traverse(PyObject *o, visitproc visit, void *arg) {
    Py_VISIT(Py_TYPE(o));
    return 0;
}

Py_hash_t nb_enum_hash(PyObject *o) {
    Py_hash_t value = 0;
    type_data *t = nb_type_data(Py_TYPE(o));
    const void *p = inst_ptr((nb_inst *) o);
    switch (t->size) {
        case 1: value = *(const int8_t *)  p; break;
        case 2: value = *(const int16_t *) p; break;
        case 4: value = *(const int32_t *) p; break;
        case 8: value = (Py_hash_t) * (const int64_t*)p; break;
        default:
            PyErr_SetString(PyExc_TypeError, "nb_enum: invalid type size!");
            return -1;
    }

    // Hash functions should return -1 when an error occurred.
    // Return -2 that case, since hash(-1) also yields -2.
    if (value == -1) value = -2;

    return value;
}

void nb_enum_prepare(const type_init_data *td,
                     PyType_Slot *&t, size_t max_slots) noexcept {
    /* 22 is the number of slot assignments below. Update it if you add more.
       These built-in slots are added before any user-defined ones. */
    check(max_slots >= 22,
          "nanobind::detail::nb_enum_prepare(\"%s\"): ran out of "
          "type slots!", td->name);

    const enum_init_data *ed = static_cast<const enum_init_data *>(td);
    auto int_fn = ed->is_signed ? nb_enum_int_signed : nb_enum_int_unsigned;

    *t++ = { Py_tp_new, (void *) nb_enum_new };
    *t++ = { Py_tp_init, (void *) nb_enum_init };
    *t++ = { Py_tp_repr, (void *) nb_enum_repr };
    *t++ = { Py_tp_richcompare, (void *) nb_enum_richcompare };
    *t++ = { Py_nb_int, (void *) int_fn };
    *t++ = { Py_nb_index, (void *) int_fn };
    *t++ = { Py_tp_getset, (void *) nb_enum_getset };
    *t++ = { Py_tp_traverse, (void *) nb_enum_traverse };
    *t++ = { Py_tp_clear, (void *) nb_enum_clear };
    *t++ = { Py_tp_hash, (void *) nb_enum_hash };

    if (ed->is_arithmetic) {
        *t++ = { Py_nb_add, (void *) nb_enum_add };
        *t++ = { Py_nb_subtract, (void *) nb_enum_sub };
        *t++ = { Py_nb_multiply, (void *) nb_enum_mul };
        *t++ = { Py_nb_floor_divide, (void *) nb_enum_div };
        *t++ = { Py_nb_or, (void *) nb_enum_or };
        *t++ = { Py_nb_xor, (void *) nb_enum_xor };
        *t++ = { Py_nb_and, (void *) nb_enum_and };
        *t++ = { Py_nb_rshift, (void *) nb_enum_rshift };
        *t++ = { Py_nb_lshift, (void *) nb_enum_lshift };
        *t++ = { Py_nb_negative, (void *) nb_enum_neg };
        *t++ = { Py_nb_invert, (void *) nb_enum_inv };
        *t++ = { Py_nb_absolute, (void *) nb_enum_abs };
    }
}

void nb_enum_put(PyObject *type, const char *name, const void *value,
                 const char *doc) noexcept {
    PyObject *doc_obj, *rec, *int_val;
    enum_supplement &supp = nb_enum_supplement((PyTypeObject *) type);

    PyObject *name_obj = PyUnicode_InternFromString(name);
    if (doc) {
        doc_obj = PyUnicode_FromString(doc);
    } else {
        doc_obj = Py_None;
        Py_INCREF(Py_None);
    }

    nb_inst *inst = (nb_inst *) inst_new_int((PyTypeObject *) type);

    if (!doc_obj || !name_obj || !inst)
        goto error;

    rec = PyTuple_New(3);
    NB_TUPLE_SET_ITEM(rec, 0, name_obj);
    NB_TUPLE_SET_ITEM(rec, 1, doc_obj);
    NB_TUPLE_SET_ITEM(rec, 2, (PyObject *) inst);

    memcpy(inst_ptr(inst), value, nb_type_data((PyTypeObject *) type)->size);
    inst->destruct = false;
    inst->cpp_delete = false;
    inst->ready = true;

    if (PyObject_SetAttr(type, name_obj, (PyObject *) inst))
        goto error;

    int_val = supp.is_signed ? nb_enum_int_signed((PyObject *) inst)
                             : nb_enum_int_unsigned((PyObject *) inst);
    if (!int_val)
        goto error;

    if (!supp.entries) {
        PyObject *dict = PyDict_New();
        if (!dict)
            goto error;

        // Stash the entries dict in the type object's dict so that GC
        // can see the enumerators. nb_type_setattro ensures that user
        // code can't reassign or delete this attribute (its logic
        // is based on the @ prefix in the name).
        if (PyObject_SetAttrString(type, "@entries", dict))
            goto error;

        supp.entries = dict;
        Py_DECREF(dict);
    }

    if (PyDict_SetItem(supp.entries, int_val, rec))
        goto error;

    Py_DECREF(int_val);
    Py_DECREF(rec);

    return;

error:
    check(false,
          "nanobind::detail::nb_enum_put(): could not create enum entry!");
}

void nb_enum_export(PyObject *tp) {
    enum_supplement &supp = nb_enum_supplement((PyTypeObject *) tp);
    check(supp.entries && supp.scope != nullptr,
          "nanobind::detail::nb_enum_export(): internal error!");

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(supp.entries, &pos, &key, &value)) {
        check(PyTuple_CheckExact(value) && NB_TUPLE_GET_SIZE(value) == 3,
              "nanobind::detail::nb_enum_export(): internal error! (2)");

        setattr(supp.scope,
                NB_TUPLE_GET_ITEM(value, 0),
                NB_TUPLE_GET_ITEM(value, 2));
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
