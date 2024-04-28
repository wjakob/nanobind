#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(Py_LIMITED_API) || NB_PY_VERSION_MIN < 0x03090000
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#endif

// Select code based on oldest python version we need to support
#if NB_PY_VERSION_MIN < 0x03090000
#define NB_PY_39(newer, older) older
#else
#define NB_PY_39(newer, older) newer
#endif

// Select code based on limited API version
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x03090000
#define NB_PY_LIMITED_39(api, fallback) fallback
#else
#define NB_PY_LIMITED_39(api, fallback) api
#endif

#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030A0000
#define NB_PY_LIMITED_310(api, fallback) fallback
#else
#define NB_PY_LIMITED_310(api, fallback) api
#endif

#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030B0000
#define NB_PY_LIMITED_311(api, fallback) fallback
#else
#define NB_PY_LIMITED_311(api, fallback) api
#endif

#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030C0000
#define NB_PY_LIMITED_312(api, fallback) fallback
#else
#define NB_PY_LIMITED_312(api, fallback) api
#endif

// Get a pointer to a function/variable at runtime in the limited API mode.
// It may not exist in Python.h (because of the stable API) but willl be
// available at runtime. Cache the result so we only look it up once.
#define NB_DYN_SYM_CACHE(sym, type)                                            \
    ([]() {                                                                    \
        static auto ptr = reinterpret_cast<type>(detail::dyn_symbol(sym));     \
        return ptr;                                                            \
    }())

