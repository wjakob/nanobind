/*
    src/internals.cpp: internal libnanobind data structures

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <structmember.h>
#include "nb_internals.h"
#include "nb_abi.h"
#include <thread>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

extern PyObject *nb_func_getattro(PyObject *, PyObject *);
extern PyObject *nb_func_get_doc(PyObject *, void *);
extern PyObject *nb_func_get_nb_signature(PyObject *, void *);
extern PyObject *nb_bound_method_getattro(PyObject *, PyObject *);
extern int nb_func_traverse(PyObject *, visitproc, void *);
extern int nb_func_clear(PyObject *);
extern void nb_func_dealloc(PyObject *);
extern int nb_bound_method_traverse(PyObject *, visitproc, void *);
extern int nb_bound_method_clear(PyObject *);
extern void nb_bound_method_dealloc(PyObject *);
extern PyObject *nb_method_descr_get(PyObject *, PyObject *, PyObject *);

#if PY_VERSION_HEX >= 0x03090000
#  define NB_HAVE_VECTORCALL_PY39_OR_NEWER NB_HAVE_VECTORCALL
#else
#  define NB_HAVE_VECTORCALL_PY39_OR_NEWER 0
#endif

static PyType_Slot nb_meta_slots[] = {
    { Py_tp_base, nullptr },
    { 0, nullptr }
};

static PyType_Spec nb_meta_spec = {
    /* .name = */ "nanobind.nb_meta",
    /* .basicsize = */ 0,
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT,
    /* .slots = */ nb_meta_slots
};

static PyMemberDef nb_func_members[] = {
    { "__vectorcalloffset__", T_PYSSIZET,
      (Py_ssize_t) offsetof(nb_func, vectorcall), READONLY, nullptr },
    { nullptr, 0, 0, 0, nullptr }
};

static PyGetSetDef nb_func_getset[] = {
    { "__doc__", nb_func_get_doc, nullptr, nullptr, nullptr },
    { "__nb_signature__", nb_func_get_nb_signature, nullptr, nullptr, nullptr },
    { nullptr, nullptr, nullptr, nullptr, nullptr }
};

static PyType_Slot nb_func_slots[] = {
    { Py_tp_members, (void *) nb_func_members },
    { Py_tp_getset, (void *) nb_func_getset },
    { Py_tp_getattro, (void *) nb_func_getattro },
    { Py_tp_traverse, (void *) nb_func_traverse },
    { Py_tp_clear, (void *) nb_func_clear },
    { Py_tp_dealloc, (void *) nb_func_dealloc },
    { Py_tp_traverse, (void *) nb_func_traverse },
    { Py_tp_new, (void *) PyType_GenericNew },
    { Py_tp_call, (void *) PyVectorcall_Call },
    { 0, nullptr }
};

static PyType_Spec nb_func_spec = {
    /* .name = */ "nanobind.nb_func",
    /* .basicsize = */ (int) sizeof(nb_func),
    /* .itemsize = */ (int) sizeof(func_data),
    /* .flags = */ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
                   NB_HAVE_VECTORCALL_PY39_OR_NEWER,
    /* .slots = */ nb_func_slots
};

static PyType_Slot nb_method_slots[] = {
    { Py_tp_members, (void *) nb_func_members },
    { Py_tp_getset, (void *) nb_func_getset },
    { Py_tp_getattro, (void *) nb_func_getattro },
    { Py_tp_traverse, (void *) nb_func_traverse },
    { Py_tp_clear, (void *) nb_func_clear },
    { Py_tp_dealloc, (void *) nb_func_dealloc },
    { Py_tp_descr_get, (void *) nb_method_descr_get },
    { Py_tp_new, (void *) PyType_GenericNew },
    { Py_tp_call, (void *) PyVectorcall_Call },
    { 0, nullptr }
};

static PyType_Spec nb_method_spec = {
    /*.name = */ "nanobind.nb_method",
    /*.basicsize = */ (int) sizeof(nb_func),
    /*.itemsize = */ (int) sizeof(func_data),
    /*.flags = */ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
                  Py_TPFLAGS_METHOD_DESCRIPTOR |
                  NB_HAVE_VECTORCALL_PY39_OR_NEWER,
    /*.slots = */ nb_method_slots
};

static PyMemberDef nb_bound_method_members[] = {
    { "__vectorcalloffset__", T_PYSSIZET,
      (Py_ssize_t) offsetof(nb_bound_method, vectorcall), READONLY, nullptr },
    { "__func__", T_OBJECT_EX,
      (Py_ssize_t) offsetof(nb_bound_method, func), READONLY, nullptr },
    { "__self__", T_OBJECT_EX,
      (Py_ssize_t) offsetof(nb_bound_method, self), READONLY, nullptr },
    { nullptr, 0, 0, 0, nullptr }
};

