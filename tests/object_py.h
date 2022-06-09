#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

template <typename T> struct type_caster<ref<T>> {
    using Value = ref<T>;
    using Caster = make_caster<T>;
    static constexpr auto Name = Caster::Name;
    static constexpr bool IsClass = true;

    template <typename T_> using Cast = movable_cast_t<T_>;

    Value value;

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        Caster caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;

        value = Value(caster.operator T *());

        return true;
    }

    static handle from_cpp(const ref<T> &value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        return Caster::from_cpp(value.get(), policy, cleanup);
    }

    explicit operator Value *() { return &value; }
    explicit operator Value &() { return value; }
    explicit operator Value &&() && { return (Value &&) value; }
};

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
