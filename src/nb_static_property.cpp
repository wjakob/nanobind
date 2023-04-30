#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// `nb_static_property.__get__()`: Always pass the class instead of the instance.
static PyObject *nb_static_property_descr_get(PyObject *self, PyObject *, PyObject *cls) {
    nb_internals &internals = internals_get();
    if (internals.nb_static_property_enabled) {
        return NB_SLOT(internals, PyProperty_Type, tp_descr_get)(self, cls, cls);
    } else {
        Py_INCREF(self);
        return self;
    }
}

/// `nb_static_property.__set__()`: Just like the above `__get__()`.
static int nb_static_property_descr_set(PyObject *self, PyObject *obj, PyObject *value) {
    PyObject *cls = PyType_Check(obj) ? obj : (PyObject *) Py_TYPE(obj);
    return NB_SLOT(internals_get(), PyProperty_Type, tp_descr_set)(self, cls, value);
}

PyTypeObject *nb_static_property_tp() noexcept {
    nb_internals &internals = internals_get();
    PyTypeObject *tp = internals.nb_static_property;

    if (NB_UNLIKELY(!tp)) {
        #if defined(Py_LIMITED_API)
            int basicsize = cast<int>(handle(&PyProperty_Type).attr("__basicsize__"));
            int itemsize = cast<int>(handle(&PyProperty_Type).attr("__itemsize__"));
        #else
            int basicsize = (int) PyProperty_Type.tp_basicsize;
            int itemsize = (int) PyProperty_Type.tp_itemsize;
        #endif

        PyType_Slot slots[] = {
            { Py_tp_base, &PyProperty_Type },
            { Py_tp_members, nullptr },
            { Py_tp_descr_get, (void *) nb_static_property_descr_get },
            { 0, nullptr }
        };

#if PY_VERSION_HEX < 0x030C0000
        slots[1].pfunc = PyProperty_Type.tp_members;
#else
        // See https://github.com/python/cpython/issues/98963
        PyMemberDef members[] = {
            { "__doc__", T_OBJECT, basicsize, 0, nullptr },
            { nullptr, 0, 0, 0, nullptr }
        };

        slots[1].pfunc = members;
        basicsize += sizeof(PyObject *);
#endif

        PyType_Spec spec = {
            /* .name = */ "nanobind.nb_static_property",
            /* .basicsize = */ basicsize,
            /* .itemsize = */ itemsize,
            /* .flags = */ Py_TPFLAGS_DEFAULT,
            /* .slots = */ slots
        };

        tp = (PyTypeObject *) PyType_FromSpec(&spec);
        if (!tp)
            fail("nb_static_property type creation failed!");

        internals.nb_static_property = tp;
        internals.nb_static_property_descr_set = nb_static_property_descr_set;
    }

    return tp;
}

NAMESPACE_END(NB_NAMESPACE)
NAMESPACE_END(detail)
