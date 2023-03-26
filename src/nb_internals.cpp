/*
    src/internals.cpp: internal libnanobind data structures

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <structmember.h>
#include "nb_internals.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

/// Tracks the ABI of nanobind
#ifndef NB_INTERNALS_VERSION
#  define NB_INTERNALS_VERSION 7
#endif

/// On MSVC, debug and release builds are not ABI-compatible!
#if defined(_MSC_VER) && defined(_DEBUG)
#  define NB_BUILD_TYPE "_debug"
#else
#  define NB_BUILD_TYPE ""
#endif

/// Let's assume that different compilers are ABI-incompatible.
#if defined(_MSC_VER)
#  define NB_COMPILER_TYPE "_msvc"
#elif defined(__INTEL_COMPILER)
#  define NB_COMPILER_TYPE "_icc"
#elif defined(__clang__)
#  define NB_COMPILER_TYPE "_clang"
#elif defined(__PGI)
#  define NB_COMPILER_TYPE "_pgi"
#elif defined(__MINGW32__)
#  define NB_COMPILER_TYPE "_mingw"
#elif defined(__CYGWIN__)
#  define NB_COMPILER_TYPE "_gcc_cygwin"
#elif defined(__GNUC__)
#  define NB_COMPILER_TYPE "_gcc"
#else
#  define NB_COMPILER_TYPE "_unknown"
#endif

/// Also standard libs
#if defined(_LIBCPP_VERSION)
#  define NB_STDLIB "_libcpp"
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
#  define NB_STDLIB "_libstdcpp"
#else
#  define NB_STDLIB ""
#endif

/// On Linux/OSX, changes in __GXX_ABI_VERSION__ indicate ABI incompatibility.
#if defined(__GXX_ABI_VERSION)
#  define NB_BUILD_ABI "_cxxabi" NB_TOSTRING(__GXX_ABI_VERSION)
#else
#  define NB_BUILD_ABI ""
#endif

#if defined(Py_LIMITED_API)
#  define NB_LIMITED_API "_limited"
#else
#  define NB_LIMITED_API ""
#endif

#define NB_INTERNALS_ID "__nb_internals_v" \
    NB_TOSTRING(NB_INTERNALS_VERSION) NB_COMPILER_TYPE NB_STDLIB NB_BUILD_ABI NB_BUILD_TYPE NB_LIMITED_API "__"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

extern PyObject *nb_func_getattro(PyObject *, PyObject *);
extern PyObject *nb_func_get_doc(PyObject *, void *);
extern PyObject *nb_bound_method_getattro(PyObject *, PyObject *);
extern PyObject *nb_func_get_getset(PyObject *, PyObject *);
extern int nb_func_traverse(PyObject *, visitproc, void *);
extern int nb_func_clear(PyObject *);
extern void nb_func_dealloc(PyObject *);
extern int nb_bound_method_traverse(PyObject *, visitproc, void *);
extern int nb_bound_method_clear(PyObject *);
extern void nb_bound_method_dealloc(PyObject *);
extern PyObject *nb_method_descr_get(PyObject *, PyObject *, PyObject *);
extern int nb_type_setattro(PyObject*, PyObject*, PyObject*);
extern PyObject *nb_ndarray_get(PyObject *, PyObject *);
extern int nb_ndarray_getbuffer(PyObject *exporter, Py_buffer *view, int);
extern void nb_ndarray_releasebuffer(PyObject *, Py_buffer *);
extern void nb_ndarray_dealloc(PyObject *self);
static PyObject *nb_static_property_get(PyObject *, PyObject *, PyObject *);

#if PY_VERSION_HEX >= 0x03090000
#  define NB_HAVE_VECTORCALL_PY39_OR_NEWER NB_HAVE_VECTORCALL
#else
#  define NB_HAVE_VECTORCALL_PY39_OR_NEWER 0
#endif

static PyMemberDef nb_func_members[] = {
    { "__vectorcalloffset__", T_PYSSIZET,
      (Py_ssize_t) offsetof(nb_func, vectorcall), READONLY, nullptr },
    { nullptr, 0, 0, 0, nullptr }
};

static PyGetSetDef nb_func_getset[] = {
    { "__doc__", nb_func_get_doc, nullptr, nullptr, nullptr },
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
    /*.name = */"nanobind.nb_method",
    /*.basicsize = */(int) sizeof(nb_func),
    /*.itemsize = */(int) sizeof(func_data),
    /*.flags = */Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC
        | Py_TPFLAGS_METHOD_DESCRIPTOR
        | NB_HAVE_VECTORCALL_PY39_OR_NEWER,
    /*.slots = */nb_method_slots
};

