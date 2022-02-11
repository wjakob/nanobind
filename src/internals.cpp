#include <nanobind/nanobind.h>
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

extern void type_free(PyObject *);

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

static PyTypeObject* metaclass(const char *name) {
    str name_py(name);

    /* Danger zone: from now (and until PyType_Ready), make sure to
       issue no Python C API calls which could potentially invoke the
       garbage collector (the GC will call type_traverse(), which will in
       turn find the newly constructed type in an invalid state) */
    PyHeapTypeObject *heap_type =
        (PyHeapTypeObject *) PyType_Type.tp_alloc(&PyType_Type, 0);
    if (!heap_type)
        fail("nanobind::detail::metaclass(\"%s\"): alloc. failed!", name);

    heap_type->ht_name = name_py.inc_ref().ptr();
    heap_type->ht_qualname = name_py.inc_ref().ptr();

    PyTypeObject *type = &heap_type->ht_type;
    type->tp_name = name;
    type->tp_base = &PyType_Type;
    Py_INCREF(type->tp_base);

    type->tp_flags =
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HEAPTYPE;

    // type->tp_call = pybind11_meta_call;
    // type->tp_setattro = pybind11_meta_setattro;
    // type->tp_getattro = pybind11_meta_getattro;
    type->tp_dealloc = type_free;

    if (PyType_Ready(type) < 0)
        fail("nanobind::detail::metaclass(\"%s\"): PyType_Ready() failed!", name);

    setattr((PyObject *) type, "__module__", str("nb_builtins"));

    return type;
}

static void make_internals() {
    internals_p = new internals();
    internals_p->exception_translators.push_back(default_exception_translator);

    PyObject *capsule = PyCapsule_New(internals_p, nullptr, nullptr);
    int rv = PyDict_SetItemString(PyEval_GetBuiltins(), NB_INTERNALS_ID, capsule);
    if (rv || !capsule)
        fail("nanobind::detail::make_internals(): internal error!");
    Py_DECREF(capsule);

    internals_p->metaclass = metaclass("nb_type");
}

static void fetch_internals() {
    PyObject *capsule =
        PyDict_GetItemString(PyEval_GetBuiltins(), NB_INTERNALS_ID);

    if (capsule) {
        internals_p = (internals *) PyCapsule_GetPointer(capsule, nullptr);
        if (!internals_p)
            fail("nanobind::detail::fetch_internals(): internal error!");
        return;
    }

    make_internals();
}

internals &get_internals() noexcept {
    if (!internals_p)
        fetch_internals();
    return *internals_p;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
