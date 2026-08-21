/* This extension does not use NB_MODULE(). It sets up the module by hand, the
   way another binding framework would, and prepares nanobind via
   nb::register_module(). */

#include <nanobind/nanobind.h>

namespace nb = nanobind;

struct Foreign {
    int value;
};

static int exec(PyObject *m) {
    if (!nb::register_module(m))
        return -1;

    // Registering an already prepared module is a no-op
    if (!nb::register_module(m))
        return -1;

    nb::module_ mod = nb::borrow<nb::module_>(m);

    nb::class_<Foreign>(mod, "Foreign")
        .def(nb::init<int>())
        .def_rw("value", &Foreign::value)
        .def("__repr__", [](const Foreign &f) {
            return nb::str("Foreign({})").format(f.value);
        });

    mod.def("roundtrip", [](Foreign f) { return f.value; });

    return 0;
}

#if defined(Py_TARGET_ABI3T)

// abi3t-style module definition

PyABIInfo_VAR(abi_info);

static PySlot slots[] = {
    PySlot_PTR(Py_mod_exec, exec),
    PySlot_PTR(Py_mod_abi, &abi_info),
    PySlot_PTR(Py_mod_gil, Py_MOD_GIL_NOT_USED),
    // PySlot_END, spelled out to avoid -Wmissing-field-initializers
    { Py_slot_end, 0, { 0 }, { nullptr } }
};

PyMODEXPORT_FUNC PyModExport_test_foreign_ext(void) {
    return slots;
}

#else

// classic module definition

static PyModuleDef_Slot slots[] = {
    { Py_mod_exec, (void *) exec },
#if defined(NB_FREE_THREADED)
    { Py_mod_gil, Py_MOD_GIL_NOT_USED },
#endif
    { 0, nullptr }
};

static PyModuleDef def = {
    PyModuleDef_HEAD_INIT,
    /* m_name = */ "test_foreign_ext",
    /* m_doc = */ nullptr,
    /* m_size = */ 0,
    /* m_methods = */ nullptr,
    /* m_slots = */ slots,
    /* m_traverse = */ nullptr,
    /* m_clear = */ nullptr,
    /* m_free = */ nullptr
};

extern "C" [[maybe_unused]] NB_EXPORT PyObject *PyInit_test_foreign_ext(void);
extern "C" PyObject *PyInit_test_foreign_ext(void) {
    return PyModuleDef_Init(&def);
}

#endif
