#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

template <typename T> struct type_caster<ref<T>> {
    using Caster = make_caster<T>;
    static constexpr bool IsClass = true;
    NB_TYPE_CASTER(ref<T>, Caster::Name)

    bool from_python(handle src, uint8_t flags, nb_internals *internals,
                     cleanup_list *cleanup) noexcept {
        Caster caster;
        if (!caster.from_python(src, flags, internals, cleanup))
            return false;

        value = Value(caster.operator T *());
        return true;
    }

    static handle from_cpp(const ref<T> &value, nb_internals *internals,
                           rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        return Caster::from_cpp(value.get(), internals, policy, cleanup);
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