// Get a fuction using compat_ function declaration for the type, uncached
#define NB_COMPAT_SYM(sym)                                                     \
    reinterpret_cast<decltype(&compat_##sym)>(detail::dyn_symbol(#sym))

// Use a symbol defined in stable API version 3.X or later. Otherwise get it
// dynamically in earlier versions where it exists in the DLL anyway.
#define NB_DYN_SYM_VER(ver, sym, type)                                         \
    (NB_PY_LIMITED_##ver(&sym, NB_DYN_SYM_CACHE(#sym, type)))

// Same, but using compat_ function declaration for the type
#define NB_COMPAT_SYM_VER(ver, sym)                                            \
    NB_DYN_SYM_VER(ver, sym, decltype(&compat_##sym))

// Use a symbol defined in stable API version 3.X or later. Not defined in older
// versions at all.
#define NB_NEW_DYN_SYM_VER(ver, sym, type)                                         \
    (NB_PY_LIMITED_##ver(NB_PY_##ver(&sym, nullptr), NB_DYN_SYM_CACHE(#sym, type)))

// The below is valid C++ without Python.h using the following stable abi
// placeholder declarations, so it should be independent of the version of
// python we're compiling against.
extern "C" {
// Just use Python.h decls normally, allow linting to ensure we're not
// accidently using things we shouldn't
#ifndef PY_VERSION_HEX
struct PyObject;
struct PyInterpreterState;
struct PyThreadState;
struct PyTypeObject;
struct PyType_Spec;
typedef struct bufferinfo Py_buffer;
typedef size_t Py_ssize_t;
typedef Py_ssize_t Py_hash_t;
typedef PyObject *(*unaryfunc)(PyObject *);
typedef PyObject *(*binaryfunc)(PyObject *, PyObject *);
typedef PyObject *(*ternaryfunc)(PyObject *, PyObject *, PyObject *);
typedef int (*inquiry)(PyObject *);
typedef Py_ssize_t (*lenfunc)(PyObject *);
typedef PyObject *(*ssizeargfunc)(PyObject *, Py_ssize_t);
typedef int (*ssizeobjargproc)(PyObject *, Py_ssize_t, PyObject *);
typedef int (*objobjargproc)(PyObject *, PyObject *, PyObject *);
typedef int (*objobjproc)(PyObject *, PyObject *);
typedef int (*visitproc)(PyObject *, void *);
typedef int (*traverseproc)(PyObject *, visitproc, void *);
typedef void (*freefunc)(void *);
typedef void (*destructor)(PyObject *);
typedef PyObject *(*getattrfunc)(PyObject *, char *);
typedef PyObject *(*getattrofunc)(PyObject *, PyObject *);
typedef int (*setattrfunc)(PyObject *, char *, PyObject *);
typedef int (*setattrofunc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*reprfunc)(PyObject *);
typedef Py_hash_t (*hashfunc)(PyObject *);
typedef PyObject *(*richcmpfunc)(PyObject *, PyObject *, int);
typedef PyObject *(*getiterfunc)(PyObject *);
typedef PyObject *(*iternextfunc)(PyObject *);
typedef PyObject *(*descrgetfunc)(PyObject *, PyObject *, PyObject *);
typedef int (*descrsetfunc)(PyObject *, PyObject *, PyObject *);
typedef int (*initproc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*newfunc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*allocfunc)(PyObject *, Py_ssize_t);
const char *Py_GetVersion(void);
PyThreadState *PyThreadState_Get();
#endif
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030B0000
// Buffer API is not declared stable before 3.11, but in practice it is so
// declare it
typedef struct bufferinfo {
    void *buf;
    PyObject *obj; /* owned reference */
    Py_ssize_t len;
    Py_ssize_t itemsize; /* This is Py_ssize_t so it can be
                            pointed to by strides in simple case.*/
    int readonly;
    int ndim;
    char *format;
    Py_ssize_t *shape;
    Py_ssize_t *strides;
    Py_ssize_t *suboffsets;
    void *internal;
} Py_buffer;

#define PyBUF_SIMPLE 0
#define PyBUF_WRITABLE 0x0001

#define PyBUF_FORMAT 0x0004
#define PyBUF_ND 0x0008
#define PyBUF_STRIDES (0x0010 | PyBUF_ND)
#define PyBUF_C_CONTIGUOUS (0x0020 | PyBUF_STRIDES)
#define PyBUF_F_CONTIGUOUS (0x0040 | PyBUF_STRIDES)
#define PyBUF_ANY_CONTIGUOUS (0x0080 | PyBUF_STRIDES)
#define PyBUF_INDIRECT (0x0100 | PyBUF_STRIDES)

#define PyBUF_CONTIG (PyBUF_ND | PyBUF_WRITABLE)
#define PyBUF_CONTIG_RO (PyBUF_ND)

#define PyBUF_STRIDED (PyBUF_STRIDES | PyBUF_WRITABLE)
#define PyBUF_STRIDED_RO (PyBUF_STRIDES)

#define PyBUF_RECORDS (PyBUF_STRIDES | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_RECORDS_RO (PyBUF_STRIDES | PyBUF_FORMAT)

#define PyBUF_FULL (PyBUF_INDIRECT | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_FULL_RO (PyBUF_INDIRECT | PyBUF_FORMAT)

#define PyBUF_READ 0x100
#define PyBUF_WRITE 0x200
#endif
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030C0000
typedef int (*getbufferproc)(PyObject *, Py_buffer *, int);
typedef void (*releasebufferproc)(PyObject *, Py_buffer *);
typedef PyObject *(*vectorcallfunc)(PyObject *callable, PyObject *const *args,
                                    size_t nargsf, PyObject *kwnames);
#endif
}

#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030A0000
#define METH_FASTCALL 0x0080
#endif

//  Fill in missing bits from the stable API for python 3.8 - 3.11.
//  The contents of the pyXX structs below are copied directly from the relevant
//  python version's headers.

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#if defined(Py_LIMITED_API) || PY_VERSION_HEX < 0x03090000

// Get a symbol from a DLL
// Useful for getting symbols that only exist in particular versions of python
inline auto dyn_symbol(const char *name) {
#if defined(_WIN32)
    FARPROC ptr = nullptr;
    HMODULE module;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&Py_GetVersion, &module))

        ptr = GetProcAddress(module, name);
#else
    auto ptr = dlsym(RTLD_DEFAULT, name);
#endif
    if (ptr == nullptr)
        nanobind::detail::fail("Unable to find function %s in dll", name);
    return ptr;
}

// Header code taken directly from Python.h, nested into structs to namespace
// them in such a way that the whole thing can be used as a template parameter

struct py38_311 {
    // In practice the following are identical for 3.8-3.11

    struct _object {
        Py_ssize_t ob_refcnt;
        struct _typeobject *ob_type;
    };

    typedef struct {
        struct _object ob_base;
        Py_ssize_t ob_size; /* Number of items in variable part */
    } PyVarObject;

    typedef struct {
        /* Number implementations must check *both*
           arguments for proper type and implement the necessary conversions
           in the slot functions themselves. */

        binaryfunc nb_add;
        binaryfunc nb_subtract;
        binaryfunc nb_multiply;
        binaryfunc nb_remainder;
        binaryfunc nb_divmod;
        ternaryfunc nb_power;
        unaryfunc nb_negative;
        unaryfunc nb_positive;
        unaryfunc nb_absolute;
        inquiry nb_bool;
        unaryfunc nb_invert;
        binaryfunc nb_lshift;
        binaryfunc nb_rshift;
        binaryfunc nb_and;
        binaryfunc nb_xor;
        binaryfunc nb_or;
        unaryfunc nb_int;
        void *nb_reserved; /* the slot formerly known as nb_long */
        unaryfunc nb_float;

        binaryfunc nb_inplace_add;
        binaryfunc nb_inplace_subtract;
        binaryfunc nb_inplace_multiply;
        binaryfunc nb_inplace_remainder;
        ternaryfunc nb_inplace_power;
        binaryfunc nb_inplace_lshift;
        binaryfunc nb_inplace_rshift;
        binaryfunc nb_inplace_and;
        binaryfunc nb_inplace_xor;
        binaryfunc nb_inplace_or;

        binaryfunc nb_floor_divide;
        binaryfunc nb_true_divide;
        binaryfunc nb_inplace_floor_divide;
        binaryfunc nb_inplace_true_divide;

        unaryfunc nb_index;

        binaryfunc nb_matrix_multiply;
        binaryfunc nb_inplace_matrix_multiply;
    } PyNumberMethods;

    typedef struct {
        lenfunc sq_length;
        binaryfunc sq_concat;
        ssizeargfunc sq_repeat;
        ssizeargfunc sq_item;
        void *was_sq_slice;
        ssizeobjargproc sq_ass_item;
        void *was_sq_ass_slice;
        objobjproc sq_contains;

        binaryfunc sq_inplace_concat;
        ssizeargfunc sq_inplace_repeat;
    } PySequenceMethods;

    typedef struct {
        lenfunc mp_length;
        binaryfunc mp_subscript;
        objobjargproc mp_ass_subscript;
    } PyMappingMethods;

    typedef struct {
        getbufferproc bf_getbuffer;
        releasebufferproc bf_releasebuffer;
    } PyBufferProcs;

    /* Allow printfunc in the tp_vectorcall_offset slot for
     * backwards-compatibility */
    typedef Py_ssize_t printfunc;

    struct _dictkeysobject;

    template <class T>
    static inline int PyType_HasFeature_(T *type, unsigned long feature) {
        return (type->tp_flags & feature) != 0;
    }

    static constexpr unsigned long Py_TPFLAGS_HAVE_VECTORCALL_ = (1UL << 11);

    static constexpr size_t PY_VECTORCALL_ARGUMENTS_OFFSET_ =
        ((size_t)1 << (8 * sizeof(size_t) - 1));

    static inline Py_ssize_t PyVectorcall_NARGS(size_t n) {
        return n & ~PY_VECTORCALL_ARGUMENTS_OFFSET_;
    }
};

struct py38 : py38_311 {
    static constexpr unsigned Version = 0x03080000;

    typedef struct {
        unaryfunc am_await;
        unaryfunc am_aiter;
        unaryfunc am_anext;
    } PyAsyncMethods;

    typedef struct _typeobject {
        // PyObject_VAR_HEAD
        PyVarObject ob_base;

        const char *tp_name; /* For printing, in format "<module>.<name>" */
        Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */

        /* Methods to implement standard operations */

        destructor tp_dealloc;
        Py_ssize_t tp_vectorcall_offset;
        getattrfunc tp_getattr;
        setattrfunc tp_setattr;
        PyAsyncMethods *tp_as_async; /* formerly known as tp_compare (Python 2)
                                        or tp_reserved (Python 3) */
        reprfunc tp_repr;

        /* Method suites for standard classes */

        PyNumberMethods *tp_as_number;
        PySequenceMethods *tp_as_sequence;
        PyMappingMethods *tp_as_mapping;

        /* More standard operations (here for binary compatibility) */

        hashfunc tp_hash;
        ternaryfunc tp_call;
        reprfunc tp_str;
        getattrofunc tp_getattro;
        setattrofunc tp_setattro;

        /* Functions to access object as input/output buffer */
        PyBufferProcs *tp_as_buffer;

        /* Flags to define presence of optional/expanded features */
        unsigned long tp_flags;

        const char *tp_doc; /* Documentation string */

        /* Assigned meaning in release 2.0 */
        /* call function for all accessible objects */
        traverseproc tp_traverse;

        /* delete references to contained objects */
        inquiry tp_clear;

        /* Assigned meaning in release 2.1 */
        /* rich comparisons */
        richcmpfunc tp_richcompare;

        /* weak reference enabler */
        Py_ssize_t tp_weaklistoffset;

        /* Iterators */
        getiterfunc tp_iter;
        iternextfunc tp_iternext;

        /* Attribute descriptor and subclassing stuff */
        struct PyMethodDef *tp_methods;
        struct PyMemberDef *tp_members;
        struct PyGetSetDef *tp_getset;
        struct _typeobject *tp_base;
        PyObject *tp_dict;
        descrgetfunc tp_descr_get;
        descrsetfunc tp_descr_set;
        Py_ssize_t tp_dictoffset;
        initproc tp_init;
        allocfunc tp_alloc;
        newfunc tp_new;
        freefunc tp_free; /* Low-level free-memory routine */
        inquiry tp_is_gc; /* For PyObject_IS_GC */
        PyObject *tp_bases;
        PyObject *tp_mro; /* method resolution order */
        PyObject *tp_cache;
        PyObject *tp_subclasses;
        PyObject *tp_weaklist;
        destructor tp_del;

        /* Type attribute cache version tag. Added in version 2.6 */
        unsigned int tp_version_tag;

        destructor tp_finalize;
        vectorcallfunc tp_vectorcall;

        /* bpo-37250: kept for backwards compatibility in CPython 3.8 only */
        /* Py_DEPRECATED(3.8) */ int (*tp_print)(PyObject *, FILE *, int);
    } PyTypeObject;

    typedef struct _heaptypeobject {
        /* Note: there's a dependency on the order of these members
           in slotptr() in typeobject.c . */
        PyTypeObject ht_type;
        PyAsyncMethods as_async;
        PyNumberMethods as_number;
        PyMappingMethods as_mapping;
        PySequenceMethods
            as_sequence; /* as_sequence comes after as_mapping,
                            so that the mapping wins when both
                            the mapping and the sequence define
                            a given operator (e.g. __getitem__).
                            see add_operators() in typeobject.c . */
        PyBufferProcs as_buffer;
        PyObject *ht_name, *ht_slots, *ht_qualname;
        struct _dictkeysobject *ht_cached_keys;
        /* here are optional user slots, followed by the members. */
    } PyHeapTypeObject;

    static inline vectorcallfunc _PyVectorcall_Function(PyObject *callable) {
        // PyTypeObject *tp = Py_TYPE(callable);
        PyTypeObject *tp =
            (PyTypeObject *)((struct _object *)callable)->ob_type;
        Py_ssize_t offset = tp->tp_vectorcall_offset;
        vectorcallfunc ptr;
        if (!PyType_HasFeature_(tp, Py_TPFLAGS_HAVE_VECTORCALL_)) {
            return nullptr;
        }
        memcpy(&ptr, (char *)callable + offset, sizeof(ptr));
        return ptr;
    }

    static inline PyObject *_PyObject_Vectorcall(PyObject *callable,
                                                 PyObject *const *args,
                                                 size_t nargsf,
                                                 PyObject *kwnames) {

        static auto _Py_CheckFunctionResult =
            reinterpret_cast<PyObject *(*)(PyObject *, PyObject *, const char *)>(
                dyn_symbol("_Py_CheckFunctionResult"));
        static auto _PyObject_MakeTpCall =
            reinterpret_cast<PyObject *(*)(PyObject *, PyObject *const *, Py_ssize_t,
                                      PyObject *)>(
                dyn_symbol("_PyObject_MakeTpCall"));

        PyObject *res;
        vectorcallfunc func;
        // assert(kwnames == NULL || PyTuple_Check(kwnames));
        // assert(args != NULL || PyVectorcall_NARGS(nargsf) == 0);
        func = _PyVectorcall_Function(callable);
        if (func == NULL) {
            Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
            return _PyObject_MakeTpCall(callable, args, nargs, kwnames);
        }
        res = func(callable, args, nargsf, kwnames);
        return _Py_CheckFunctionResult(callable, res, NULL);
    }

    static PyObject *PyObject_VectorcallMethod(PyObject *base,
                                               PyObject *const *args,
                                               size_t nargsf,
                                               PyObject *kwnames) {
        PyObject *res = nullptr;
        PyObject *self = PyObject_GetAttr(args[0], /* name = */ base);
        if (self) {
            res = _PyObject_Vectorcall(self, (PyObject **)args + 1, nargsf - 1,
                                       kwnames);
            Py_DECREF(self);
        }
        return res;
    }

    static inline int PyObject_CheckBuffer_(PyObject *obj) {
        auto tp = (PyTypeObject *)(((_object *)obj)->ob_type);
        return (tp->tp_as_buffer != NULL) &&
               (tp->tp_as_buffer->bf_getbuffer != NULL);
    }
};

struct py39 : py38_311 {
    static constexpr unsigned Version = 0x03090000;

    typedef struct {
        unaryfunc am_await;
        unaryfunc am_aiter;
        unaryfunc am_anext;
    } PyAsyncMethods;

    struct _typeobject {
        // PyObject_VAR_HEAD
        PyVarObject ob_base;

        const char *tp_name; /* For printing, in format "<module>.<name>" */
        Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */

        /* Methods to implement standard operations */

        destructor tp_dealloc;
        Py_ssize_t tp_vectorcall_offset;
        getattrfunc tp_getattr;
        setattrfunc tp_setattr;
        PyAsyncMethods *tp_as_async; /* formerly known as tp_compare (Python 2)
                                        or tp_reserved (Python 3) */
        reprfunc tp_repr;

        /* Method suites for standard classes */

        PyNumberMethods *tp_as_number;
        PySequenceMethods *tp_as_sequence;
        PyMappingMethods *tp_as_mapping;

        /* More standard operations (here for binary compatibility) */

        hashfunc tp_hash;
        ternaryfunc tp_call;
        reprfunc tp_str;
        getattrofunc tp_getattro;
        setattrofunc tp_setattro;

        /* Functions to access object as input/output buffer */
        PyBufferProcs *tp_as_buffer;

        /* Flags to define presence of optional/expanded features */
        unsigned long tp_flags;

        const char *tp_doc; /* Documentation string */

        /* Assigned meaning in release 2.0 */
        /* call function for all accessible objects */
        traverseproc tp_traverse;

        /* delete references to contained objects */
        inquiry tp_clear;

        /* Assigned meaning in release 2.1 */
        /* rich comparisons */
        richcmpfunc tp_richcompare;

        /* weak reference enabler */
        Py_ssize_t tp_weaklistoffset;

        /* Iterators */
        getiterfunc tp_iter;
        iternextfunc tp_iternext;

        /* Attribute descriptor and subclassing stuff */
        struct PyMethodDef *tp_methods;
        struct PyMemberDef *tp_members;
        struct PyGetSetDef *tp_getset;
        struct _typeobject *tp_base;
        PyObject *tp_dict;
        descrgetfunc tp_descr_get;
        descrsetfunc tp_descr_set;
        Py_ssize_t tp_dictoffset;
        initproc tp_init;
        allocfunc tp_alloc;
        newfunc tp_new;
        freefunc tp_free; /* Low-level free-memory routine */
        inquiry tp_is_gc; /* For PyObject_IS_GC */
        PyObject *tp_bases;
        PyObject *tp_mro; /* method resolution order */
        PyObject *tp_cache;
        PyObject *tp_subclasses;
        PyObject *tp_weaklist;
        destructor tp_del;

        /* Type attribute cache version tag. Added in version 2.6 */
        unsigned int tp_version_tag;

        destructor tp_finalize;
        vectorcallfunc tp_vectorcall;
    };

    typedef struct _typeobject PyTypeObject;

    typedef struct _heaptypeobject {
        /* Note: there's a dependency on the order of these members
           in slotptr() in typeobject.c . */
        PyTypeObject ht_type;
        PyAsyncMethods as_async;
        PyNumberMethods as_number;
        PyMappingMethods as_mapping;
        PySequenceMethods
            as_sequence; /* as_sequence comes after as_mapping,
                            so that the mapping wins when both
                            the mapping and the sequence define
                            a given operator (e.g. __getitem__).
                            see add_operators() in typeobject.c . */
        PyBufferProcs as_buffer;
        PyObject *ht_name, *ht_slots, *ht_qualname;
        struct _dictkeysobject *ht_cached_keys;
        PyObject *ht_module;
        /* here are optional user slots, followed by the members. */
    } PyHeapTypeObject;

    static inline vectorcallfunc PyVectorcall_Function(PyObject *callable) {
        PyTypeObject *tp;
        Py_ssize_t offset;
        vectorcallfunc ptr;

        // assert(callable != NULL);
        // tp = Py_TYPE(callable);
        tp = (PyTypeObject *)((struct _object *)callable)->ob_type;
        if (!PyType_HasFeature_(tp, Py_TPFLAGS_HAVE_VECTORCALL_)) {
            return NULL;
        }
        // assert(PyCallable_Check(callable));
        offset = tp->tp_vectorcall_offset;
        // assert(offset > 0);
        memcpy(&ptr, (char *)callable + offset, sizeof(ptr));
        return ptr;
    }

    static inline PyObject *_PyObject_VectorcallTstate(PyThreadState *tstate,
                                                       PyObject *callable,
                                                       PyObject *const *args,
                                                       size_t nargsf,
                                                       PyObject *kwnames) {
        static auto _Py_CheckFunctionResult =
            reinterpret_cast<PyObject *(*)(PyThreadState *, PyObject *,
                                           PyObject *, const char *)>(
                dyn_symbol("_Py_CheckFunctionResult"));
        static auto _PyObject_MakeTpCall =

            reinterpret_cast<PyObject *(*)(PyThreadState *, PyObject *,
                                           PyObject *const *, Py_ssize_t,
                                           PyObject *)>(
                dyn_symbol("_PyObject_MakeTpCall"));

        vectorcallfunc func;
        PyObject *res;

        // assert(kwnames == NULL || PyTuple_Check(kwnames));
        // assert(args != NULL || PyVectorcall_NARGS(nargsf) == 0);

        func = PyVectorcall_Function(callable);
        if (func == NULL) {
            Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
            return _PyObject_MakeTpCall(tstate, callable, args, nargs, kwnames);
        }
        res = func(callable, args, nargsf, kwnames);
        return _Py_CheckFunctionResult(tstate, callable, res, NULL);
    }

    static inline PyObject *PyObject_Vectorcall(PyObject *callable,
                                                PyObject *const *args,
                                                size_t nargsf,
                                                PyObject *kwnames) {
        PyThreadState *tstate = PyThreadState_Get();
        return _PyObject_VectorcallTstate(tstate, callable, args, nargsf,
                                          kwnames);
    }
};

// PyTypeObject is identical for 3.10 and 3.11
struct py310_311 : py38_311 {
    typedef enum {
        PYGEN_RETURN = 0,
        PYGEN_ERROR = -1,
        PYGEN_NEXT = 1,
    } PySendResult;

    typedef PySendResult (*sendfunc)(PyObject *iter, PyObject *value,
                                     PyObject **result);

    typedef struct {
        unaryfunc am_await;
        unaryfunc am_aiter;
        unaryfunc am_anext;
        sendfunc am_send;
    } PyAsyncMethods;

    // If this structure is modified, Doc/includes/typestruct.h should be
    // updated as well.
    struct _typeobject {
        // PyObject_VAR_HEAD
        PyVarObject ob_base;

        const char *tp_name; /* For printing, in format "<module>.<name>" */
        Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */

        /* Methods to implement standard operations */

        destructor tp_dealloc;
        Py_ssize_t tp_vectorcall_offset;
        getattrfunc tp_getattr;
        setattrfunc tp_setattr;
        PyAsyncMethods *tp_as_async; /* formerly known as tp_compare (Python 2)
                                        or tp_reserved (Python 3) */
        reprfunc tp_repr;

        /* Method suites for standard classes */

        PyNumberMethods *tp_as_number;
        PySequenceMethods *tp_as_sequence;
        PyMappingMethods *tp_as_mapping;

        /* More standard operations (here for binary compatibility) */

        hashfunc tp_hash;
        ternaryfunc tp_call;
        reprfunc tp_str;
        getattrofunc tp_getattro;
        setattrofunc tp_setattro;

        /* Functions to access object as input/output buffer */
        PyBufferProcs *tp_as_buffer;

        /* Flags to define presence of optional/expanded features */
        unsigned long tp_flags;

        const char *tp_doc; /* Documentation string */

        /* Assigned meaning in release 2.0 */
        /* call function for all accessible objects */
        traverseproc tp_traverse;

        /* delete references to contained objects */
        inquiry tp_clear;

        /* Assigned meaning in release 2.1 */
        /* rich comparisons */
        richcmpfunc tp_richcompare;

        /* weak reference enabler */
        Py_ssize_t tp_weaklistoffset;

        /* Iterators */
        getiterfunc tp_iter;
        iternextfunc tp_iternext;

        /* Attribute descriptor and subclassing stuff */
        struct PyMethodDef *tp_methods;
        struct PyMemberDef *tp_members;
        struct PyGetSetDef *tp_getset;
        // Strong reference on a heap type, borrowed reference on a static type
        struct _typeobject *tp_base;
        PyObject *tp_dict;
        descrgetfunc tp_descr_get;
        descrsetfunc tp_descr_set;
        Py_ssize_t tp_dictoffset;
        initproc tp_init;
        allocfunc tp_alloc;
        newfunc tp_new;
        freefunc tp_free; /* Low-level free-memory routine */
        inquiry tp_is_gc; /* For PyObject_IS_GC */
        PyObject *tp_bases;
        PyObject *tp_mro; /* method resolution order */
        PyObject *tp_cache;
        PyObject *tp_subclasses;
        PyObject *tp_weaklist;
        destructor tp_del;

        /* Type attribute cache version tag. Added in version 2.6 */
        unsigned int tp_version_tag;

        destructor tp_finalize;
        vectorcallfunc tp_vectorcall;
    };

    typedef struct _typeobject PyTypeObject;
};

struct py310 : py310_311 {
    static constexpr unsigned Version = 0x030A0000;

    typedef struct _heaptypeobject {
        /* Note: there's a dependency on the order of these members
           in slotptr() in typeobject.c . */
        struct _typeobject ht_type;
        PyAsyncMethods as_async;
        PyNumberMethods as_number;
        PyMappingMethods as_mapping;
        PySequenceMethods
            as_sequence; /* as_sequence comes after as_mapping,
                            so that the mapping wins when both
                            the mapping and the sequence define
                            a given operator (e.g. __getitem__).
                            see add_operators() in typeobject.c . */
        PyBufferProcs as_buffer;
        PyObject *ht_name, *ht_slots, *ht_qualname;
        struct _dictkeysobject *ht_cached_keys;
        PyObject *ht_module;
        /* here are optional user slots, followed by the members. */
    } PyHeapTypeObject;

    static inline vectorcallfunc PyVectorcall_Function(PyObject *callable) {
        PyTypeObject *tp;
        Py_ssize_t offset;
        vectorcallfunc ptr;

        // assert(callable != NULL);
        // tp = Py_TYPE(callable);
        tp = (PyTypeObject *)((struct _object *)callable)->ob_type;
        if (!PyType_HasFeature_(tp, Py_TPFLAGS_HAVE_VECTORCALL_)) {
            return NULL;
        }
        // assert(PyCallable_Check(callable));
        offset = tp->tp_vectorcall_offset;
        // assert(offset > 0);
        memcpy(&ptr, (char *)callable + offset, sizeof(ptr));
        return ptr;
    }

    static inline PyObject *_PyObject_VectorcallTstate(PyThreadState *tstate,
                                                       PyObject *callable,
                                                       PyObject *const *args,
                                                       size_t nargsf,
                                                       PyObject *kwnames) {
        static auto _Py_CheckFunctionResult =
            reinterpret_cast<PyObject *(*)(PyThreadState *, PyObject *,
                                           PyObject *, const char *)>(
                dyn_symbol("_Py_CheckFunctionResult"));
        static auto _PyObject_MakeTpCall =

            reinterpret_cast<PyObject *(*)(PyThreadState *, PyObject *,
                                           PyObject *const *, Py_ssize_t,
                                           PyObject *)>(
                dyn_symbol("_PyObject_MakeTpCall"));

        vectorcallfunc func;
        PyObject *res;

        // assert(kwnames == NULL || PyTuple_Check(kwnames));
        // assert(args != NULL || PyVectorcall_NARGS(nargsf) == 0);

        func = PyVectorcall_Function(callable);
        if (func == NULL) {
            Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
            return _PyObject_MakeTpCall(tstate, callable, args, nargs, kwnames);
        }
        res = func(callable, args, nargsf, kwnames);
        return _Py_CheckFunctionResult(tstate, callable, res, NULL);
    }

    static inline PyObject *PyObject_Vectorcall(PyObject *callable,
                                                PyObject *const *args,
                                                size_t nargsf,
                                                PyObject *kwnames) {
        PyThreadState *tstate = PyThreadState_Get();
        return _PyObject_VectorcallTstate(tstate, callable, args, nargsf,
                                          kwnames);
    }
};

struct py311 : py310_311 {
    static constexpr unsigned Version = 0x030B0000;

    struct _specialization_cache {
        PyObject *getitem;
    };
#
    typedef struct _heaptypeobject {
        /* Note: there's a dependency on the order of these members
           in slotptr() in typeobject.c . */
        struct _typeobject ht_type;
        PyAsyncMethods as_async;
        PyNumberMethods as_number;
        PyMappingMethods as_mapping;
        PySequenceMethods
            as_sequence; /* as_sequence comes after as_mapping,
                            so that the mapping wins when both
                            the mapping and the sequence define
                            a given operator (e.g. __getitem__).
                            see add_operators() in typeobject.c . */
        PyBufferProcs as_buffer;
        PyObject *ht_name, *ht_slots, *ht_qualname;
        struct _dictkeysobject *ht_cached_keys;
        PyObject *ht_module;
        char *_ht_tpname; // Storage for "tp_name"; see PyType_FromModuleAndSpec
        struct _specialization_cache _spec_cache; // For use by the specializer.
        /* here are optional user slots, followed by the members. */
    } PyHeapTypeObject;
};

#endif

#if !defined(Py_LIMITED_API)
struct py_current {
    static constexpr uint32_t Version = PY_VERSION_HEX;
    using PyTypeObject = ::PyTypeObject;
    using PyHeapTypeObject = ::PyHeapTypeObject;
};
#endif

#if !defined(PYPY_VERSION)
inline PyInterpreterState *compat_PyInterpreterState_Get() {
#if defined(Py_LIMITED_API) && (NB_PY_VERSION_MIN < 0x03090000 || (defined(_WIN32) && NB_PY_VERSION_MIN < 0x030A0000))
    return NB_DYN_SYM_CACHE(NB_PY_VERSION_CHECK(0x03090000)
                                ? "PyInterpreterState_Get"
                                : "_PyInterpreterState_Get",
                            decltype(&compat_PyInterpreterState_Get))();
#else
    return NB_PY_39(PyInterpreterState_Get, _PyInterpreterState_Get)();
#endif
}

// In theory this is in the stable ABI, but it's missing on windows in 3.8, 3.9
inline PyObject *compat_PyInterpreterState_GetDict(PyInterpreterState *interp) {
#if defined(_WIN32)
    return NB_COMPAT_SYM_VER(310, PyInterpreterState_GetDict)(interp);
#else
    return PyInterpreterState_GetDict(interp);
#endif
}
#endif

inline int compat_PyObject_GetBuffer(PyObject *exporter, Py_buffer *view,
                                     int flags) {
    return NB_COMPAT_SYM_VER(311, PyObject_GetBuffer)(exporter, view, flags);
}

inline void compat_PyBuffer_Release(Py_buffer *view) {
    return NB_COMPAT_SYM_VER(311, PyBuffer_Release)(view);
}

// Defined as a macro in 3.8, need to implement separately
inline int compat_PyObject_CheckBuffer(PyObject *obj) {
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030B0000
    static decltype(&compat_PyObject_CheckBuffer) f =
        NB_PY_VERSION_CHECK(0x03090000)
            ? NB_COMPAT_SYM(PyObject_CheckBuffer)
            : py38::PyObject_CheckBuffer_;
    return f(obj);
#else
    return PyObject_CheckBuffer(obj);
#endif
}

inline PyObject *compat_PyObject_Vectorcall(PyObject *callable,
                                            PyObject *const *args,
                                            size_t nargsf, PyObject *kwnames) {
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030C0000
    static decltype(&compat_PyObject_Vectorcall) f = []() {
        if (NB_PY_VERSION_CHECK(0x030B0000)) {
            return NB_COMPAT_SYM(PyObject_Vectorcall);
        } else if (NB_PY_VERSION_CHECK(0x030A0000)) {
            return &py310::PyObject_Vectorcall;
        } else if (NB_PY_VERSION_CHECK(0x03090000)) {
            return &py39::PyObject_Vectorcall;
        } else if (NB_PY_VERSION_CHECK(0x03080000)) {
            return &py38::_PyObject_Vectorcall;
        }
    }();
    return f(callable, args, nargsf, kwnames);
#else
    return NB_PY_39(PyObject_Vectorcall, _PyObject_Vectorcall)(callable, args, nargsf, kwnames);
#endif
}

inline PyObject *compat_PyObject_VectorcallMethod(PyObject *name,
                                                  PyObject *const *args,
                                                  size_t nargsf,
                                                  PyObject *kwnames) {
#if defined(Py_LIMITED_API) && NB_PY_VERSION_MIN < 0x030C0000
    static decltype(&compat_PyObject_VectorcallMethod) f =
        NB_PY_VERSION_CHECK(0x03090000)
            ? NB_COMPAT_SYM(PyObject_VectorcallMethod)
            : py38::PyObject_VectorcallMethod;
    return f(name, args, nargsf, kwnames);
#else
    return NB_PY_39(PyObject_VectorcallMethod, py38::PyObject_VectorcallMethod)(
        name, args, nargsf, kwnames);
#endif
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)