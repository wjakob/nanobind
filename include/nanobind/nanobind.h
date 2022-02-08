#if __cplusplus < 201703L
#  error NanoBind requires C++17
#endif

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <new>
#include <utility>

#include "nb_python.h"
#include "nb_defs.h"
#include "nb_traits.h"
#include "nb_tuple.h"
#include "nb_lib.h"
#include "nb_types.h"
#include "nb_cast.h"
#include "nb_attr.h"
#include "nb_func.h"
