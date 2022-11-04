/*
    nanobind/stl/variant.h: type caster for std::variant<...>

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <variant>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T, typename...>
struct concat_variant { using type = T; };
template <typename... Ts1, typename... Ts2, typename... Ts3>
struct concat_variant<std::variant<Ts1...>, std::variant<Ts2...>, Ts3...>
    : concat_variant<std::variant<Ts1..., Ts2...>, Ts3...> {};

template <typename... Ts> struct remove_opt_mono<std::variant<Ts...>>
    : concat_variant<std::conditional_t<std::is_same_v<std::monostate, Ts>, std::variant<>, std::variant<Ts>>...> {};

template <>
struct type_caster<std::monostate> {
    NB_TYPE_CASTER(std::monostate, const_name("None"));

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        if (src.is_none())
            return true;
        return false;
    }

    static handle from_cpp(const std::monostate &, rv_policy,
                           cleanup_list *) noexcept {
        return none().release();
    }
};

template <typename... Ts>
class type_caster<std::variant<Ts...>> {
    template <typename T> using Caster = make_caster<detail::intrinsic_t<T>>;

    template <typename T>
    bool variadic_caster(const handle &src, uint8_t flags, cleanup_list *cleanup) {
        Caster<T> caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;

        if constexpr (is_pointer_v<T>) {
            static_assert(Caster<T>::IsClass,
                          "Binding 'variant<T*,...>' requires that 'T' can also be bound by nanobind.");
            value = caster.operator cast_t<T>();
        } else if constexpr (Caster<T>::IsClass) {
            // Non-pointer classes do not accept a null pointer
            if (src.is_none())
                return false;
            value = caster.operator cast_t<T &>();
        } else {
            value = std::move(caster).operator cast_t<T &&>();
        }
        return true;
    }

public:
    using Value = std::variant<Ts...>;

    static constexpr auto Name = const_name("Union[") + concat(Caster<Ts>::Name...) + const_name("]");
    static constexpr bool IsClass = false;

    template <typename T> using Cast = movable_cast_t<T>;

    Value value;

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        return (variadic_caster<Ts>(src, flags, cleanup) || ...);
    }

    template <typename T>
    static handle from_cpp(T *value, rv_policy policy, cleanup_list *cleanup) noexcept {
        if (!value)
            return none().release();
        return from_cpp(*value, policy, cleanup);
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy, cleanup_list *cleanup) noexcept {
        return std::visit([&](auto &&v) {
                return Caster<decltype(v)>::from_cpp(std::forward<decltype(v)>(v), policy, cleanup);
            }, std::forward<T>(value));
    }

    explicit operator Value *() { return &value; }
    explicit operator Value &() { return value; }
    explicit operator Value &&() && { return (Value &&) value; }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
