NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

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
    void (*free)(void *);
};

template <typename T> NB_INLINE void type_extra_apply(type_data &, const T &) { }

NB_INLINE void type_extra_apply(type_data &d, const handle &h) {
    d.base_py = (PyTypeObject *) h.ptr();
}

NB_INLINE void type_extra_apply(type_data &d, const char *doc) {
    d.doc = doc;
}

NAMESPACE_END(detail)

template <typename T, typename Base = T>
class class_ : public object {
public:
    NB_OBJECT_DEFAULT(class_, object, PyType_Check);

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
        data.free = [](void *value) { ((T *) value)->~T(); };

        (detail::type_extra_apply(&data, extra), ...);

        m_ptr = detail::type_new(&data);
    }

    template <typename Func, typename... Extra>
    class_ &def(const char *name_, Func &&f, const Extra &...extra) {
        cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                         is_method(), extra...);
        return *this;
    }
};

NAMESPACE_END(NB_NAMESPACE)