static PyMemberDef nb_bound_method_members[] = {
    { "__vectorcalloffset__", T_PYSSIZET,
      (Py_ssize_t) offsetof(nb_bound_method, vectorcall), READONLY, nullptr },
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

static PyType_Slot nb_type_slots[] = {
    { Py_tp_base, nullptr },
    { Py_tp_dealloc, (void *) nb_type_dealloc },
    { Py_tp_setattro, (void *) nb_type_setattro },
    { Py_tp_init, (void *) nb_type_init },
    { 0, nullptr }
};

static PyType_Spec nb_type_spec = {
    /* .name = */ "nanobind.nb_type",
    /* .basicsize = */ 0,
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /* .slots = */ nb_type_slots
};

static PyType_Slot nb_enum_slots[] = {
    { Py_tp_base, nullptr },
    { 0, nullptr }
};

static PyType_Spec nb_enum_spec = {
    /*.name = */"nanobind.nb_enum",
    /*.basicsize = */0,
    /*.itemsize = */0,
    /*.flags = */Py_TPFLAGS_DEFAULT,
    /*.slots = */nb_enum_slots
};


#if PY_VERSION_HEX >= 0x030C0000
static PyMemberDef nb_static_property_members[] = {
    { "__doc__", T_OBJECT, 0, 0, nullptr },
    { nullptr, 0, 0, 0, nullptr }
};
#endif

static PyType_Slot nb_static_property_slots[] = {
    { Py_tp_base, nullptr },
    { Py_tp_members, nullptr },
    { Py_tp_descr_get, (void *) nb_static_property_get },
    { 0, nullptr }
};

static PyType_Spec nb_static_property_spec = {
    /* .name = */ "nanobind.nb_static_property",
    /* .basicsize = */ 0,
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT,
    /* .slots = */ nb_static_property_slots
};

static PyType_Slot nb_ndarray_slots[] = {
    { Py_tp_dealloc, (void *) nb_ndarray_dealloc },
#if PY_VERSION_HEX >= 0x03090000
    { Py_bf_getbuffer, (void *) nb_ndarray_getbuffer },
    { Py_bf_releasebuffer, (void *) nb_ndarray_releasebuffer },
#endif
    { 0, nullptr }
};

static PyType_Spec nb_ndarray_spec = {
    /* .name = */ "nanobind.nb_ndarray",
    /* .basicsize = */ (int) sizeof(nb_ndarray),
    /* .itemsize = */ 0,
    /* .flags = */ Py_TPFLAGS_DEFAULT,
    /* .slots = */ nb_ndarray_slots
};

/// `nb_static_property_property.__get__()`: Always pass the class instead of the instance.
static PyObject *nb_static_property_get(PyObject *self, PyObject *, PyObject *cls) {
    if (internals_get().nb_static_property_enabled) {
        #if defined(Py_LIMITED_API)
            static descrgetfunc tp_descr_get =
                (descrgetfunc) PyType_GetSlot(&PyProperty_Type, Py_tp_descr_get);
        #else
            descrgetfunc tp_descr_get = PyProperty_Type.tp_descr_get;
        #endif

        return tp_descr_get(self, cls, cls);
    } else {
        Py_INCREF(self);
        return self;
    }
}

/// `nb_static_property_property.__set__()`: Just like the above `__get__()`.
int nb_static_property_set(PyObject *self, PyObject *obj, PyObject *value) {
    PyObject *cls = PyType_Check(obj) ? obj : (PyObject *) Py_TYPE(obj);

    #if defined(Py_LIMITED_API)
        static descrsetfunc tp_descr_set =
            (descrsetfunc) PyType_GetSlot(&PyProperty_Type, Py_tp_descr_set);
    #else
        descrsetfunc tp_descr_set = PyProperty_Type.tp_descr_set;
    #endif

    return tp_descr_set(self, cls, value);
}

NB_THREAD_LOCAL current_method current_method_data =
    current_method{ nullptr, nullptr };

static nb_internals *internals_p = nullptr;

void default_exception_translator(const std::exception_ptr &p, void *) {
    try {
        std::rethrow_exception(p);
    } catch (python_error &e) {
        e.restore();
    } catch (const builtin_exception &e) {
        e.set_error();
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

#if !defined(PYPY_VERSION)
static void internals_cleanup() {
    bool leak = false;

    if (!internals_p->inst_c2p.empty()) {
        if (internals_p->print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu instances!\n",
                    internals_p->inst_c2p.size());
        }
        leak = true;
    }

    if (!internals_p->keep_alive.empty()) {
        if (internals_p->print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu keep_alive records!\n",
                    internals_p->keep_alive.size());
        }
        leak = true;
    }

    if (!internals_p->type_c2p.empty()) {
        if (internals_p->print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu types!\n",
                    internals_p->type_c2p.size());
            int ctr = 0;
            for (const auto &kv : internals_p->type_c2p) {
                fprintf(stderr, " - leaked type \"%s\"\n", kv.second->name);
                if (ctr++ == 10) {
                    fprintf(stderr, " - ... skipped remainder\n");
                    break;
                }
            }
        }
        leak = true;
    }

    if (!internals_p->funcs.empty()) {
        if (internals_p->print_leak_warnings) {
            fprintf(stderr, "nanobind: leaked %zu functions!\n",
                    internals_p->funcs.size());
            int ctr = 0;
            for (void *f : internals_p->funcs) {
                fprintf(stderr, " - leaked function \"%s\"\n",
                        nb_func_data(f)->name);
                if (ctr++ == 10) {
                    fprintf(stderr, " - ... skipped remainder\n");
                    break;
                }
            }
        }
        leak = true;
    }

    if (!leak) {
        delete internals_p;
        internals_p = nullptr;
    } else {
        if (internals_p->print_leak_warnings) {
            fprintf(stderr, "nanobind: this is likely caused by a reference "
                            "counting issue in the binding code.\n");
        }

#if NB_ABORT_ON_LEAK == 1
        abort(); // Extra-strict behavior for the CI server
#endif
    }
}
#endif

