#pragma once

#if __cplusplus < 201703L
#  error The nanobind library requires C++17!
#endif

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <new>
#include <utility>

#include "nb_python.h"
#include "nb_defs.h"
#include "nb_enums.h"
#include "nb_traits.h"
#include "nb_tuple.h"
#include "nb_lib.h"
#include "nb_descr.h"
#include "nb_types.h"
#include "nb_accessor.h"
#include "nb_error.h"
#include "nb_attr.h"
#include "nb_cast.h"
#include "nb_call.h"
#include "nb_func.h"
#include "nb_class.h"
#include "nb_misc.h"
#include "nb_compat.h"
