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

#if defined(NB_FREE_THREADED)
#  if defined(_WIN32)
#    include <windows.h>
#  else
#    include <pthread.h>
#  endif
#endif

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


static PyType_Slot nb_meta_slots[] = {
    { Py_tp_base, nullptr },
    { 0, nullptr }
};

static PyType_Spec nb_meta_spec = {
    /* .name = */ "nanobind.nb_meta",
    /* .basicsize = */ 0,
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT |
                   NB_TPFLAGS_IMMUTABLETYPE,
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
    { Py_tp_new, (void *) PyType_GenericNew },
    { Py_tp_call, (void *) PyVectorcall_Call },
    { 0, nullptr }
};

static PyType_Spec nb_func_spec = {
    /* .name = */ "nanobind.nb_func",
    /* .basicsize = */ (int) sizeof(nb_func),
    /* .itemsize = */ (int) sizeof(func_data),
    /* .flags = */ Py_TPFLAGS_DEFAULT |
                   Py_TPFLAGS_HAVE_GC |
                   Py_TPFLAGS_HAVE_VECTORCALL |
                   NB_TPFLAGS_IMMUTABLETYPE,
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
    /*.flags = */ Py_TPFLAGS_DEFAULT |
                  Py_TPFLAGS_HAVE_GC |
                  Py_TPFLAGS_METHOD_DESCRIPTOR |
                  Py_TPFLAGS_HAVE_VECTORCALL |
                  NB_TPFLAGS_IMMUTABLETYPE,
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
    { Py_tp_call, (void *) PyVectorcall_Call },
    { 0, nullptr }
};

