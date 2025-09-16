#include <string.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/shared_ptr.h>
#include "inter_module.h"
#include "delattr_and_ensure_destroyed.h"
#include "../src/pymetabind.h"

namespace nb = nanobind;

// The following is a manual binding to `struct Shared`, created using the
// CPython C API only.

struct raw_shared_instance {
    PyObject_HEAD
    uintptr_t spacer[2]; // ensure instance layout differs from nanobind's
    bool deallocate;
    Shared *ptr;
    Shared value;
    PyObject *weakrefs;
};

static void Shared_dealloc(struct raw_shared_instance *self) {
    if (self->spacer[0] != 0x5a5a5a5a || self->spacer[1] != 0xa5a5a5a5)
        nb::detail::fail("instance corrupted");
    if (self->weakrefs)
        PyObject_ClearWeakRefs((PyObject *) self);
    if (self->deallocate)
        free(self->ptr);

    PyTypeObject *tp = Py_TYPE((PyObject *) self);
    PyObject_Free(self);
    Py_DECREF(tp);
}

static PyObject *Shared_new(PyTypeObject *type, Shared *value, pymb_rv_policy rvp) {
    struct raw_shared_instance *self;
    self = PyObject_New(raw_shared_instance, type);
    if (self) {
        memset((char *) self + sizeof(PyObject), 0,
               sizeof(*self) - sizeof(PyObject));
        self->spacer[0] = 0x5a5a5a5a;
        self->spacer[1] = 0xa5a5a5a5;
        switch (rvp) {
            case pymb_rv_policy_take_ownership:
                self->ptr = value;
                self->deallocate = true;
                break;
            case pymb_rv_policy_copy:
            case pymb_rv_policy_move:
                memcpy(&self->value, value, sizeof(Shared));
                self->ptr = &self->value;
                self->deallocate = false;
                break;
            case pymb_rv_policy_reference:
            case pymb_rv_policy_share_ownership:
                self->ptr = value;
                self->deallocate = false;
                break;
            default:
                nb::detail::fail("unhandled rvp %d", (int) rvp);
                break;
        }
    }
    return (PyObject *) self;
}

static int Shared_init(struct raw_shared_instance *, PyObject *, PyObject *) {
    PyErr_SetString(PyExc_TypeError, "cannot be constructed from Python");
    return -1;
}

// And a minimal implementation for our "foreign framework" of the pymetabind
// interface, so nanobind can use raw_shared_instances.

static void *hook_from_python(pymb_binding *binding,
                              PyObject *pyobj,
                              uint8_t,
                              void (*)(void *ctx, PyObject *obj),
                              void *) noexcept {
    if (binding->pytype != Py_TYPE(pyobj))
        return nullptr;
    return ((raw_shared_instance *) pyobj)->ptr;
}

static PyObject *hook_to_python(pymb_binding *binding,
                                void *cobj,
                                enum pymb_rv_policy rvp,
                                pymb_to_python_feedback *feedback) noexcept {
    feedback->relocate = 0;
    if (rvp == pymb_rv_policy_none)
        return nullptr;
    feedback->is_new = 1;
    return Shared_new(binding->pytype, (Shared *) cobj, rvp);
}

static void hook_ignore_foreign_binding(pymb_binding *) noexcept {}
static void hook_ignore_foreign_framework(pymb_framework *) noexcept {}

