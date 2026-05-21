#pragma once

#include <nanobind/xtensor/xview.h>
#include <xtensor/core/xvectorize.hpp>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <class F>
using get_function_type =
    xt::remove_class_t<decltype(&std::remove_reference_t<F>::operator())>;

NAMESPACE_END(detail)

template <class Func, class R, class... Args>
struct xvectorizer {
    xt::xvectorizer<Func, R> m_vectorizer;

    template <class F, class = std::enable_if_t<
        !std::is_same_v<std::decay_t<F>, xvectorizer>>>
    xvectorizer(F&& func)
        : m_vectorizer(std::forward<F>(func)) {}

    /// We use view to preserve zero-copy behaviour.
    /// The const modifier rejects the modification of the source array.
    auto operator()(const xarray_view<Args>&... args) const {
        return m_vectorizer(args...);
    }
};

template <class R, class... Args>
inline xvectorizer<R (*)(Args...), R, Args...> xvectorize(R (*f)(Args...)) {
    return xvectorizer<R (*)(Args...), R, Args...>(f);
}

template <class F, class R, class... Args>
inline xvectorizer<F, R, Args...> xvectorize(F&& f, R (*)(Args...)) {
    return xvectorizer<F, R, Args...>(std::forward<F>(f));
}

template <class F>
inline auto xvectorize(F&& f) {
    return xvectorize(std::forward<F>(f), (detail::get_function_type<F>*) nullptr);
}

NAMESPACE_END(NB_NAMESPACE)
