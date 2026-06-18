/*
    nanobind/nb_enums.h: enumerations used in nanobind (just rv_policy atm.)

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

// Approach used to cast a previously unknown C++ instance into a Python object.
// The named constants are tag objects, so binding code can recover the return
// policy at compile time while runtime APIs can still accept 'rv_policy'.
class rv_policy {
public:
    enum value : uint8_t {
        automatic_v,
        automatic_reference_v,
        take_ownership_v,
        copy_v,
        move_v,
        reference_v,
        reference_internal_v,
        none_v
        /* Note to self: nb_func.h assumes that this value fits into 3 bits,
           hence no further policies can be added. */
    };

    template <value V> struct policy_tag {
        static constexpr value policy = V;
        constexpr operator rv_policy() const noexcept { return rv_policy(V); }
        constexpr operator value() const noexcept { return V; }
    };

    constexpr rv_policy(value v) noexcept : m_value(v) { }

    template <value V>
    constexpr rv_policy(policy_tag<V>) noexcept : m_value(V) { }

    constexpr operator value() const noexcept { return m_value; }

    static constexpr policy_tag<automatic_v> automatic { };
    static constexpr policy_tag<automatic_reference_v> automatic_reference { };
    static constexpr policy_tag<take_ownership_v> take_ownership { };
    static constexpr policy_tag<copy_v> copy { };
    static constexpr policy_tag<move_v> move { };
    static constexpr policy_tag<reference_v> reference { };
    static constexpr policy_tag<reference_internal_v> reference_internal { };
    static constexpr policy_tag<none_v> none { };

private:
    value m_value;
};

NAMESPACE_END(NB_NAMESPACE)
