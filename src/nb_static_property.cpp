#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// `nb_static_property.__get__()`: Always pass the class instead of the instance.
static PyObject *nb_static_property_descr_get(PyObject *self, PyObject *, PyObject *cls) {

    // Flag to avoid infinite recursion during static attribute assignment
    bool static_property_disabled;
#if defined(NB_FREE_THREADED)
    static_property_disabled = (bool) PyThread_tss_get(internals->nb_static_property_disabled);
#else
    static_property_disabled = internals->nb_static_property_disabled;
#endif

    if (!static_property_disabled) {
        return NB_SLOT(PyProperty_Type, tp_descr_get)(self, cls, cls);
    } else {
        Py_INCREF(self);
        return self;
    }
}

/// `nb_static_property.__set__()`: Just like the above `__get__()`.
static int nb_static_property_descr_set(PyObject *self, PyObject *obj, PyObject *value) {
    PyObject *cls = PyType_Check(obj) ? obj : (PyObject *) Py_TYPE(obj);
    return NB_SLOT(PyProperty_Type, tp_descr_set)(self, cls, value);
}

PyTypeObject *nb_static_property_tp() noexcept {
    nb_internals *internals_ = internals;
    PyTypeObject *tp = internals_->nb_static_property.load_acquire();

    if (NB_UNLIKELY(!tp)) {
        lock_internals guard(internals_);

        tp = internals_->nb_static_property.load_relaxed();
        if (tp)
            return tp;

        PyMemberDef *members;

        #if defined(Py_LIMITED_API)
            members = (PyMemberDef *) PyType_GetSlot(&PyProperty_Type, Py_tp_members);
        #else
            members = PyProperty_Type.tp_members;
        #endif

        PyType_Slot slots[] = {
            { Py_tp_base, &PyProperty_Type },
            { Py_tp_descr_get, (void *) nb_static_property_descr_get },
            { Py_tp_members, members },
            { 0, nullptr }
        };

        PyType_Spec spec = {
            /* .name = */ "nanobind.nb_static_property",
            /* .basicsize = */ 0,
            /* .itemsize = */ 0,
            /* .flags = */ Py_TPFLAGS_DEFAULT,
            /* .slots = */ slots
        };

        tp = (PyTypeObject *) PyType_FromSpec(&spec);
        check(tp, "nb_static_property type creation failed!");

        internals_->nb_static_property_descr_set = nb_static_property_descr_set;
        internals_->nb_static_property.store_release(tp);
    }

    return tp;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
