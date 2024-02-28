/*
    nanobind/typing.h: Optional typing-related functionality

    Copyright (c) 2024 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)

template <typename... Args>
object type_var(Args&&... args) {
    return module_::import_("typing").attr("TypeVar")((detail::forward_t<Args>) args...);
}

template <typename... Args>
object type_var_tuple(Args&&... args) {
    return module_::import_("typing").attr("TypeVarTuple")((detail::forward_t<Args>) args...);
}

NAMESPACE_END(NB_NAMESPACE)
