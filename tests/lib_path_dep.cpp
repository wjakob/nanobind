#include <nanobind/nb_defs.h>

#if defined(_WIN32)
#  if defined(LIB_PATH_DEP_BUILD)
#    define LIB_PATH_DEP_API NB_EXPORT
#  else
#    define LIB_PATH_DEP_API NB_IMPORT
#  endif
#else
#  define LIB_PATH_DEP_API NB_EXPORT
#endif

extern "C" LIB_PATH_DEP_API int lib_path_value() {
    return 123;
}
