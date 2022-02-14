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
#  define NB_EXPORT        __declspec(dllexport)
#  define NB_IMPORT        __declspec(import)
#  define NB_INLINE        __forceinline
#  define NB_INLINE_LAMBDA
#else
#  define NB_EXPORT        __attribute__ ((visibility("default")))
#  define NB_IMPORT
#  define NB_INLINE        inline __attribute__((always_inline))
#  define NB_INLINE_LAMBDA __attribute__((always_inline))
#endif

#if defined(__GNUC__)
#  define NB_NAMESPACE nanobind __attribute__((visibility("hidden")))
#else
#  define NB_NAMESPACE nanobind
#endif

#if defined(NB_SHARED)
#  if defined(NB_BUILD)
#    define NB_CORE NB_EXPORT
#  else
#    define NB_CORE NB_IMPORT
#  endif
#else
#  if defined(_WIN32)
#    define NB_CORE
#  else
#    define NB_CORE NB_EXPORT
#  endif
#endif

#if defined(__cpp_lib_char8_t) && __cpp_lib_char8_t >= 201811L
#  define NB_HAS_U8STRING
#endif

#define NB_MODULE(name, variable)                                              \
    extern "C" [[maybe_unused]] NB_EXPORT PyObject *PyInit_##name();           \
    static PyModuleDef NB_CONCAT(nanobind_module_def_, name);                  \
    [[maybe_unused]] static void NB_CONCAT(nanobind_init_,                     \
                                           name)(::nanobind::module_ &);       \
    extern "C" NB_EXPORT PyObject *PyInit_##name() {                           \
        nanobind::module_ m =                                                  \
            nanobind::borrow<nanobind::module_>(nanobind::detail::module_new(  \
                NB_TOSTRING(name), &NB_CONCAT(nanobind_module_def_, name)));   \
        try {                                                                  \
            NB_CONCAT(nanobind_init_, name)(m);                                \
            nanobind::detail::nbfunc_finalize();                               \
            return m.ptr();                                                    \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        }                                                                      \
    }                                                                          \
    void NB_CONCAT(nanobind_init_, name)(::nanobind::module_ & (variable))

