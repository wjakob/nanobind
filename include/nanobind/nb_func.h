NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

template <typename Func, typename Return, typename... Args, typename... Extra>
object func_create(Func &&f, Return (*)(Args...), const Extra &...extra) {
    struct capture {
        std::remove_reference_t<Func> f;
    };

    // Store the capture object in the function record if there is space
    constexpr bool IsSmall = sizeof(capture) <= sizeof(void *) * 3;
    constexpr bool IsTrivial = std::is_trivially_destructible_v<capture>;

    void *func_rec = func_alloc();
    void (*free_capture)(void *ptr) = nullptr;

    if constexpr (IsSmall) {
        capture *cap = std::launder((capture *) func_rec);
        new (cap) capture{ std::forward<Func>(f) };

        if constexpr (!IsTrivial) {
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

    auto impl = [](void *func_rec_2) -> PyObject * {
        capture *cap;
        if constexpr (IsSmall)
            cap = std::launder((capture *) func_rec_2);
        else
            cap = std::launder((void **) func_rec_2)[0];

        cap->f();

        return nullptr;
    };

    (detail::func_apply(func_rec, extra), ...);

    return reinterpret_steal<object>(func_init(func_rec, free_capture, impl));
}

template <typename T>
constexpr bool is_lambda_v = !std::is_function_v<T> && !std::is_pointer_v<T> &&
                             !std::is_member_pointer_v<T>;


/// Strip the class from a method type
template <typename T> struct remove_class { };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...)> { using type = R (A...); };
template <typename C, typename R, typename... A> struct remove_class<R (C::*)(A...) const> { using type = R (A...); };

NAMESPACE_END(detail)

template <bool V> using enable_if_t = std::enable_if_t<V, int>;

template <typename Return, typename... Args, typename... Extra>
object cpp_function(Return (*f)(Args...), const Extra&... extra) {
    return detail::func_create(f, f, extra...);
}

/// Construct a cpp_function from a lambda function (possibly with internal state)
template <typename Func, typename... Extra,
          enable_if_t<detail::is_lambda_v<std::remove_reference_t<Func>>> = 0>
object cpp_function(Func &&f, const Extra &...extra) {
    using RawFunc =
        typename detail::remove_class<decltype(&Func::operator())>::type;
    return detail::func_create(std::forward<Func>(f), (RawFunc *) nullptr,
                               extra...);
}

/// Construct a cpp_function from a class method (non-const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
object cpp_function(Return (Class::*f)(Args...), const Extra&... extra) {
    return detail::func_create(
        [f](Class *c, Args... args) -> Return {
            return (c->*f)(std::forward<Args>(args)...);
        },
        (Return(*)(Class *, Args...)) nullptr, extra...);
}

/// Construct a cpp_function from a class method (const, no ref-qualifier)
template <typename Return, typename Class, typename... Args, typename... Extra>
object cpp_function(Return (Class::*f)(Args...) const, const Extra &...extra) {
    return detail::func_create(
        [f](const Class *c, Args... args) -> Return {
            return (c->*f)(std::forward<Args>(args)...);
        },
        (Return(*)(const Class *, Args...)) nullptr, extra...);
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