static PyType_Spec nb_bound_method_spec = {
    /* .name = */ "nanobind.nb_bound_method",
    /* .basicsize = */ (int) sizeof(nb_bound_method),
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT |
                   Py_TPFLAGS_HAVE_GC |
                   Py_TPFLAGS_HAVE_VECTORCALL |
                   NB_TPFLAGS_IMMUTABLETYPE,
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

#if defined(NB_FREE_THREADED)
NB_THREAD_LOCAL nb_thread_state *nb_thread_state_tls = nullptr;

// Reclaims a thread's state when it exits (the cleanup-key callback).
static void nb_thread_state_destroy(void *p) noexcept {
    nb_thread_state *ts = (nb_thread_state *) p;
    if (!ts)
        return;

    // Reclaim this thread's instance pools if the runtime is still alive
    if (internals && ts->pools) {
        PyGILState_STATE state = PyGILState_Ensure();
        for (uint32_t i = 0; i < ts->pools_size; ++i)
            nb_pool_drain(&ts->pools[i], /* can_free = */ true);
        PyGILState_Release(state);
    }
    PyMem_Free(ts->pools);

    nb_thread_state_tls = nullptr;
    delete ts;
}

// Slow path for nb_thread_state_get(): allocate the per-thread state with a cleanup callback
nb_thread_state *nb_thread_state_alloc() noexcept {
#if defined(_WIN32)
    DWORD key = internals->thread_state_key;
    nb_thread_state *ts = (nb_thread_state *) FlsGetValue(key);
    if (!ts) {
        ts = new nb_thread_state();
        check(FlsSetValue(key, ts), "nanobind: FlsSetValue() failed!");
    }
#else
    pthread_key_t key = internals->thread_state_key;
    nb_thread_state *ts = (nb_thread_state *) pthread_getspecific(key);
    if (!ts) {
        ts = new nb_thread_state();
        check(pthread_setspecific(key, ts) == 0,
              "nanobind: pthread_setspecific() failed!");
    }
#endif
    nb_thread_state_tls = ts;
    return ts;
}
#endif


static const char* interned_c_strs[pyobj_name::string_count] {
    #define NB_INTERNED_ENTRY(name) #name,
    NB_INTERNED_STRINGS(NB_INTERNED_ENTRY)
    #undef NB_INTERNED_ENTRY
};

PyObject *static_pyobjects[pyobj_name::total_count] = {};

static void new_constant(nb_internals *p, int index, PyObject *o) {
    static_pyobjects[index] = o;
    new_object(p, o);
}

/// Lifeline generation against which this library's static_pyobjects[] was
/// populated; a mismatch indicates stale entries from a destroyed lifeline.
static uint32_t static_pyobjects_generation = 0;

/// Populate this library's static_pyobjects[]
static void init_pyobjects(nb_internals *p) {
    if (static_pyobjects[0] &&
        static_pyobjects_generation == p->lifeline_generation)
        return;

    static_pyobjects_generation = p->lifeline_generation;

    NB_NOUNROLL
    for (int i = 0; i < pyobj_name::string_count; ++i)
        new_constant(p, i, PyUnicode_InternFromString(interned_c_strs[i]));

    new_constant(p, pyobj_name::interned_max_version_tpl,
                 PyTuple_Pack(1, NB_INTERNED(max_version)));

    PyObject *one = PyLong_FromLong(1), *zero = PyLong_FromLong(0);
    new_constant(p, pyobj_name::interned_dl_cpu_tpl, PyTuple_Pack(2, one, zero));
    Py_DECREF(zero);
    Py_DECREF(one);

    PyObject *major = PyLong_FromLong(dlpack::major_version),
             *minor = PyLong_FromLong(dlpack::minor_version);
    new_constant(p, pyobj_name::interned_dl_version_tpl, PyTuple_Pack(2, major, minor));
    Py_DECREF(minor);
    Py_DECREF(major);
}

/// Create lifeline + internal types if needed
static void init_internals(nb_internals *p) {
    if (p->lifeline)
        return;

    p->lifeline = PyList_New(0);
    check(p->lifeline, "nanobind::detail::nb_module_exec(): "
                        "could not create lifeline list!");

    str nb_name("nanobind");
    p->nb_module = PyModule_NewObject(nb_name.ptr());
    new_object(p, p->nb_module);

    // Construct nanobind's meta-meta class
    nb_meta_slots[0].pfunc = (PyObject *) &PyType_Type;
    PyTypeObject *nb_meta = new_type(p, &nb_meta_spec);

    p->nb_func         = new_type(p, &nb_func_spec);
    p->nb_method       = new_type(p, &nb_method_spec);
    p->nb_bound_method = new_type(p, &nb_bound_method_spec);

    check(p->nb_module && nb_meta && p->nb_func &&
              p->nb_method && p->nb_bound_method,
          "nanobind::detail::nb_module_exec(): initialization failed!");

#if defined(Py_LIMITED_API)
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

    PyObject *dummy = PyType_FromMetaclass(
        nb_meta, p->nb_module, &dummy_spec, nullptr);
    p->type_data_offset =
        ((uint8_t *) PyObject_GetTypeData(dummy, nb_meta) - (uint8_t *) dummy);
    Py_DECREF(dummy);
#endif

    // Create the single metaclass shared by all bound types. This may
    // access 'type_data_offset' defined just above.
    p->nb_type = nb_type_create_metaclass(p, nb_meta);
    check(p->nb_type, "nanobind::detail::nb_module_exec(): "
                      "nb_type metaclass creation failed!");
}

void internals_inc_ref() {
    internals->shared_ref_count.value++;
}

void internals_dec_ref() {
    nb_internals *p = internals;
    auto value = --p->shared_ref_count.value;
    if (value != 0)
        return;

    // Invalidate every library's cached 'static_pyobjects' array: destroying
    // the lifeline frees the objects that these arrays reference.
    p->lifeline_generation++;

    Py_CLEAR(p->lifeline);

    p->nb_module = nullptr;
    p->nb_type = nullptr;
    p->nb_func = nullptr;
    p->nb_method = nullptr;
    p->nb_bound_method = nullptr;
    p->nb_static_property.store_release(nullptr);
    p->nb_ndarray.store_release(nullptr);
    for (auto &entry : p->ndarray_export)
        entry.store_release(nullptr);

    for (int i = 0; i < pyobj_name::total_count; ++i)
        static_pyobjects[i] = nullptr;
}

void nb_module_free(void *) {
    internals_dec_ref();
}


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

    // Unmap pooled instances to avoid false leaks (can_free=false: no thread state here).
    for (const auto &kv : p->type_c2p_slow) {
        type_data *td = kv.second;
        if (td->flags & (uint32_t) type_flags::pooled)
            nb_pool_drain(&td->pool, /* can_free = */ false);
    }

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
                INC_CTR;
                if (ctr == 10) {
                    fprintf(stderr, " - ... skipped remainder\n");
                    break;
                }
            }
        }
        leak = true;
    }

    if (!leak) {
        nb_translator_seq* t = p->translators.load_relaxed();
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

NB_NOINLINE void nb_module_exec(const char *name, PyObject *) {
    if (internals) {
        init_internals(internals);
        init_pyobjects(internals);
        internals_inc_ref();
        return;
    }

#if defined(PYPY_VERSION)
    PyObject *dict = PyEval_GetBuiltins();
#else
    PyObject *dict = PyInterpreterState_GetDict(PyInterpreterState_Get());
#endif
    check(dict, "nanobind::detail::nb_module_exec(): "
                "could not access internals dictionary!");

    PyObject *key = PyUnicode_FromFormat("__nb_internals_%s_%s__",
                                         abi_tag(), name ? name : "");
    check(key, "nanobind::detail::nb_module_exec(): "
               "could not create dictionary key!");

    PyObject *capsule = dict_getitem_or_default(dict, key, nullptr);
    if (capsule) {
        Py_DECREF(key);
        internals = (nb_internals *) PyCapsule_GetPointer(capsule, "nb_internals");
        check(internals, "nanobind::detail::nb_module_exec(): "
                         "capsule pointer is NULL!");
        is_alive_ptr = internals->is_alive_ptr;

        init_internals(internals);
        init_pyobjects(internals);
        internals_inc_ref();

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

    // Per-domain key for reclaiming nb_thread_state at thread exit
#if defined(_WIN32)
    p->thread_state_key = FlsAlloc((PFLS_CALLBACK_FUNCTION) nb_thread_state_destroy);
    check(p->thread_state_key != FLS_OUT_OF_INDEXES, "nanobind: FlsAlloc() failed!");
#else
    check(pthread_key_create(&p->thread_state_key, nb_thread_state_destroy) == 0,
          "nanobind: pthread_key_create() failed!");
#endif
#endif
    p->shard_count = shard_count;

    internals = p;

    init_internals(p);
    init_pyobjects(p);

#if defined(NB_FREE_THREADED)
    p->nb_static_property_disabled = PyThread_tss_alloc();
    PyThread_tss_create(p->nb_static_property_disabled);
#endif

    p->translators.store_release(
        new nb_translator_seq{ default_exception_translator, nullptr, nullptr });

    is_alive_value = true;
    is_alive_ptr = &is_alive_value;
    p->is_alive_ptr = is_alive_ptr;

    internals_inc_ref();

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
    check(capsule,
          "nanobind::detail::nb_module_exec(): capsule creation failed!");
    check(PyDict_SetItem(dict, key, capsule) == 0,
          "nanobind::detail::nb_module_exec(): could not register the "
          "internals capsule!");
    Py_DECREF(capsule);
    Py_DECREF(key);
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