NB_MODULE(test_inter_module_foreign_ext, m) {
    static PyMemberDef Shared_members[] = {
        {"__weaklistoffset__", T_PYSSIZET,
         offsetof(struct raw_shared_instance, weakrefs), READONLY, nullptr},
        {nullptr, 0, 0, 0, nullptr},
    };
    static PyType_Slot Shared_slots[] = {
        {Py_tp_doc, (void *) "Shared object"},
        {Py_tp_init, (void *) Shared_init},
        {Py_tp_dealloc, (void *) Shared_dealloc},
        {Py_tp_members, (void *) Shared_members},
        {0, nullptr},
    };
    static PyType_Spec Shared_spec = {
        /* name */ "test_inter_module_foreign_ext.RawShared",
        /* basicsize */ sizeof(struct raw_shared_instance),
        /* itemsize */ 0,
        /* flags */ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
        /* slots */ Shared_slots,
    };

    static auto *registry = pymb_get_registry();
    if (!registry)
        throw nb::python_error();

    static auto *fw = new pymb_framework{};
    fw->name = "example framework for nanobind tests";
    fw->flags = pymb_framework_leak_safe;
    fw->abi_lang = pymb_abi_lang_c;
    fw->from_python = hook_from_python;
    fw->to_python = hook_to_python;
    fw->keep_alive = [](PyObject *, void *, void (*)(void *)) noexcept {
        return 0;
    };
    fw->remove_local_binding = [](pymb_binding *) noexcept {};
    fw->free_local_binding = [](pymb_binding *binding) noexcept {
        delete binding;
    };
    fw->add_foreign_binding = hook_ignore_foreign_binding;
    fw->remove_foreign_binding = hook_ignore_foreign_binding;
    fw->add_foreign_framework = hook_ignore_foreign_framework;
    fw->remove_foreign_framework = hook_ignore_foreign_framework;

    pymb_add_framework(registry, fw);
    int res = Py_AtExit(+[]() {
        pymb_remove_framework(fw);
        delete fw;
    });
    if (res != 0)
        throw nb::python_error();

    m.def("export_raw_binding", [hm = nb::handle(m)]() {
        auto type = hm.attr("RawShared");
        auto *binding = new pymb_binding{};
        binding->framework = fw;
        binding->pytype = (PyTypeObject *) type.ptr();
        binding->source_name = "Shared";
        pymb_add_binding(binding, /* tp_finalize_will_remove */ 0);
        nb::import_for_interop<Shared>(type);
    });

    m.def("create_raw_binding", [hm = nb::handle(m)]() {
        auto *type = (PyTypeObject *) PyType_FromSpec(&Shared_spec);
        if (!type)
            throw nb::python_error();
#if PY_VERSION_HEX < 0x03090000
        // __weaklistoffset__ member wasn't parsed until 3.9
        type->tp_weaklistoffset = offsetof(struct raw_shared_instance, weakrefs);
#endif
        hm.attr("RawShared") = nb::steal(type);
        hm.attr("export_raw_binding")();
    });
    m.attr("create_raw_binding")();

    m.def("remove_raw_binding", [hm = nb::handle(m)]() {
        delattr_and_ensure_destroyed(hm, "RawShared");
    });

    m.def("create_nb_binding", [hm = nb::handle(m)]() {
        nb::class_<Shared>(hm, "NbShared");
    });

    m.def("import_for_interop", &nb::import_for_interop<>);
    m.def("export_for_interop", &nb::export_for_interop);
    m.def("import_all", []() {
        nb::interoperate_by_default(false, true);
    });
    m.def("export_all", []() {
        nb::interoperate_by_default(true, false);
    });

    m.def("remove_all_bindings", [hm = nb::handle(m)]() {
        // NB: this is not a general purpose solution; the bindings removed
        // here won't be re-added if `import_all` is called
        nb::list bound;
        pymb_lock_registry(registry);
        PYMB_LIST_FOREACH(struct pymb_binding*, binding, registry->bindings) {
            bound.append(nb::borrow(binding->pytype));
        }
        pymb_unlock_registry(registry);
        for (auto type : bound) {
            nb::delattr(type, "__pymetabind_binding__");
        }

        // Restore the ability for our own create_shared() etc to work
        // properly, since that's a foreign type relationship too
        hm.attr("export_raw_binding")();
        nb::import_for_interop<Shared>(hm.attr("RawShared"));
    });

    m.def("create_shared", &create_shared);
    m.def("create_shared_sp", &create_shared_sp);
    m.def("create_shared_up", &create_shared_up);
    m.def("create_enum", &create_enum);
    m.def("check_shared", &check_shared);
    m.def("check_shared_sp", &check_shared_sp);
    m.def("check_shared_up", &check_shared_up);
    m.def("check_enum", &check_enum);
    m.def("throw_shared", &throw_shared);

    struct Convertible { int value; };
    nb::class_<Convertible>(m, "Convertible")
        .def("__init__", [](Convertible *self, const Shared &arg) {
            new (self) Convertible{arg.value};
        })
        .def_ro("value", &Convertible::value);
    nb::implicitly_convertible<Shared, Convertible>();
    m.def("test_implicit", [](Convertible conv) { return conv; });
}
