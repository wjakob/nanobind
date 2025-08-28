/*
    nanobind/nb_defs.h: Preprocessor definitions used by the project

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#define NB_STRINGIFY(x) #x
#define NB_TOSTRING(x) NB_STRINGIFY(x)
#define NB_CONCAT(first, second) first##second
#define NB_NEXT_OVERLOAD ((PyObject *) 1) // special failure return code

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif

#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#if defined(_WIN32)
#  define NB_EXPORT          __declspec(dllexport)
#  define NB_IMPORT          __declspec(dllimport)
#  define NB_INLINE          __forceinline
#  define NB_NOINLINE        __declspec(noinline)
#  define NB_INLINE_LAMBDA
#else
#  define NB_EXPORT          __attribute__ ((visibility("default")))
#  define NB_IMPORT          NB_EXPORT
#  define NB_INLINE          inline __attribute__((always_inline))
#  define NB_NOINLINE        __attribute__((noinline))
#  if defined(__clang__)
#    define NB_INLINE_LAMBDA __attribute__((always_inline))
#  else
#    define NB_INLINE_LAMBDA
#  endif
#endif

#if defined(__GNUC__) && !defined(_WIN32)
#  define NB_NAMESPACE nanobind __attribute__((visibility("hidden")))
#else
#  define NB_NAMESPACE nanobind
#endif

#if defined(__GNUC__)
#  define NB_UNLIKELY(x) __builtin_expect(bool(x), 0)
#  define NB_LIKELY(x)   __builtin_expect(bool(x), 1)
#else
#  define NB_LIKELY(x) x
#  define NB_UNLIKELY(x) x
#endif

#if defined(NB_SHARED)
#  if defined(NB_BUILD)
#    define NB_CORE NB_EXPORT
#  else
#    define NB_CORE NB_IMPORT
#  endif
#else
#  define NB_CORE
#endif

#if !defined(NB_SHARED) && defined(__GNUC__) && !defined(_WIN32)
#  define NB_EXPORT_SHARED __attribute__ ((visibility("hidden")))
#else
#  define NB_EXPORT_SHARED
#endif

#if defined(__cpp_lib_char8_t) && __cpp_lib_char8_t >= 201811L
#  define NB_HAS_U8STRING
#endif

#if defined(Py_TPFLAGS_HAVE_VECTORCALL)
#  define NB_VECTORCALL PyObject_Vectorcall
#  define NB_HAVE_VECTORCALL Py_TPFLAGS_HAVE_VECTORCALL
#elif defined(_Py_TPFLAGS_HAVE_VECTORCALL)
#  define NB_VECTORCALL _PyObject_Vectorcall
#  define NB_HAVE_VECTORCALL _Py_TPFLAGS_HAVE_VECTORCALL
#else
#  define NB_HAVE_VECTORCALL (1UL << 11)
#endif

#if defined(PY_VECTORCALL_ARGUMENTS_OFFSET)
#  define NB_VECTORCALL_ARGUMENTS_OFFSET PY_VECTORCALL_ARGUMENTS_OFFSET
#  define NB_VECTORCALL_NARGS PyVectorcall_NARGS
#else
#  define NB_VECTORCALL_ARGUMENTS_OFFSET ((size_t) 1 << (8 * sizeof(size_t) - 1))
#  define NB_VECTORCALL_NARGS(n) ((n) & ~NB_VECTORCALL_ARGUMENTS_OFFSET)
#endif

#if PY_VERSION_HEX < 0x03090000
#  define NB_TYPING_ABC   "typing."
#  define NB_TYPING_TUPLE "typing.Tuple"
#  define NB_TYPING_LIST  "typing.List"
#  define NB_TYPING_DICT  "typing.Dict"
#  define NB_TYPING_SET   "typing.Set"
#  define NB_TYPING_TYPE  "typing.Type"
#else
#  define NB_TYPING_ABC   "collections.abc."
#  define NB_TYPING_TUPLE "tuple"
#  define NB_TYPING_LIST  "list"
#  define NB_TYPING_DICT  "dict"
#  define NB_TYPING_SET   "set"
#  define NB_TYPING_TYPE  "type"
#endif

#if PY_VERSION_HEX < 0x030D0000
#  define NB_TYPING_CAPSULE "typing_extensions.CapsuleType"
#else
#  define NB_TYPING_CAPSULE "types.CapsuleType"
#endif

#define NB_TYPING_SEQUENCE     NB_TYPING_ABC "Sequence"
#define NB_TYPING_MAPPING      NB_TYPING_ABC "Mapping"
#define NB_TYPING_CALLABLE     NB_TYPING_ABC "Callable"
#define NB_TYPING_ITERATOR     NB_TYPING_ABC "Iterator"
#define NB_TYPING_ITERABLE     NB_TYPING_ABC "Iterable"

#if PY_VERSION_HEX < 0x03090000
#  define NB_TYPING_ABSTRACT_SET "typing.AbstractSet"
#else
#  define NB_TYPING_ABSTRACT_SET "collections.abc.Set"
#endif

#if defined(Py_LIMITED_API)
#  if PY_VERSION_HEX < 0x030C0000 || defined(PYPY_VERSION)
#    error "nanobind can target Python's limited API, but this requires CPython >= 3.12"
#  endif
#  define NB_TUPLE_GET_SIZE PyTuple_Size
#  define NB_TUPLE_GET_ITEM PyTuple_GetItem
#  define NB_TUPLE_SET_ITEM PyTuple_SetItem
#  define NB_LIST_GET_SIZE PyList_Size
#  define NB_LIST_GET_ITEM PyList_GetItem
#  define NB_LIST_SET_ITEM PyList_SetItem
#  define NB_DICT_GET_SIZE PyDict_Size
#  define NB_SET_GET_SIZE PySet_Size
#else
#  define NB_TUPLE_GET_SIZE PyTuple_GET_SIZE
#  define NB_TUPLE_GET_ITEM PyTuple_GET_ITEM
#  define NB_TUPLE_SET_ITEM PyTuple_SET_ITEM
#  define NB_LIST_GET_SIZE PyList_GET_SIZE
#  define NB_LIST_GET_ITEM PyList_GET_ITEM
#  define NB_LIST_SET_ITEM PyList_SET_ITEM
#  define NB_DICT_GET_SIZE PyDict_GET_SIZE
#  define NB_SET_GET_SIZE PySet_GET_SIZE
#endif

#if defined(PYPY_VERSION_NUM) && PYPY_VERSION_NUM < 0x07030a00
#    error "nanobind requires a newer PyPy version (>= 7.3.10)"
#endif

#if defined(NB_FREE_THREADED) && !defined(Py_GIL_DISABLED)
#    error "Free-threaded extensions require a free-threaded version of Python"
#endif

#if defined(NB_DOMAIN)
#  define NB_DOMAIN_STR NB_TOSTRING(NB_DOMAIN)
#else
#  define NB_DOMAIN_STR nullptr
#endif

#if !defined(PYPY_VERSION)
#  if PY_VERSION_HEX < 0x030A0000
#    define NB_TYPE_GET_SLOT_IMPL 1 // Custom implementation of nb::type_get_slot
#  else
#    define NB_TYPE_GET_SLOT_IMPL 0
#  endif
#  if PY_VERSION_HEX < 0x030C0000
#    define NB_TYPE_FROM_METACLASS_IMPL 1 // Custom implementation of PyType_FromMetaclass
#  else
#    define NB_TYPE_FROM_METACLASS_IMPL 0
#  endif
#else
#  define NB_TYPE_FROM_METACLASS_IMPL 1
#  define NB_TYPE_GET_SLOT_IMPL 1
#endif

#define NB_MODULE_SLOTS_0 { 0, nullptr }

#if PY_VERSION_HEX < 0x030C0000
#  define NB_MODULE_SLOTS_1 NB_MODULE_SLOTS_0
#else
#  define NB_MODULE_SLOTS_1                                                    \
    { Py_mod_multiple_interpreters,                                            \
      Py_MOD_MULTIPLE_INTERPRETERS_NOT_SUPPORTED },                            \
    NB_MODULE_SLOTS_0
#endif

#if !defined(NB_FREE_THREADED)
#  define NB_MODULE_SLOTS_2 NB_MODULE_SLOTS_1
#else
#  define NB_MODULE_SLOTS_2                                                    \
   { Py_mod_gil, Py_MOD_GIL_NOT_USED },                                        \
   NB_MODULE_SLOTS_1
#endif

#define NB_NONCOPYABLE(X)                                                      \
    X(const X &) = delete;                                                     \
    X &operator=(const X &) = delete;

// Helper macros to ensure macro arguments are expanded before token pasting/stringification
#define NB_MODULE_IMPL(name, variable) NB_MODULE_IMPL2(name, variable)
#define NB_MODULE_IMPL2(name, variable)                                        \
    static void nanobind_##name##_exec_impl(nanobind::module_);                \
    static int nanobind_##name##_exec(PyObject *m) {                           \
        nanobind::detail::init(NB_DOMAIN_STR);                                 \
        try {                                                                  \
            nanobind_##name##_exec_impl(                                       \
                nanobind::borrow<nanobind::module_>(m));                       \
            return 0;                                                          \
        } catch (nanobind::python_error &e) {                                  \
            e.restore();                                                       \
            nanobind::chain_error(                                             \
                PyExc_ImportError,                                             \
                "Encountered an error while initializing the extension.");     \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
        }                                                                      \
        return -1;                                                             \
    }                                                                          \
    static PyModuleDef_Slot nanobind_##name##_slots[] = {                      \
        { Py_mod_exec, (void *) nanobind_##name##_exec },                      \
        NB_MODULE_SLOTS_2                                                      \
    };                                                                         \
    static struct PyModuleDef nanobind_##name##_module = {                     \
        PyModuleDef_HEAD_INIT, #name, nullptr, 0, nullptr,                     \
        nanobind_##name##_slots, nullptr, nullptr, nullptr                     \
    };                                                                         \
    extern "C" [[maybe_unused]] NB_EXPORT PyObject *PyInit_##name(void);       \
    extern "C" PyObject *PyInit_##name(void) {                                 \
        return PyModuleDef_Init(&nanobind_##name##_module);                    \
    }                                                                          \
    void nanobind_##name##_exec_impl(nanobind::module_ variable)

#define NB_MODULE(name, variable) NB_MODULE_IMPL(name, variable)
