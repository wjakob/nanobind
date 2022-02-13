NAMESPACE_BEGIN(NB_NAMESPACE)

using error_already_set = python_error;
using return_value_policy = rv_policy;
using module = module_;

template <typename T> T reinterpret_borrow(handle h) {
    return { h, detail::borrow_t() };
}

template <typename T> T reinterpret_steal(handle h) {
    return { h, detail::steal_t() };
}

NAMESPACE_END(NB_NAMESPACE)
