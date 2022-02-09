NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

template <typename Func, typename Return, typename... Args, size_t... Is,
          typename... Extra>
object func_create(Func &&f, Return (*)(Args...), std::index_sequence<Is...>,
                   const Extra &...extra) {
    static constexpr size_t
        args_pos_1 = index_1_v<std::is_same_v<std::decay_t<Args>, args>...>,
        kwargs_pos_1 = index_1_v<std::is_same_v<std::decay_t<Args>, kwargs>...>,
        args_pos_n = index_n_v<std::is_same_v<std::decay_t<Args>, args>...>,
        kwargs_pos_n = index_n_v<std::is_same_v<std::decay_t<Args>, kwargs>...>;

    static_assert(
        args_pos_1 == args_pos_n && kwargs_pos_1 == kwargs_pos_n,
        "Repeated use of nanobind::kwargs/args in function signature!");

    struct capture {
        std::remove_reference_t<Func> f;
    };

    // Store the capture object in the function record if there is space
    constexpr bool is_small    = sizeof(capture) <= sizeof(void *) * 3,
                   is_trivial  = std::is_trivially_destructible_v<capture>,
                   is_void_ret = std::is_void_v<Return>;

    void *func_rec = func_alloc();
    void (*free_capture)(void *ptr) = nullptr;

    if constexpr (is_small) {
        capture *cap = std::launder((capture *) func_rec);
        new (cap) capture{ std::forward<Func>(f) };

        if constexpr (!is_trivial) {
            free_capture = [](void *func_rec_2) {
                capture *cap_2 = std::launder((capture *) func_rec_2);
                cap_2->~capture();
            };
        }
    } else {
        void **cap = std::launder((void **) func_rec);
        cap[0] = new capture{ std::forward<Func>(f) };

        free_capture = [](void *func_rec_2) {
            void **cap_2 = std::launder((void **) func_rec_2);
            delete (capture *) cap_2[0];
        };
    }

    using cast_out =
        make_caster<std::conditional_t<is_void_ret, std::nullptr_t, Return>>;

    auto impl = [](void *func_rec_2, PyObject **args, bool *args_convert,
                   PyObject *parent) -> PyObject * {
        const capture *cap;
        if constexpr (is_small)
            cap = std::launder((capture *) func_rec_2);
        else
            cap = (capture *) std::launder((void **) func_rec_2)[0];

        nb_tuple<make_caster<Args>...> in;
        if ((... || !in.template get<Is>().load(args[Is], args_convert[Is])))
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

    (detail::func_apply(func_rec, extra), ...);

    static constexpr auto signature = const_name("(") +
                                      concat(type_descr(make_caster<Args>::name)...) +
                                      const_name(") -> ") + cast_out::name;

    const std::type_info* types[signature.type_count() + 1];
    signature.put_types(types);

    return steal(
        func_init(func_rec, sizeof...(Args), args_pos_1, kwargs_pos_1,
                  free_capture, impl, signature.text, types));
}


NAMESPACE_END(detail)

template <typename Return, typename... Args, typename... Extra>
object cpp_function(Return (*f)(Args...), const Extra&... extra) {
    return detail::func_create(
        f, f, std::make_index_sequence<sizeof...(Args)>(), extra...);
}

/// Construct a cpp_function from a lambda function (pot. with internal state)
template <
    typename Func, typename... Extra,
    detail::enable_if_t<detail::is_lambda_v<std::remove_reference_t<Func>>> = 0>
object cpp_function(Func &&f, const Extra &...extra) {
    using am = detail::analyze_method<decltype(&Func::operator())>;
    return detail::func_create(std::forward<Func>(f), (typename am::func *) nullptr,
                               std::make_index_sequence<am::argc>(), extra...);
}

/// Construct a cpp_function from a class method (non-const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
object cpp_function(Return (Class::*f)(Args...), const Extra &...extra) {
    return detail::func_create(
        [f](Class *c, Args... args) -> Return {
            return (c->*f)(std::forward<Args>(args)...);
        },
        (Return(*)(Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...);
}

/// Construct a cpp_function from a class method (const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
object cpp_function(Return (Class::*f)(Args...) const, const Extra &...extra) {
    return detail::func_create(
        [f](const Class *c, Args... args) -> Return {
            return (c->*f)(std::forward<Args>(args)...);
        },
        (Return(*)(const Class *, Args...)) nullptr,
        std::make_index_sequence<sizeof...(Args)>(), extra...);
}

template <typename Func, typename... Extra>
module_ &module_::def(const char *name_, Func &&f, const Extra &...extra) {
    object func = cpp_function(std::forward<Func>(f), name(name_), scope(*this),
                               pred(getattr(*this, name_, none())), extra...);
    if (PyModule_AddObject(m_ptr, name_, func.release().ptr()))
        detail::fail("module::def(): could not add object!");
    return *this;
}

NAMESPACE_END(nanobind)
