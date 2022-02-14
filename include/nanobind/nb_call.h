NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

class kwargs_proxy : public handle {
public:
    explicit kwargs_proxy(handle h) : handle(h) { }
};

class args_proxy : public handle {
public:
    explicit args_proxy(handle h) : handle(h) { }
    kwargs_proxy operator*() const { return kwargs_proxy(*this); }
};

template <typename Derived>
args_proxy api<Derived>::operator*() const {
    return args_proxy(derived().ptr());
}

/// Implementation detail of api<T>::operator() (call operator)
template <typename T>
NB_INLINE void call_prepare(size_t &nargs, bool &needs_kwargs, const T &value) {
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, arg_v>) {
        needs_kwargs = true;
    } else if constexpr (std::is_same_v<D, args_proxy>) {
        nargs += len(value);
    } else if constexpr (std::is_same_v<D, kwargs_proxy>) {
        needs_kwargs = true;
    } else {
        nargs += 1;
    }
    (void) nargs; (void) needs_kwargs; (void) value;
}

/// Implementation detail of api<T>::operator() (call operator)
template <rv_policy policy, typename T>
NB_INLINE void call_append(PyObject *args, PyObject *kwargs, size_t &nargs, T &&value) {
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, arg_v>) {
        call_append_kwarg(kwargs, value.name, value.value.ptr());
    } else if constexpr (std::is_same_v<D, args_proxy>) {
        call_append_args(args, nargs, value.ptr());
    } else if constexpr (std::is_same_v<D, kwargs_proxy>) {
        call_append_kwargs(kwargs, value.ptr());
    } else {
        call_append_arg(
            args, nargs,
            make_caster<T>::cast((forward_t<T>) value, policy, nullptr).ptr());
    }
    (void) args; (void) kwargs; (void) nargs;
}

template <typename Derived>
template <rv_policy policy, typename... Args>
object api<Derived>::operator()(Args &&...args) const {
    if constexpr (((std::is_same_v<Args, arg_v> ||
                    std::is_same_v<Args, args_proxy> ||
                    std::is_same_v<Args, kwargs_proxy>) || ...)) {
        size_t nargs = 0;
        bool needs_kwargs = false;

        (call_prepare(nargs, needs_kwargs, (const Args &) args), ...);

        tuple args_py = steal<tuple>(PyTuple_New((Py_ssize_t) nargs));
        dict kwargs_py = steal<dict>(needs_kwargs ? PyDict_New() : nullptr);

        nargs = 0;
        PyObject *args_o = args_py.ptr(), *kwargs_o = kwargs_py.ptr();
        (call_append<policy>(args_o, kwargs_o, nargs, (forward_t<Args>) args), ...);

        // Complex call with keyword arguments, *args/**kwargs expansion, etc.
        return steal(obj_call_kw(derived().ptr(), args_py.release().ptr(),
                                 kwargs_py.release().ptr()));
    } else {
        // Simple call using only positional arguments
        tuple args_py = make_tuple<policy>((forward_t<Args>) args...);
        return steal(obj_call(derived().ptr(), args_py.release().ptr()));
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
