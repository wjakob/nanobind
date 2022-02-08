#define NB_STRINGIFY(x) #x
#define NB_TOSTRING(x) NB_STRINGIFY(x)
#define NB_CONCAT(first, second) first##second

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif

#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#if !defined(NB_EXPORT)
#  if defined(_WIN32)
#    define NB_EXPORT __declspec(dllexport)
#  else
#    define NB_EXPORT __attribute__ ((visibility("default")))
#  endif
#endif

#define NB_MODULE(name, variable)                                              \
    extern "C" [[maybe_unused]] NB_EXPORT PyObject *PyInit_##name();           \
    static PyModuleDef NB_CONCAT(nanobind_module_def_, name);                  \
    [[maybe_unused]] static void NB_CONCAT(nanobind_init_,                     \
                                           name)(::nanobind::module_ &);       \
    extern "C" NB_EXPORT PyObject *PyInit_##name() {                           \
        nanobind::module_ m = nanobind::reinterpret_borrow<nanobind::module_>( \
            nanobind::detail::module_new(                                   \
                NB_TOSTRING(name), &NB_CONCAT(nanobind_module_def_, name)));   \
        try {                                                                  \
            NB_CONCAT(nanobind_init_, name)(m);                                \
            return m.ptr();                                                    \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        }                                                                      \
    }                                                                          \
    void NB_CONCAT(nanobind_init_, name)(::nanobind::module_ & (variable))
