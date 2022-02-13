NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#define NB_TRIVIAL_DESTRUCT ((void (*)(void *)) uintptr_t(1))
#define NB_TRIVIAL_COPY     ((void (*)(void *, const void *)) uintptr_t(1))
#define NB_TRIVIAL_MOVE     ((void (*)(void *, void *)) uintptr_t(1))

struct type_data {
    uint32_t size;
    uint32_t align;
    const char *name;
    const char *doc;
    PyObject *scope;
    const std::type_info *type;
    const std::type_info *base;
    PyTypeObject *type_py;
    PyTypeObject *base_py;
    void (*destruct)(void *);
    void (*copy)(void *, const void *);
    void (*move)(void *, void *);
};

template <typename T> NB_INLINE void type_extra_apply(type_data &, const T &) { }

NB_INLINE void type_extra_apply(type_data &d, const handle &h) {
    d.base_py = (PyTypeObject *) h.ptr();
}

NB_INLINE void type_extra_apply(type_data &d, const char *doc) {
    d.doc = doc;
}

template <typename... Args> struct init {
    template <typename Class, typename... Extra>
    NB_INLINE static void execute(Class &cl, const Extra&... extra) {
        using Value = typename Class::Value;
        cl.def(
            "__init__",
            [](Value *v, Args... args) {
                new (v) Value((forward_t<Args>) args...);
            },
            extra...);
    }
};

NAMESPACE_END(detail)

template <typename T, typename Base = T>
class class_ : public object {
public:
    NB_OBJECT_DEFAULT(class_, object, PyType_Check);
    using Value = T;

    template <typename... Extra>
    NB_INLINE class_(handle scope, const char *name, const Extra &... extra) {
        detail::type_data data;

        data.size = (uint32_t) sizeof(T);
        data.align = (uint32_t) alignof(T);
        data.name = name;
        data.doc = nullptr;
        data.scope = scope.ptr();
        data.type = &typeid(T);

        if constexpr (!std::is_same_v<T, Base>)
            data.base = &typeid(Base);
        else
            data.base = nullptr;

        data.type_py = nullptr;
        data.base_py = nullptr;

        if constexpr (std::is_destructible_v<T>)
            data.destruct = nullptr;
        if constexpr (std::is_trivially_destructible_v<T>)
            data.destruct = NB_TRIVIAL_DESTRUCT;
        else
            data.destruct = [](void *value) { ((T *) value)->~T(); };

        if constexpr (!std::is_copy_constructible_v<T>)
            data.copy = nullptr;
        else if constexpr (std::is_trivially_copy_constructible_v<T>)
            data.copy = NB_TRIVIAL_COPY;
        else
            data.copy = [](void *dst, const void *src) {
                new ((T *) dst) T(*(const T *) src);
            };

        if constexpr (!std::is_move_constructible_v<T>)
            data.move = nullptr;
        else if constexpr (std::is_trivially_move_constructible_v<T>)
            data.move = NB_TRIVIAL_MOVE;
        else
            data.move = [](void *dst, void *src) {
                new ((T *) dst) T(std::move(*(T *) src));
            };

        (detail::type_extra_apply(&data, extra), ...);

        m_ptr = detail::type_new(&data);
    }

    template <typename Func, typename... Extra>
    NB_INLINE class_ &def(const char *name_, Func &&f, const Extra &... extra) {
        cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                         is_method(), extra...);
        return *this;
    }

    template <typename Func, typename... Extra> class_ &
    def_static(const char *name_, Func &&f, const Extra&... extra) {
        static_assert(
            !std::is_member_function_pointer_v<Func>,
            "def_static(...) called with a non-static member function pointer");
        cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                         is_static(), extra...);
        return *this;
    }

    template <typename... Args, typename... Extra>
    NB_INLINE class_ &def(detail::init<Args...> init, const Extra &...extra) {
        init.execute(*this, extra...);
        return *this;
    }
};

template <typename... Args> NB_INLINE detail::init<Args...> init() { return { }; }

NAMESPACE_END(NB_NAMESPACE)
