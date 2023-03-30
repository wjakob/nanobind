/*
    nanobind/stl/optional.h: type caster for std::optional<...>

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <optional>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T> struct remove_opt_mono<std::optional<T>> : remove_opt_mono<T> {};

template <typename T>
struct type_caster<std::optional<T>> {
    using Value = std::optional<T>;
    using Ti = detail::intrinsic_t<T>;
    using Caster = make_caster<Ti>;

    static constexpr auto Name = const_name("Optional[") + concat(Caster::Name) + const_name("]");
    static constexpr bool IsClass = false;

    template <typename T_>
    using Cast = movable_cast_t<T_>;

    Value value = std::nullopt;

    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept {
        if (src.is_none())
            return true;

        Caster caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;

        if constexpr (is_pointer_v<T>) {
            static_assert(Caster::IsClass,
                            "Binding 'optional<T*>' requires that 'T' can also be bound by nanobind.");
            value = caster.operator cast_t<T>();
        } else if constexpr (Caster::IsClass) {
            value = caster.operator cast_t<T&>();
        } else {
            value = std::move(caster).operator cast_t<T&&>();
        }

        return true;
    }

    template <typename T_>
    static handle from_cpp(T_ *value, rv_policy policy, cleanup_list *cleanup) noexcept {
        if (!value)
            return none().release();
        return from_cpp(*value, policy, cleanup);
    }

    template <typename T_>
    static handle from_cpp(T_ &&value, rv_policy policy, cleanup_list *cleanup) noexcept {
        if (!value)
            return none().release();
        return Caster::from_cpp(forward_like<T_>(*value), policy, cleanup);
    }

    explicit operator Value *() { return &value; }
    explicit operator Value &() { return value; }
    explicit operator Value &&() && { return (Value &&) value; }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
