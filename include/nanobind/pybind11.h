#include <nanobind/nanobind.h>
#include <nanobind/trampoline.h>
#include <nanobind/enum.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/shared_ptr.h>

#define PYBIND11_OVERRIDE(...)           NB_OVERRIDE_NAME(__VA_ARGS__)
#define PYBIND11_OVERRIDE_NAME(...)      NB_OVERRIDE_NAME(__VA_ARGS__)
#define PYBIND11_OVERRIDE_PURE(...)      NB_OVERRIDE_PURE(__VA_ARGS__)
#define PYBIND11_OVERRIDE_PURE_NAME(...) NB_OVERRIDE_PURE_NAME(__VA_ARGS__)

NAMESPACE_BEGIN(pybind11)

using namespace nanobind;

using error_already_set = python_error;
using return_value_policy = rv_policy;
using module = module_;

template <typename T> T reinterpret_borrow(handle h) {
    return { h, nanobind::detail::borrow_t() };
}

template <typename T> T reinterpret_steal(handle h) {
    return { h, nanobind::detail::steal_t() };
}

template <typename T1, typename T2> void implicitly_convertible() {
    using Caster = make_caster<T1>;

    if constexpr (Caster::IsClass) {
        nanobind::detail::implicitly_convertible(&typeid(T1), &typeid(T2));
    } else {
        nanobind::detail::implicitly_convertible(
            [](PyObject *src, cleanup_list *cleanup) noexcept -> bool {
                return Caster().from_python(src, cast_flags::convert, cleanup);
            },
            &typeid(T2));
    }
}

NAMESPACE_END(pybind11)
