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
#if PY_VERSION_HEX < 0x03090000
        // Simple call using only positional arguments
        tuple args_py = make_tuple<policy>((forward_t<Args>) args...);
        return steal(obj_call(derived().ptr(), args_py.release().ptr()));
#else
        /// More efficient version of the above using a vector call
        PyObject *args_py[sizeof...(Args) + 1],
                 **args_ptr = nullptr,
                 *base = nullptr;
        size_t nargs = 0;

        ((args_py[1 + nargs++] =
              detail::make_caster<Args>::cast((detail::forward_t<Args>) args,
                                              policy, nullptr).ptr()),
         ...);

        static constexpr bool method_call =
            std::is_same_v<Derived, accessor<obj_attr>> ||
            std::is_same_v<Derived, accessor<str_attr>>;

        if constexpr (method_call) {
            base = derived().key().release().ptr();
            args_py[0] = derived().base().inc_ref().ptr();
            args_ptr = args_py;
            nargs++;
        } else {
            base = derived().inc_ref().ptr();
            args_py[0] = nullptr;
            args_ptr = args_py + 1;
        }

        nargs |= PY_VECTORCALL_ARGUMENTS_OFFSET;

        return steal(
            obj_vectorcall(base, args_ptr, nargs, nullptr, method_call));
#endif
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
