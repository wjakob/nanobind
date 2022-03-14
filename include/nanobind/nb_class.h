/*
    nanobind/nb_class.h: Functionality for binding C++ classes/structs

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

enum class type_flags : uint32_t {
    /// Does the type provide a C++ destructor?
    is_destructible          = (1 << 0),

    /// Does the type provide a C++ copy constructor?
    is_copy_constructible    = (1 << 1),

    /// Does the type provide a C++ move constructor?
    is_move_constructible    = (1 << 2),

    /// Is this a python type that extends a bound C++ type?
    is_python_type           = (1 << 4),

    /// Is the 'scope' field of the type_data struture set?
    has_scope                = (1 << 5),

    /// Is the 'doc' field of the type_data struture set?
    has_doc                  = (1 << 6),

    /// Is the 'base' field of the type_data struture set?
    has_base                 = (1 << 7),

    /// Is the 'base_py' field of the type_data struture set?
    has_base_py              = (1 << 8),

    /// Is the 'destruct' field of the type_data struture set?
    has_destruct             = (1 << 9),

    /// Is the 'copy' field of the type_data struture set?
    has_copy                 = (1 << 10),

    /// Is the 'move' field of the type_data struture set?
    has_move                 = (1 << 11),

    /// Internal: does the type maintain a list of implicit conversions?
    has_implicit_conversions = (1 << 12),

    /// This type is a signed enumeration
    is_signed_enum           = (1 << 13),

    /// This type is an unsigned enumeration
    is_unsigned_enum         = (1 << 14),

    /// This type is an arithmetic enumeration
    is_arithmetic            = (1 << 15),

    /// This type is an arithmetic enumeration
    has_type_callback        = (1 << 16)
};

struct type_data {
    uint32_t size : 24;
    uint32_t align : 8;
    uint32_t flags : 24;
    uint32_t supplement : 8;
    const char *name;
    const char *doc;
    PyObject *scope;
    const std::type_info *type;
    const std::type_info *base;
    PyTypeObject *type_py;
    PyTypeObject *base_py;
    void (*destruct)(void *);
    void (*copy)(void *, const void *);
    void (*move)(void *, void *) noexcept;
    const std::type_info **implicit;
    bool (**implicit_py)(PyObject *, cleanup_list *) noexcept;
    void (*type_callback)(PyTypeObject *) noexcept;
};

static_assert(sizeof(type_data) == 8 + sizeof(void *) * 13);

NB_INLINE void type_extra_apply(type_data &t, const handle &h) {
    t.flags |= (uint32_t) type_flags::has_base_py;
    t.base_py = (PyTypeObject *) h.ptr();
}

NB_INLINE void type_extra_apply(type_data &t, const char *doc) {
    t.flags |= (uint32_t) type_flags::has_doc;
    t.doc = doc;
}

NB_INLINE void type_extra_apply(type_data &t, type_callback c) {
    t.flags |= (uint32_t) type_flags::has_type_callback;
    t.type_callback = c.value;
}

NB_INLINE void type_extra_apply(type_data &t, is_enum e) {
    if (e.is_signed)
        t.flags |= (uint32_t) type_flags::is_signed_enum;
    else
        t.flags |= (uint32_t) type_flags::is_unsigned_enum;
}

NB_INLINE void type_extra_apply(type_data &t, is_arithmetic) {
    t.flags |= (uint32_t) type_flags::is_arithmetic;
}

template <typename T>
NB_INLINE void type_extra_apply(type_data &t, supplement<T>) {
    static_assert(sizeof(T) <= 0xFF, "Supplement is too big!");
    t.supplement += sizeof(T);
}

template <typename... Args> struct init {
    template <typename Class, typename... Extra>
    NB_INLINE static void execute(Class &cl, const Extra&... extra) {
        using Type = typename Class::Type;
        using Alias = typename Class::Alias;
        static_assert(
            make_caster<Type>::IsClass,
            "Attempted to create a constructor for a type that won't be "
            "handled by the nanobind's class type caster. Is it possible that "
            "you forgot to add NB_MAKE_OPAQUE() somewhere?");

        cl.def(
            "__init__",
            [](Type *v, Args... args) {
                new ((Alias *) v) Alias{ (forward_t<Args>) args... };
            },
            extra...);
    }
};

template <typename Arg> struct init_implicit {
    template <typename Class, typename... Extra>
    NB_INLINE static void execute(Class &cl, const Extra&... extra) {
        using Type = typename Class::Type;
        using Alias = typename Class::Alias;
        using Caster = make_caster<Arg>;
        static_assert(
            make_caster<Type>::IsClass,
            "Attempted to create a constructor for a type that won't be "
            "handled by the nanobind's class type caster. Is it possible that "
            "you forgot to add NB_MAKE_OPAQUE() somewhere?");

        cl.def(
            "__init__",
            [](Type *v, Arg arg) {
                new ((Alias *) v) Alias{ (forward_t<Arg>) arg };
            }, is_implicit(), extra...);

        if constexpr (!Caster::IsClass) {
            implicitly_convertible(
                [](PyObject *src, cleanup_list *cleanup) noexcept -> bool {
                    return Caster().from_python(src, cast_flags::convert,
                                                cleanup);
                },
                &typeid(Type));
        }
    }
};

template <typename T> void wrap_copy(void *dst, const void *src) {
    new ((T *) dst) T(*(const T *) src);
}

template <typename T> void wrap_move(void *dst, void *src) noexcept {
    new ((T *) dst) T(std::move(*(T *) src));
}

template <typename T> void wrap_destruct(void *value) noexcept {
    ((T *) value)->~T();
}

template <typename, template <typename, typename> typename, typename...>
struct extract;

template <typename T, template <typename, typename> typename Pred>
struct extract<T, Pred> {
    using type = T;
};

template <typename T, template <typename, typename> typename Pred,
          typename Tv, typename... Ts>
struct extract<T, Pred, Tv, Ts...> {
    using type = std::conditional_t<
        Pred<T, Tv>::value,
        Tv,
        typename extract<T, Pred, Ts...>::type
    >;
};

template <typename T, typename Arg> using is_alias = std::is_base_of<T, Arg>;
template <typename T, typename Arg> using is_base = std::is_base_of<Arg, T>;

enum op_id : int;
enum op_type : int;
struct undefined_t;
template <op_id id, op_type ot, typename L = undefined_t, typename R = undefined_t> struct op_;

NAMESPACE_END(detail)

template <typename T, typename... Ts>
class class_ : public object {
public:
    NB_OBJECT_DEFAULT(class_, object, PyType_Check);
    using Type = T;
    using Base  = typename detail::extract<T, detail::is_base,  Ts...>::type;
    using Alias = typename detail::extract<T, detail::is_alias, Ts...>::type;

    static_assert(sizeof(Alias) < (1 << 24), "instance size is too big!");
    static_assert(alignof(Alias) < (1 << 8), "instance alignment is too big!");
    static_assert(
        sizeof...(Ts) == !std::is_same_v<Base, T> + !std::is_same_v<Alias, T>,
        "nanobind::class_<> was invoked with extra arguments that could not be handled");

    template <typename... Extra>
    NB_INLINE class_(handle scope, const char *name, const Extra &... extra) {
        detail::type_data d;

        d.flags = (uint32_t) detail::type_flags::has_scope;
        d.align = (uint8_t) alignof(Alias);
        d.size = (uint32_t) sizeof(Alias);
        d.supplement = 0;
        d.name = name;
        d.scope = scope.ptr();
        d.type = &typeid(T);

        if constexpr (!std::is_same_v<Base, T>) {
            d.base = &typeid(Base);
            d.flags |= (uint16_t) detail::type_flags::has_base;
        }

        if constexpr (std::is_copy_constructible_v<T>) {
            d.flags |= (uint16_t) detail::type_flags::is_copy_constructible;

            if constexpr (!std::is_trivially_copy_constructible_v<T>) {
                d.flags |= (uint16_t) detail::type_flags::has_copy;
                d.copy = detail::wrap_copy<T>;
            }
        }

        if constexpr (std::is_move_constructible_v<T>) {
            d.flags |= (uint16_t) detail::type_flags::is_move_constructible;

            if constexpr (!std::is_trivially_move_constructible_v<T>) {
                d.flags |= (uint16_t) detail::type_flags::has_move;
                d.move = detail::wrap_move<T>;
            }
        }

        if constexpr (std::is_destructible_v<T>) {
            d.flags |= (uint16_t) detail::type_flags::is_destructible;

            if constexpr (!std::is_trivially_destructible_v<T>) {
                d.flags |= (uint16_t) detail::type_flags::has_destruct;
                d.destruct = detail::wrap_destruct<T>;
            }
        }

        (detail::type_extra_apply(d, extra), ...);

        m_ptr = detail::nb_type_new(&d);
    }

    template <typename Func, typename... Extra>
    NB_INLINE class_ &def(const char *name_, Func &&f, const Extra &... extra) {
        cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                         is_method(), extra...);
        return *this;
    }

    template <typename... Args, typename... Extra>
    NB_INLINE class_ &def(detail::init<Args...> init, const Extra &... extra) {
        init.execute(*this, extra...);
        return *this;
    }

    template <typename... Args, typename... Extra>
    NB_INLINE class_ &def(detail::init_implicit<Args...> init,
                          const Extra &... extra) {
        init.execute(*this, extra...);
        return *this;
    }

    template <typename Func, typename... Extra>
    NB_INLINE class_ &def_static(const char *name_, Func &&f,
                                 const Extra &... extra) {
        static_assert(
            !std::is_member_function_pointer_v<Func>,
            "def_static(...) called with a non-static member function pointer");
        cpp_function_def((detail::forward_t<Func>) f, scope(*this), name(name_),
                         extra...);
        return *this;
    }

    template <typename Getter, typename Setter, typename... Extra>
    NB_INLINE class_ &def_property(const char *name_, Getter &&getter,
                                   Setter &&setter, const Extra &...extra) {
        object get_p, set_p;

        if constexpr (!std::is_same_v<Getter, std::nullptr_t>)
            get_p = cpp_function((detail::forward_t<Getter>) getter,
                                 scope(*this), is_method(),
                                 rv_policy::reference_internal, extra...);

        if constexpr (!std::is_same_v<Setter, std::nullptr_t>)
            set_p = cpp_function((detail::forward_t<Setter>) setter,
                                 scope(*this), is_method(), extra...);

        detail::property_install(m_ptr, name_, false, get_p.ptr(),
                                 set_p.ptr());
        return *this;
    }

    template <typename Getter, typename Setter, typename... Extra>
    NB_INLINE class_ &def_property_static(const char *name_, Getter &&getter,
                                          Setter &&setter,
                                          const Extra &...extra) {
        object get_p, set_p;

        if constexpr (!std::is_same_v<Getter, std::nullptr_t>)
            get_p = cpp_function((detail::forward_t<Getter>) getter,
                                 scope(*this), rv_policy::reference, extra...);

        if constexpr (!std::is_same_v<Setter, std::nullptr_t>)
            set_p = cpp_function((detail::forward_t<Setter>) setter,
                                 scope(*this), extra...);

        detail::property_install(m_ptr, name_, true, get_p.ptr(),
                                 set_p.ptr());
        return *this;
    }

    template <typename Getter, typename... Extra>
    NB_INLINE class_ &def_property_readonly(const char *name_, Getter &&getter,
                                            const Extra &...extra) {
        return def_property(name_, getter, nullptr, extra...);
    }

    template <typename Getter, typename... Extra>
    NB_INLINE class_ &def_property_readonly_static(const char *name_,
                                                   Getter &&getter,
                                                   const Extra &...extra) {
        return def_property_static(name_, getter, nullptr, extra...);
    }

    template <typename C, typename D, typename... Extra>
    NB_INLINE class_ &def_readwrite(const char *name, D C::*pm,
                                    const Extra &...extra) {
        static_assert(std::is_base_of_v<C, T>,
                      "def_readwrite() requires a (base) class member!");

        def_property(name,
            [pm](const T &c) -> const D & { return c.*pm; },
            [pm](T &c, const D &value) { c.*pm = value; },
            extra...);

        return *this;
    }

    template <typename D, typename... Extra>
    NB_INLINE class_ &def_readwrite_static(const char *name, D *pm,
                                           const Extra &...extra) {
        def_property_static(name,
            [pm](handle) -> const D & { return *pm; },
            [pm](handle, const D &value) { *pm = value; }, extra...);

        return *this;
    }

    template <typename C, typename D, typename... Extra>
    NB_INLINE class_ &def_readonly(const char *name, D C::*pm,
                                   const Extra &...extra) {
        static_assert(std::is_base_of_v<C, T>,
                      "def_readonly() requires a (base) class member!");

        def_property_readonly(name,
            [pm](const T &c) -> const D & { return c.*pm; }, extra...);

        return *this;
    }

    template <typename D, typename... Extra>
    NB_INLINE class_ &def_readonly_static(const char *name, D *pm,
                                          const Extra &...extra) {
        def_property_readonly_static(name,
            [pm](handle) -> const D & { return *pm; }, extra...);

        return *this;
    }

    template <detail::op_id id, detail::op_type ot, typename L, typename R, typename... Extra>
    class_ &def(const detail::op_<id, ot, L, R> &op, const Extra&... extra) {
        op.execute(*this, extra...);
        return *this;
    }

    template <detail::op_id id, detail::op_type ot, typename L, typename R, typename... Extra>
    class_ & def_cast(const detail::op_<id, ot, L, R> &op, const Extra&... extra) {
        op.execute_cast(*this, extra...);
        return *this;
    }

};

template <typename T> class enum_ : public class_<T> {
public:
    static_assert(std::is_enum_v<T>, "nanobind::enum_<> requires an enumeration type!");

    using Base = class_<T>;

    template <typename... Extra>
    NB_INLINE enum_(handle scope, const char *name, const Extra &...extra)
        : Base(scope, name, extra...,
               is_enum{ std::is_signed_v<std::underlying_type_t<T>> }) { }

    NB_INLINE enum_ &value(const char *name, T value, const char *doc = nullptr) {
        detail::nb_enum_put(Base::m_ptr, name, &value, doc);
        return *this;
    }
};

template <typename... Args> NB_INLINE detail::init<Args...> init() { return { }; }
template <typename Arg> NB_INLINE detail::init_implicit<Arg> init_implicit() { return { }; }

template <typename T>
inline T &type_supplement(handle h) { return *(T *) detail::nb_type_extra(h.ptr()); }

template <typename T> T *instance(PyObject *o) {
    return (T *) detail::nb_inst_data(o);
}

NAMESPACE_END(NB_NAMESPACE)