static PyObject *internals_dict() {
#if defined(PYPY_VERSION)
    PyObject *dict = PyEval_GetBuiltins();
#elif PY_VERSION_HEX < 0x03090000
    PyObject *dict = PyInterpreterState_GetDict(_PyInterpreterState_Get());
#else
    PyObject *dict = PyInterpreterState_GetDict(PyInterpreterState_Get());
#endif
    if (!dict)
        fail("nanobind::detail::internals_dict(): failed!");

    return dict;
}

static void internals_make() {
    str nb_name("nanobind");

    internals_p = new nb_internals();

    PyObject *dict = internals_dict();

    const char *internals_id = NB_INTERNALS_ID;
    PyObject *capsule = PyCapsule_New(internals_p, internals_id, nullptr);
    PyObject *nb_module = PyModule_NewObject(nb_name.ptr());
    int rv = PyDict_SetItemString(dict, internals_id, capsule);
    if (rv || !capsule || !nb_module)
        fail("nanobind::detail::internals_make(): allocation failed!");
    Py_DECREF(capsule);

    internals_p->nb_module = nb_module;

    // Function objects
    internals_p->nb_func = (PyTypeObject *) PyType_FromSpec(&nb_func_spec);
    internals_p->nb_method = (PyTypeObject *) PyType_FromSpec(&nb_method_spec);
    internals_p->nb_bound_method =
        (PyTypeObject *) PyType_FromSpec(&nb_bound_method_spec);

    // Metaclass #1 (nb_type)
#if defined(Py_LIMITED_API)
    int tp_itemsize = cast<int>(handle(&PyType_Type).attr("__itemsize__"));
    int tp_basicsize = cast<int>(handle(&PyType_Type).attr("__basicsize__"));
#else
    int tp_itemsize = (int) PyType_Type.tp_itemsize;
    int tp_basicsize = (int) PyType_Type.tp_basicsize;
#endif
    nb_type_spec.basicsize = nb_enum_spec.basicsize = tp_basicsize
        + (int) sizeof(type_data);
    nb_type_spec.itemsize = nb_enum_spec.itemsize = tp_itemsize;
    nb_type_slots[0].pfunc = &PyType_Type;
    internals_p->nb_type = (PyTypeObject *) PyType_FromSpec(&nb_type_spec);

    // Metaclass #2 (nb_enum)
    nb_enum_slots[0].pfunc = internals_p->nb_type;
    internals_p->nb_enum = (PyTypeObject *) PyType_FromSpec(&nb_enum_spec);

    /// Static properties
 #if defined(Py_LIMITED_API)
    tp_basicsize = cast<int>(handle(&PyProperty_Type).attr("__basicsize__"));
    tp_itemsize = cast<int>(handle(&PyProperty_Type).attr("__itemsize__"));
 #else
    tp_basicsize = (int) PyProperty_Type.tp_basicsize;
    tp_itemsize = (int) PyProperty_Type.tp_itemsize;
 #endif

    // See https://github.com/python/cpython/issues/98963
#if PY_VERSION_HEX >= 0x030C0000
    nb_static_property_members[0].offset = tp_basicsize;
    nb_static_property_slots[1].pfunc = nb_static_property_members;
    tp_basicsize += sizeof(PyObject *);
#else
    nb_static_property_slots[1].pfunc = PyProperty_Type.tp_members;
#endif
    nb_static_property_slots[0].pfunc = &PyProperty_Type;
    nb_static_property_spec.basicsize = tp_basicsize;
    nb_static_property_spec.itemsize = tp_itemsize;

    internals_p->nb_static_property =
        (PyTypeObject *) PyType_FromSpec(&nb_static_property_spec);
    internals_p->nb_static_property_enabled = true;

    // Tensor type
    internals_p->nb_ndarray = (PyTypeObject *) PyType_FromSpec(&nb_ndarray_spec);

    if (!internals_p->nb_func || !internals_p->nb_method ||
        !internals_p->nb_bound_method || !internals_p->nb_type ||
        !internals_p->nb_enum || !internals_p->nb_static_property ||
        !internals_p->nb_ndarray)
        fail("nanobind::detail::internals_make(): type initialization failed!");

#if PY_VERSION_HEX < 0x03090000
    internals_p->nb_ndarray->tp_as_buffer->bf_getbuffer = nb_ndarray_getbuffer;
    internals_p->nb_ndarray->tp_as_buffer->bf_releasebuffer = nb_ndarray_releasebuffer;
    internals_p->nb_func->tp_flags |= NB_HAVE_VECTORCALL;
    internals_p->nb_func->tp_vectorcall_offset = offsetof(nb_func, vectorcall);
    internals_p->nb_method->tp_flags |= NB_HAVE_VECTORCALL;
    internals_p->nb_method->tp_vectorcall_offset = offsetof(nb_func, vectorcall);
    internals_p->nb_bound_method->tp_flags |= NB_HAVE_VECTORCALL;
    internals_p->nb_bound_method->tp_vectorcall_offset = offsetof(nb_bound_method, vectorcall);
#endif

    register_exception_translator(default_exception_translator, nullptr);

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

#if !defined(PYPY_VERSION)
    /* Install the memory leak checker. This feature is unsupported on
       PyPy, see https://foss.heptapod.net/pypy/pypy/-/issues/3855 */
    if (Py_AtExit(internals_cleanup))
        fprintf(stderr,
                "Warning: could not install the nanobind cleanup handler! This "
                "is needed to check for reference leaks and release remaining "
                "resources at interpreter shutdown (e.g., to avoid leaks being "
                "reported by tools like 'valgrind'). If you are a user of a "
                "python extension library, you can ignore this warning.");
#endif
}

static void internals_fetch() {
    PyObject *dict = internals_dict();

    const char *internals_id = NB_INTERNALS_ID;
    PyObject *capsule = PyDict_GetItemString(dict, internals_id);

    if (capsule) {
        internals_p = (nb_internals *) PyCapsule_GetPointer(capsule, internals_id);
        if (!internals_p)
            fail("nanobind::detail::internals_fetch(): capsule pointer is NULL!");
        return;
    }

    internals_make();
}

nb_internals &internals_get() noexcept {
    if (!internals_p)
        internals_fetch();
    return *internals_p;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
