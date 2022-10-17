#pragma once

#include "detail/nb_array.h"
#include <array>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Type, size_t Size> struct type_caster<std::array<Type, Size>>
 : array_caster<std::array<Type, Size>, Type, Size> { };

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
