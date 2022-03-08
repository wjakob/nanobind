/*
    src/internals.cpp: internal libnanobind data structures

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <structmember.h>
#include "internals.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

/// Tracks the ABI of nanobind
#ifndef NB_INTERNALS_VERSION
#    define NB_INTERNALS_VERSION 1
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

#define NB_INTERNALS_ID "__nb_internals_v" \
    NB_TOSTRING(NB_INTERNALS_VERSION) NB_COMPILER_TYPE NB_STDLIB NB_BUILD_ABI NB_BUILD_TYPE "__"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// `nb_static_property_property.__get__()`: Always pass the class instead of the instance.
extern "C" inline PyObject *nb_static_property_get(PyObject *self, PyObject * /*ob*/, PyObject *cls) {
    return PyProperty_Type.tp_descr_get(self, cls, cls);
}

/// `nb_static_property_property.__set__()`: Just like the above `__get__()`.
extern "C" inline int nb_static_property_set(PyObject *self, PyObject *obj, PyObject *value) {
    PyObject *cls = PyType_Check(obj) ? obj : (PyObject *) Py_TYPE(obj);
    return PyProperty_Type.tp_descr_set(self, cls, value);
}

PyTypeObject nb_static_property_type = {
    .tp_name = "nb_static_property",
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "nanobind function object",
    .tp_descr_get = nb_static_property_get,
    .tp_descr_set = nb_static_property_set
};

thread_local current_method current_method_data;

static internals *internals_p = nullptr;

void default_exception_translator(const std::exception_ptr &p) {
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

static void internals_cleanup() {
    bool leak = false;

    if (!internals_p->inst_c2p.empty()) {
        fprintf(stderr, "nanobind: leaked %zu instances!\n",
                internals_p->inst_c2p.size());
        leak = true;
    }

    if (!internals_p->keep_alive.empty()) {
        fprintf(stderr, "nanobind: leaked %zu keep_alive records!\n",
                internals_p->keep_alive.size());
        leak = true;
    }

    if (!internals_p->type_c2p.empty()) {
        fprintf(stderr, "nanobind: leaked %zu types!\n",
                internals_p->type_c2p.size());
        for (const auto &kv : internals_p->type_c2p)
            fprintf(stderr, " - leaked type \"%s\"\n", kv.second->name);
        leak = true;
    }

    if (!internals_p->funcs.empty()) {
        fprintf(stderr, "nanobind: leaked %zu functions!\n",
                internals_p->funcs.size());
        for (void *f : internals_p->funcs)
            fprintf(stderr, " - leaked function \"%s\"\n",
                    nb_func_get(f)->name);
        leak = true;
    }

    if (!leak) {
        delete internals_p;
        internals_p = nullptr;
    } else {
        fprintf(stderr, "nanobind: this is likely caused by a reference "
                        "counting issue in the binding code.\n");
    }
}

static void internals_make() {
    internals_p = new internals();
    internals_p->exception_translators.push_back(default_exception_translator);

    PyObject *capsule = PyCapsule_New(internals_p, nullptr, nullptr);
    int rv = PyDict_SetItemString(PyEval_GetBuiltins(), NB_INTERNALS_ID, capsule);
    if (rv || !capsule)
        fail("nanobind::detail::internals_make(): could not install internals "
             "data structure!");
    Py_DECREF(capsule);

    nb_type_type.ob_base.ob_base.ob_refcnt = 1;
    nb_enum_type.ob_base.ob_base.ob_refcnt = 1;
    nb_func_type.ob_base.ob_base.ob_refcnt = 1;
    nb_meth_type.ob_base.ob_base.ob_refcnt = 1;
    nb_static_property_type.ob_base.ob_base.ob_refcnt = 1;

    nb_type_type.tp_base = &PyType_Type;
    nb_enum_type.tp_base = &nb_type_type;
    nb_enum_type.tp_clear = PyType_Type.tp_clear;
    nb_enum_type.tp_traverse = PyType_Type.tp_traverse;
    nb_static_property_type.tp_base = &PyProperty_Type;
    nb_static_property_type.tp_traverse = PyProperty_Type.tp_traverse;
    nb_static_property_type.tp_clear = PyProperty_Type.tp_clear;
    nb_static_property_type.tp_methods = PyProperty_Type.tp_methods;
    nb_static_property_type.tp_members = PyProperty_Type.tp_members;

    if (PyType_Ready(&nb_type_type) < 0 || PyType_Ready(&nb_func_type) < 0 ||
        PyType_Ready(&nb_meth_type) < 0 || PyType_Ready(&nb_enum_type) < 0 ||
        PyType_Ready(&nb_static_property_type))
        fail("nanobind::detail::internals_make(): type initialization failed!");

    if ((nb_type_type.tp_flags & Py_TPFLAGS_HEAPTYPE) != 0 ||
        nb_type_type.tp_basicsize != sizeof(PyHeapTypeObject) + sizeof(type_data) ||
        nb_type_type.tp_itemsize != sizeof(PyMemberDef))
        fail("nanobind::detail::internals_make(): initialized type invalid!");

    if (Py_AtExit(internals_cleanup))
        fprintf(stderr,
                "Warning: could not install the nanobind cleanup handler! This "
                "is needed to check for reference leaks and release remaining "
                "resources at interpreter shutdown (e.g., to avoid leaks being "
                "reported by tools like 'valgrind'). If you are a user of a "
                "python extension library, you can ignore this warning.");

    internals_p->nb_type = &nb_type_type;
    internals_p->nb_func = &nb_func_type;
    internals_p->nb_meth = &nb_meth_type;
    internals_p->nb_enum = &nb_enum_type;
    internals_p->nb_static_property = &nb_static_property_type;
}

static void internals_fetch() {
    PyObject *capsule =
        PyDict_GetItemString(PyEval_GetBuiltins(), NB_INTERNALS_ID);

    if (capsule) {
        internals_p = (internals *) PyCapsule_GetPointer(capsule, nullptr);
        if (!internals_p)
            fail("nanobind::detail::internals_fetch(): internal error!");
        return;
    }

    internals_make();
}

internals &internals_get() noexcept {
    if (!internals_p)
        internals_fetch();
    return *internals_p;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