static PyType_Slot nb_bound_method_slots[] = {
    { Py_tp_members, (void *) nb_bound_method_members },
    { Py_tp_getattro, (void *) nb_bound_method_getattro },
    { Py_tp_traverse, (void *) nb_bound_method_traverse },
    { Py_tp_clear, (void *) nb_bound_method_clear },
    { Py_tp_dealloc, (void *) nb_bound_method_dealloc },
    { Py_tp_traverse, (void *) nb_bound_method_traverse },
    { Py_tp_call, (void *) PyVectorcall_Call },
    { 0, nullptr }
};

static PyType_Spec nb_bound_method_spec = {
    /* .name = */ "nanobind.nb_bound_method",
    /* .basicsize = */ (int) sizeof(nb_bound_method),
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
                   NB_HAVE_VECTORCALL_PY39_OR_NEWER,
    /* .slots = */ nb_bound_method_slots
};

void default_exception_translator(const std::exception_ptr &p, void *) {
    try {
        std::rethrow_exception(p);
    } catch (const std::bad_alloc &e) {
        PyErr_SetString(PyExc_MemoryError, e.what());
    } catch (const std::domain_error &e) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } catch (const std::invalid_argument &e) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } catch (const std::length_error &e) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } catch (const std::out_of_range &e) {
        PyErr_SetString(PyExc_IndexError, e.what());
    } catch (const std::range_error &e) {
        PyErr_SetString(PyExc_ValueError, e.what());
    } catch (const std::overflow_error &e) {
        PyErr_SetString(PyExc_OverflowError, e.what());
    } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

// Initialized once when the module is loaded, no locking needed
nb_internals *internals = nullptr;
PyTypeObject *nb_meta_cache = nullptr;

static bool is_alive_value = false;
static bool *is_alive_ptr = &is_alive_value;
bool is_alive() noexcept { return *is_alive_ptr; }

const char *abi_tag() { return NB_ABI_TAG; }

static void internals_cleanup() {
    nb_internals *p = internals;
    if (!p)
        return;

    *is_alive_ptr = false;

#if !defined(PYPY_VERSION) && !defined(NB_FREE_THREADED)
    /* The memory leak checker is unsupported on PyPy, see
       see https://foss.heptapod.net/pypy/pypy/-/issues/3855.

       Leak reporting is explicitly disabled on free-threaded builds
       for now because of the decision to immortalize function and
       type objects. This may change in the future. */

    bool print_leak_warnings = p->print_leak_warnings;

    size_t inst_leaks = 0, keep_alive_leaks = 0;

    // Shard locking no longer needed, Py_AtExit is single-threaded
    for (size_t i = 0; i < p->shard_count; ++i) {
        nb_shard &s = p->shards[i];
        inst_leaks += s.inst_c2p.size();
        keep_alive_leaks += s.keep_alive.size();
    }

#ifdef _DEBUG
// in debug mode, show all leak records
#define INC_CTR do {} while(0)
#else
// otherwise show just the first 10 or 20
#define INC_CTR ctr++
#endif

    bool leak = inst_leaks > 0 || keep_alive_leaks > 0;

    if (print_leak_warnings && inst_leaks > 0) {
        fprintf(stderr, "nanobind: leaked %zu instances!\n", inst_leaks);

#if !defined(Py_LIMITED_API)
        auto print_leak = [](void* k, PyObject* v) {
            type_data *tp = nb_type_data(Py_TYPE(v));
            fprintf(stderr, " - leaked instance %p of type \"%s\"\n", k, tp->name);
        };

        int ctr = 0;
        for (size_t i = 0; i < p->shard_count && ctr < 20; ++i) {
            for (auto [k, v]: p->shards[i].inst_c2p) {
                if (NB_UNLIKELY(nb_is_seq(v))) {
                    nb_inst_seq* seq = nb_get_seq(v);
                    for(; seq != nullptr && ctr < 20; seq = seq->next) {
                        print_leak(k, seq->inst);
                        INC_CTR;
                    }
                } else {
                    print_leak(k, (PyObject*)v);
                    INC_CTR;
                }
                if (ctr >= 20)
                    break;
            }
        }
        if (ctr >= 20) {
            fprintf(stderr, " - ... skipped remainder\n");
        }
#endif
    }

    if (print_leak_warnings && keep_alive_leaks > 0)
        fprintf(stderr, "nanobind: leaked %zu keep_alive records!\n",
                keep_alive_leaks);

    // Only report function/type leaks if actual nanobind instances were leaked
#if !defined(NB_ABORT_ON_LEAK)
    if (!leak)
        print_leak_warnings = false;
#endif

    if (!p->type_c2p_slow.empty()) {
        if (print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu types!\n",
                    p->type_c2p_slow.size());
            int ctr = 0;
            for (const auto &kv : p->type_c2p_slow) {
                fprintf(stderr, " - leaked type \"%s\"\n", kv.second->name);
                INC_CTR;
                if (ctr == 10) {
                    fprintf(stderr, " - ... skipped remainder\n");
                    break;
                }
            }
        }
        leak = true;
    }

    if (!p->funcs.empty()) {
        if (print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu functions!\n",
                    p->funcs.size());
            int ctr = 0;
            for (auto [f, p2] : p->funcs) {
                fprintf(stderr, " - leaked function \"%s\"\n",
                        nb_func_data(f)->name);
                if (ctr == 10) {
                    INC_CTR;
                    fprintf(stderr, " - ... skipped remainder\n");
                    break;
                }
            }
        }
        leak = true;
    }

    if (!leak) {
        nb_translator_seq* t = p->translators.next;
        while (t) {
            nb_translator_seq *next = t->next;
            delete t;
            t = next;
        }

#if defined(NB_FREE_THREADED)
        // This code won't run for now but is kept here for a time when
        // immortalization isn't needed anymore.

        PyThread_tss_delete(p->nb_static_property_disabled);
        PyThread_tss_free(p->nb_static_property_disabled);
        delete[] p->shards;
#endif

        delete p;
        internals = nullptr;
        nb_meta_cache = nullptr;
    } else {
        if (print_leak_warnings) {
            fprintf(stderr, "nanobind: this is likely caused by a reference "
                            "counting issue in the binding code.\n"
                            "See https://nanobind.readthedocs.io/en/latest/refleaks.html");
        }

        #if defined(NB_ABORT_ON_LEAK) && !defined(NB_FREE_THREADED)
            abort(); // Extra-strict behavior for the CI server
        #endif
    }
