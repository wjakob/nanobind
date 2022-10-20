#pragma once

#include "detail/nb_set.h"
#include <set>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Key, typename Compare, typename Alloc>
struct type_caster<std::set<Key, Compare, Alloc>>
    : set_caster<std::set<Key, Compare, Alloc>, Key> {
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
