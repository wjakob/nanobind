NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <bool ReturnHandle, typename Func, typename Return, typename... Args,
          size_t... Is, typename... Extra>
NB_INLINE PyObject *func_create(Func &&f, Return (*)(Args...),
                                std::index_sequence<Is...>,
                                const Extra &...extra) {
    // Detect locations of nb::args / nb::kwargs (if exists)
    static constexpr size_t
        args_pos_1 = index_1_v<std::is_same_v<intrinsic_t<Args>, args>...>,
        args_pos_n = index_n_v<std::is_same_v<intrinsic_t<Args>, args>...>,
        kwargs_pos_1 = index_1_v<std::is_same_v<intrinsic_t<Args>, kwargs>...>,
        kwargs_pos_n = index_n_v<std::is_same_v<intrinsic_t<Args>, kwargs>...>,
        nargs = sizeof...(Args);

    // Determine the number of nb::arg/nb::arg_v annotations
    constexpr size_t nargs_provided =
        ((std::is_same_v<arg, Extra> + std::is_same_v<arg_v, Extra>) + ...);

    /// A few compile-time consistency checks
    static_assert(args_pos_1 == args_pos_n && kwargs_pos_1 == kwargs_pos_n,
        "Repeated use of nb::kwargs or nb::args in the function signature!");
    static_assert(nargs_provided == 0 || nargs_provided == nargs,
        "The number of nb::arg annotations must match the argument count!");
    static_assert(kwargs_pos_1 == nargs || kwargs_pos_1 + 1 == nargs,
        "nb::kwargs must be the last element of the function signature!");
    static_assert(args_pos_1 == nargs || args_pos_1 + 1 == kwargs_pos_1,
        "nb::args must follow positional arguments and precede nb::kwargs!");

    // Collect function signature information for the docstring
    constexpr bool is_void_ret = std::is_void_v<Return>;
    using cast_out =
        make_caster<std::conditional_t<is_void_ret, std::nullptr_t, Return>>;
    constexpr auto descr =
        const_name("(") + concat(type_descr(make_caster<Args>::cname)...) +
        const_name(") -> ") + cast_out::cname;
    const std::type_info* descr_types[descr.type_count() + 1];
    descr.put_types(descr_types);

    // The following temporary record will describe the function in detail
    func_data<nargs_provided> data;

    // Auxiliary data structure to capture the provided function/closure
    struct capture {
        std::remove_reference_t<Func> f;
    };

    // Store the capture object in the function record if there is space
    constexpr bool is_small    = sizeof(capture) <= sizeof(data.capture),
                   is_trivial  = std::is_trivially_destructible_v<capture>;

    void (*free_capture)(void *ptr) = nullptr;

    /* No aliasing or need for std::launder() for 'capture' below. Problematic
       optimizations are avoided via separate compilation of libnanobind-core */
    if constexpr (is_small) {
        capture *cap = (capture *) data.capture;
        new (cap) capture{ (forward_t<Func>) f };

        if constexpr (!is_trivial) {
            data.free_capture = [](void *p) {
                ((capture *) p)->~capture();
            };
        } else {
            data.free_capture = nullptr;
        }
    } else {
        void **cap = (void **) data.capture;
        cap[0] = new capture{ (forward_t<Func>) f };

        data.free_capture = [](void *p) {
            delete (capture *) ((void **) p)[0];
        };
    }

    data.impl = [](void *p, PyObject **args, bool *args_convert,
                   PyObject *parent) __attribute__((__visibility__("hidden"))) -> PyObject * {
        const capture *cap;
        if constexpr (is_small)
            cap = (capture *) p;
        else
            cap = (capture *) ((void **) p)[0];

        nb_tuple<make_caster<Args>...> in;
        if ((!in.template get<Is>().load(args[Is], args_convert[Is]) || ...))
            return NB_NEXT_OVERLOAD;

        if constexpr (is_void_ret) {
            cap->f(
                in.template get<Is>().operator typename make_caster<Args>::
                    template cast_op_type<Args>()...),
            Py_INCREF(Py_None);
            return Py_None;
        } else {
            return cast_out::cast(
                cap->f(
                    in.template get<Is>().operator typename make_caster<Args>::
                        template cast_op_type<Args>()...),
                return_value_policy::move, parent).ptr();
        }
    };

    data.descr = descr.text;
    data.descr_types = descr_types;
    data.nargs = (uint32_t) nargs;
    data.nargs_provided = 0;
    data.flags = (args_pos_1   < nargs ? (uint32_t) func_flags::has_args   : 0) |
                 (kwargs_pos_1 < nargs ? (uint32_t) func_flags::has_kwargs : 0);

    // Fill remaining fields of 'data'
    detail::func_extra_init(data);
    (detail::func_extra_apply(data, extra), ...);

    return func_init((void *) &data, ReturnHandle);
}