#endif
}

NB_NOINLINE void init(const char *name) {
    if (internals)
        return;

#if defined(PYPY_VERSION)
    PyObject *dict = PyEval_GetBuiltins();
#elif PY_VERSION_HEX < 0x03090000
    PyObject *dict = PyInterpreterState_GetDict(_PyInterpreterState_Get());
#else
    PyObject *dict = PyInterpreterState_GetDict(PyInterpreterState_Get());
#endif
    check(dict, "nanobind::detail::init(): could not access internals dictionary!");

    PyObject *key = PyUnicode_FromFormat("__nb_internals_%s_%s__",
                                         abi_tag(), name ? name : "");
    check(key, "nanobind::detail::init(): could not create dictionary key!");

    PyObject *capsule = dict_get_item_ref_or_fail(dict, key);
    if (capsule) {
        Py_DECREF(key);
        internals = (nb_internals *) PyCapsule_GetPointer(capsule, "nb_internals");
        check(internals,
              "nanobind::detail::internals_fetch(): capsule pointer is NULL!");
        nb_meta_cache = internals->nb_meta;
        is_alive_ptr = internals->is_alive_ptr;
        Py_DECREF(capsule);
        return;
    }

    nb_internals *p = new nb_internals();

    size_t shard_count = 1;
#if defined(NB_FREE_THREADED)
    size_t hw_concurrency = std::thread::hardware_concurrency();
    while (shard_count < hw_concurrency)
        shard_count *= 2;
    shard_count *= 2;
    p->shards = new nb_shard[shard_count];
    p->shard_mask = shard_count - 1;
#endif
    p->shard_count = shard_count;

    str nb_name("nanobind");
    p->nb_module = PyModule_NewObject(nb_name.ptr());

    nb_meta_slots[0].pfunc = (PyObject *) &PyType_Type;
    nb_meta_cache = p->nb_meta = (PyTypeObject *) PyType_FromSpec(&nb_meta_spec);
    p->nb_type_dict = PyDict_New();
    p->nb_func = (PyTypeObject *) PyType_FromSpec(&nb_func_spec);
    p->nb_method = (PyTypeObject *) PyType_FromSpec(&nb_method_spec);
    p->nb_bound_method = (PyTypeObject *) PyType_FromSpec(&nb_bound_method_spec);

#if defined(NB_FREE_THREADED)
    p->nb_static_property_disabled = PyThread_tss_alloc();
    PyThread_tss_create(p->nb_static_property_disabled);
#endif

    for (size_t i = 0; i < shard_count; ++i) {
        p->shards[i].keep_alive.min_load_factor(.1f);
        p->shards[i].inst_c2p.min_load_factor(.1f);
    }

    check(p->nb_module && p->nb_meta && p->nb_type_dict && p->nb_func &&
              p->nb_method && p->nb_bound_method,
          "nanobind::detail::init(): initialization failed!");

#if PY_VERSION_HEX < 0x03090000
    p->nb_func->tp_flags |= NB_HAVE_VECTORCALL;
    p->nb_func->tp_vectorcall_offset = offsetof(nb_func, vectorcall);
    p->nb_method->tp_flags |= NB_HAVE_VECTORCALL;
    p->nb_method->tp_vectorcall_offset = offsetof(nb_func, vectorcall);
    p->nb_bound_method->tp_flags |= NB_HAVE_VECTORCALL;
    p->nb_bound_method->tp_vectorcall_offset = offsetof(nb_bound_method, vectorcall);
#endif

#if defined(Py_LIMITED_API)
    // Cache important functions from PyType_Type and PyProperty_Type
    p->PyType_Type_tp_free = (freefunc) PyType_GetSlot(&PyType_Type, Py_tp_free);
    p->PyType_Type_tp_init = (initproc) PyType_GetSlot(&PyType_Type, Py_tp_init);
    p->PyType_Type_tp_dealloc =
        (destructor) PyType_GetSlot(&PyType_Type, Py_tp_dealloc);
    p->PyType_Type_tp_setattro =
        (setattrofunc) PyType_GetSlot(&PyType_Type, Py_tp_setattro);
    p->PyProperty_Type_tp_descr_get =
        (descrgetfunc) PyType_GetSlot(&PyProperty_Type, Py_tp_descr_get);
    p->PyProperty_Type_tp_descr_set =
        (descrsetfunc) PyType_GetSlot(&PyProperty_Type, Py_tp_descr_set);

    PyType_Slot dummy_slots[] = {
        { Py_tp_base, &PyType_Type },
        { 0, nullptr }
    };

    PyType_Spec dummy_spec = {
        /* .name = */ "nanobind.dummy",
        /* .basicsize = */ - (int) sizeof(void*),
        /* .itemsize = */ 0,
        /* .flags = */ Py_TPFLAGS_DEFAULT,
        /* .slots = */ dummy_slots
    };

    // Determine the offset, at which types defined by nanobind begin
    PyObject *dummy = PyType_FromMetaclass(
        p->nb_meta, p->nb_module, &dummy_spec, nullptr);
    p->type_data_offset =
        (uint8_t *) PyObject_GetTypeData(dummy, p->nb_meta) - (uint8_t *) dummy;
    Py_DECREF(dummy);
#endif

    p->translators = { default_exception_translator, nullptr, nullptr };
    is_alive_value = true;
    is_alive_ptr = &is_alive_value;
    p->is_alive_ptr = is_alive_ptr;

#if PY_VERSION_HEX < 0x030C0000 && !defined(PYPY_VERSION)
    /* The implementation of typing.py on CPython <3.12 tends to introduce
       spurious reference leaks that upset nanobind's leak checker. The
       following band-aid, installs an 'atexit' handler that clears LRU caches
       used in typing.py. To be resilient to potential future changes in
       typing.py, the implementation fails silently if any step goes wrong. For
       context, see https://github.com/python/cpython/issues/98253. */

    const char *str =
        "def cleanup():\n"
        "    try:\n"
        "        import sys\n"
        "        fs = getattr(sys.modules.get('typing'), '_cleanups', None)\n"
        "        if fs is not None:\n"
        "            for f in fs:\n"
        "                f()\n"
        "    except:\n"
        "        pass\n"
        "import atexit\n"
        "atexit.register(cleanup)\n"
        "del atexit, cleanup";

    PyObject *code = Py_CompileString(str, "<internal>", Py_file_input);
    if (code) {
        PyObject *result = PyEval_EvalCode(code, PyEval_GetGlobals(), nullptr);
        if (!result)
            PyErr_Clear();
        Py_XDECREF(result);
        Py_DECREF(code);
    } else {
        PyErr_Clear();
    }
#endif

    if (Py_AtExit(internals_cleanup))
        fprintf(stderr,
                "Warning: could not install the nanobind cleanup handler! This "
                "is needed to check for reference leaks and release remaining "
                "resources at interpreter shutdown (e.g., to avoid leaks being "
                "reported by tools like 'valgrind'). If you are a user of a "
                "python extension library, you can ignore this warning.");

    capsule = PyCapsule_New(p, "nb_internals", nullptr);
    int rv = PyDict_SetItem(dict, key, capsule);
    check(!rv && capsule,
          "nanobind::detail::init(): capsule creation failed!");
    Py_DECREF(capsule);
    Py_DECREF(key);
    internals = p;
}

#if defined(NB_COMPACT_ASSERTIONS)
NB_NOINLINE void fail_unspecified() noexcept {
    #if defined(NB_COMPACT_ASSERTION_MESSAGE)
        fail(NB_COMPACT_ASSERTION_MESSAGE);
    #else
        fail("encountered an unrecoverable error condition. Recompile using the"
             " 'Debug' mode to obtain further information about this problem.");
    #endif
}
#endif

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
