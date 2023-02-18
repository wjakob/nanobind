/*
    nanobind/trampoline.h: functionality for overriding C++ virtual
    functions from within Python

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

NB_CORE void trampoline_new(void **data, size_t size, void *ptr,
                            const std::type_info *cpp_type) noexcept;
NB_CORE void trampoline_release(void **data, size_t size) noexcept;

NB_CORE PyObject *trampoline_lookup(void **data, size_t size, const char *name,
                                    bool pure);

template <size_t Size> struct trampoline {
    mutable void *data[2 * Size + 1];

    NB_INLINE trampoline(void *ptr, const std::type_info *cpp_type) {
        trampoline_new(data, Size, ptr, cpp_type);
    }

    NB_INLINE ~trampoline() { trampoline_release(data, Size); }

    NB_INLINE handle lookup(const char *name, bool pure) const {
        return trampoline_lookup(data, Size, name, pure);
    }

    NB_INLINE handle base() const { return (PyObject *) data[0]; }
};

#define NB_TRAMPOLINE(base, size)                                              \
    using NBBase = base;                                                       \
    using NBBase::NBBase;                                                      \
    nanobind::detail::trampoline<size> nb_trampoline{ this, &typeid(NBBase) }

#define NB_OVERRIDE_NAME(name, func, ...)                                      \
    nanobind::handle nb_key = nb_trampoline.lookup(name, false);               \
    using nb_ret_type = decltype(NBBase::func(__VA_ARGS__));                   \
    if (nb_key.is_valid()) {                                                   \
        nanobind::gil_scoped_acquire nb_guard;                                 \
        return nanobind::cast<nb_ret_type>(                                    \
            nb_trampoline.base().attr(nb_key)(__VA_ARGS__));                   \
    } else                                                                     \
        return NBBase::func(__VA_ARGS__)                                       \

#define NB_OVERRIDE_PURE_NAME(name, func, ...)                                 \
    nanobind::handle nb_key = nb_trampoline.lookup(name, true);                \
    nanobind::gil_scoped_acquire nb_guard;                                     \
    using nb_ret_type = decltype(NBBase::func(__VA_ARGS__));                   \
    return nanobind::cast<nb_ret_type>(                                        \
        nb_trampoline.base().attr(nb_key)(__VA_ARGS__))

#define NB_OVERRIDE(func, ...)                                                 \
    NB_OVERRIDE_NAME(#func, func, __VA_ARGS__)

#define NB_OVERRIDE_PURE(func, ...)                                            \
    NB_OVERRIDE_PURE_NAME(#func, func, __VA_ARGS__)

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