NAMESPACE_END(detail)

template <typename Return, typename... Args, typename... Extra>
NB_INLINE object cpp_function(Return (*f)(Args...), const Extra&... extra) {
    return steal(detail::func_create<true>(
        f, f, std::make_index_sequence<sizeof...(Args)>(), extra...));
}

template <typename Return, typename... Args, typename... Extra>
NB_INLINE void cpp_function_def(Return (*f)(Args...), const Extra&... extra) {
    detail::func_create<false>(
        f, f, std::make_index_sequence<sizeof...(Args)>(), extra...);
}

/// Construct a cpp_function from a lambda function (pot. with internal state)
template <
    typename Func, typename... Extra,
    detail::enable_if_t<detail::is_lambda_v<std::remove_reference_t<Func>>> = 0>
NB_INLINE object cpp_function(Func &&f, const Extra &...extra) {
    using am = detail::analyze_method<decltype(&Func::operator())>;
    return steal(detail::func_create<true>(
        (detail::forward_t<Func>) f, (typename am::func *) nullptr,
        std::make_index_sequence<am::argc>(), extra...));
}

template <
    typename Func, typename... Extra,
    detail::enable_if_t<detail::is_lambda_v<std::remove_reference_t<Func>>> = 0>
NB_INLINE void cpp_function_def(Func &&f, const Extra &...extra) {
    using am = detail::analyze_method<decltype(&Func::operator())>;
    detail::func_create<false>(
        (detail::forward_t<Func>) f, (typename am::func *) nullptr,
        std::make_index_sequence<am::argc>(), extra...);
}

/// Construct a cpp_function from a class method (non-const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
NB_INLINE object cpp_function(Return (Class::*f)(Args...), const Extra &...extra) {
    return steal(detail::func_create<true>(
        [f](Class *c, Args... args) NB_INLINE_LAMBDA -> Return {
            return (c->*f)((detail::forward_t<Args>) args...);
        },
        (Return(*)(Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...));
}

template <typename Return, typename Class, typename... Args, typename... Extra>
NB_INLINE void cpp_function_def(Return (Class::*f)(Args...), const Extra &...extra) {
    detail::func_create<false>(
        [f](Class *c, Args... args) NB_INLINE_LAMBDA -> Return {
            return (c->*f)((detail::forward_t<Args>) args...);
        },
        (Return(*)(Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...);
}

/// Construct a cpp_function from a class method (const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
NB_INLINE object cpp_function(Return (Class::*f)(Args...) const, const Extra &...extra) {
    return steal(detail::func_create<true>(
        [f](const Class *c, Args... args) NB_INLINE_LAMBDA -> Return {
            return (c->*f)((detail::forward_t<Args>) args...);
        },
        (Return(*)(const Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...));
}

template <typename Return, typename Class, typename... Args, typename... Extra>
NB_INLINE void cpp_function_def(Return (Class::*f)(Args...) const, const Extra &...extra) {
    detail::func_create<false>(
        [f](const Class *c, Args... args) NB_INLINE_LAMBDA -> Return {
            return (c->*f)((detail::forward_t<Args>) args...);
        },
        (Return(*)(const Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...);
}

template <typename Func, typename... Extra>
module_ &module_::def(const char *name_, Func &&f,
                                const Extra &...extra) {
    cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                     extra...);
    return *this;
}

NAMESPACE_END(NB_NAMESPACE)
