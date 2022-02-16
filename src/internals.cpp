#include <nanobind/nanobind.h>
#include <structmember.h>
#include "internals.h"

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

extern void nb_func_dealloc(PyObject *);
extern void nb_type_dealloc(PyObject *o);
extern PyObject *nb_func_call(PyObject *self, PyObject *, PyObject *);
extern PyObject *nb_func_get_doc(PyObject *, void *);
extern PyObject *nb_func_get_name(PyObject *, void *);
extern PyObject *nb_meth_descr_get(PyObject *, PyObject *, PyObject *);

    // {"__name__", (getter)meth_get__name__, nullptr, nullptr},
    // {"__qualname__", (getter)meth_get__qualname__, nullptr, nullptr},
    // {"__self__", (getter)meth_get__self__, nullptr, nullptr},
    // {"__text_signature__", (getter)meth_get__text_signature__, nullptr, nullptr},

static PyGetSetDef nb_func_getset[] = {
    { "__doc__", nb_func_get_doc, nullptr, nullptr, nullptr },
    { "__name__", nb_func_get_name, nullptr, nullptr, nullptr },
    { nullptr, nullptr, nullptr, nullptr, nullptr }
};

static PyTypeObject nb_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nb_type",
    .tp_doc = "nanobind metaclass",
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_dealloc = nb_type_dealloc,
    .tp_basicsize = sizeof(PyHeapTypeObject) + sizeof(type_data)
};

static PyTypeObject nb_func_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nb_func",
    .tp_doc = "nanobind function object",
    .tp_basicsize = sizeof(nb_func),
    .tp_itemsize = sizeof(func_record),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_VECTORCALL,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = nb_func_dealloc,
    .tp_vectorcall_offset = offsetof(nb_func, vectorcall),
    .tp_call = PyVectorcall_Call,
    .tp_getset = nb_func_getset,
    .tp_getattro = PyObject_GenericGetAttr
};

static PyTypeObject nb_meth_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nb_meth",
    .tp_doc = "nanobind method object",
    .tp_basicsize = sizeof(nb_func),
    .tp_itemsize = sizeof(func_record),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_VECTORCALL |
                Py_TPFLAGS_METHOD_DESCRIPTOR,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = nb_func_dealloc,
    .tp_vectorcall_offset = offsetof(nb_func, vectorcall),
    .tp_call = PyVectorcall_Call,
    .tp_getset = nb_func_getset,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_descr_get = nb_meth_descr_get
};

extern void type_dealloc(PyObject *);

static internals *internals_p = nullptr;

void default_exception_translator(std::exception_ptr p) {
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
    auto &inst_c2p = internals_p->inst_c2p;
    auto &type_c2p = internals_p->type_c2p;
    bool leak = false;

    if (!inst_c2p.empty()) {
        fprintf(stderr, "nanobind: leaked %zu instances!\n", inst_c2p.size());
        leak = true;
    }

    if (!type_c2p.empty()) {
        fprintf(stderr, "nanobind: leaked %zu types!\n", type_c2p.size());
        for (const auto &kv : type_c2p)
            fprintf(stderr, " - leaked type \"%s\"\n", kv.second->name);
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

    nb_type.tp_base = &PyType_Type;
    if (PyType_Ready(&nb_type) < 0 || PyType_Ready(&nb_func_type) < 0 ||
        PyType_Ready(&nb_meth_type) < 0)
        fail("nanobind::detail::internals_make(): type initialization failed!");

    if ((nb_type.tp_flags & Py_TPFLAGS_HEAPTYPE) != 0 ||
        nb_type.tp_basicsize != sizeof(PyHeapTypeObject) + sizeof(type_data) ||
        nb_type.tp_itemsize != sizeof(PyMemberDef))
        fail("nanobind::detail::internals_make(): initialized type invalid!");

    if (Py_AtExit(internals_cleanup))
        fprintf(stderr,
                "Warning: could not install the nanobind cleanup handler! This "
                "is needed to check for reference leaks and release remaining "
                "resources at interpreter shutdown (e.g., to avoid leaks being "
                "reported by tools like 'valgrind'). If you are a user of a "
                "python extension library, you can ignore this warning.");

    internals_p->nb_type = &nb_type;
    internals_p->nb_func = &nb_func_type;
    internals_p->nb_meth = &nb_meth_type;
    internals_p->keep_alive = PyDict_New();
    internals_p->funcs = PySet_New(nullptr);
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
