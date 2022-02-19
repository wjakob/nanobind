#include <nanobind/nanobind.h>
#include <nanobind/trampoline.h>

#define PYBIND11_OVERRIDE(...)           NB_OVERRIDE_NAME(__VA_ARGS__)
#define PYBIND11_OVERRIDE_NAME(...)      NB_OVERRIDE_NAME(__VA_ARGS__)
#define PYBIND11_OVERRIDE_PURE(...)      NB_OVERRIDE_PURE(__VA_ARGS__)
#define PYBIND11_OVERRIDE_PURE_NAME(...) NB_OVERRIDE_PURE_NAME(__VA_ARGS__)

NAMESPACE_BEGIN(pybind11)

using namespace nanbind;

using error_already_set = python_error;
using return_value_policy = rv_policy;
using module = module_;

template <typename T> T reinterpret_borrow(handle h) {
    return { h, nanobind::detail::borrow_t() };
}

template <typename T> T reinterpret_steal(handle h) {
    return { h, nanobind::detail::steal_t() };
}

NAMESPACE_END(pybind11)
