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
NB_INLINE void call_prepare(size_t &nargs, size_t &nkwargs, const T &value) {
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, arg_v>) {
        nkwargs++;
    } else if constexpr (std::is_same_v<D, args_proxy>) {
        nargs += len(value);
    } else if constexpr (std::is_same_v<D, kwargs_proxy>) {
        nkwargs += len(value);
    } else {
        nargs += 1;
    }

    (void) nargs; (void) nkwargs; (void) value;
}

/// Implementation detail of api<T>::operator() (call operator)
template <rv_policy policy, typename T>
NB_INLINE void call_append(PyObject **args, PyObject *kwnames, size_t &args_i,
                           size_t &kwargs_i, const size_t kwargs_offset, T &&value) {
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, arg_v>) {
        printf("Case A\n");
        args[kwargs_i + kwargs_offset] = value.value.inc_ref().ptr();
        PyTuple_SET_ITEM(kwnames, kwargs_i++,
                         PyUnicode_InternFromString(value.name));
    } else if constexpr (std::is_same_v<D, args_proxy>) {
        printf("Case B\n");
        for (size_t i = 0, l = len(value); i < l; ++i)
            args[args_i++] = borrow(value[i]).release().ptr();
    } else if constexpr (std::is_same_v<D, kwargs_proxy>) {
        printf("Case C\n");
        PyObject *key, *entry;
        Py_ssize_t pos = 0;

        while (PyDict_Next(value.ptr(), &pos, &key, &entry)) {
            Py_INCREF(key); Py_INCREF(entry);
            args[kwargs_i + kwargs_offset] = entry;
            PyTuple_SET_ITEM(kwnames, kwargs_i++, key);
        }
    } else {
        printf("Case D\n");
        args[args_i++] =
            make_caster<T>::cast((forward_t<T>) value, policy, nullptr).ptr();
    }
    (void) args; (void) kwnames; (void) args_i; (void) kwargs_i; (void) kwargs_offset;
}

template <typename Derived>
template <rv_policy policy, typename... Args>
object api<Derived>::operator()(Args &&...args) const {
    static constexpr bool method_call =
        std::is_same_v<Derived, accessor<obj_attr>> ||
        std::is_same_v<Derived, accessor<str_attr>>;

    if constexpr (((std::is_same_v<Args, arg_v> ||
                    std::is_same_v<Args, args_proxy> ||
                    std::is_same_v<Args, kwargs_proxy>) || ...)) {
        // Complex call with keyword arguments, *args/**kwargs expansion, etc.
        size_t nargs = 0, nkwargs = 0;

        (call_prepare(nargs, nkwargs, (const Args &) args), ...);

        PyObject **args_py = (PyObject **) alloca(nargs + nkwargs + 1);
        PyObject *kwnames = nkwargs ? PyTuple_New((Py_ssize_t) nkwargs) : nullptr;

        size_t args_i = 0, kwargs_i = 0;
        (call_append<policy>(args_py + 1, kwnames, args_i, kwargs_i, nargs,
                             (forward_t<Args>) args), ...);

        printf("%zu %zu %zu %zu\n", nargs, nkwargs, args_i, kwargs_i);
        PyObject *base, **args_ptr;
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
            obj_vectorcall(base, args_ptr, nargs, kwnames, method_call));
    } else {
        // Simple version with only positional arguments
        PyObject *args_py[sizeof...(Args) + 1];
        size_t nargs = 0;

        ((args_py[1 + nargs++] =
              detail::make_caster<Args>::cast((detail::forward_t<Args>) args,
                                              policy, nullptr).ptr()),
         ...);

        PyObject **args_ptr, *base;
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
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
