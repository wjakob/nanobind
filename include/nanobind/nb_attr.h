NAMESPACE_BEGIN(nanobind)

struct scope {
    handle value;
    scope(handle value) : value(value) { }
};

struct pred {
    handle value;
    pred(handle value) : value(value) { }
};

struct name {
    const char *value;
    name(const char *value) : value(value) { }
};

struct arg_v;
struct arg {
    constexpr explicit arg(const char *name = nullptr) : m_name(name), m_convert(true), m_none(false) { }
    template <typename T> arg_v operator=(T &&value) const;
    arg &noconvert(bool value = true) { m_convert = !value; return *this; }
    arg &none(bool value = true) { m_none = value; return *this; }

    const char *m_name;
    bool m_convert;
    bool m_none;
};

struct arg_v : arg {
    object m_value;
    arg_v(const arg &base, object &&value) : arg(base), m_value(std::move(value)) { }
};

template <typename T> arg_v arg::operator=(T &&value) const {
    return arg_v(*this, cast(std::forward<T>(value)));
}

struct is_method { };

NAMESPACE_BEGIN(literals)
constexpr arg operator"" _a(const char *name, size_t) { return arg(name); }
NAMESPACE_END(literals)

NAMESPACE_BEGIN(detail)

enum class func_flags : uint32_t {
    is_method  = (1 << 0),
    has_args   = (1 << 1),
    has_kwargs = (1 << 2)
};


inline void func_apply(void *func_rec, const pred &pred) {
    func_set_pred(func_rec, pred.value.ptr());
}

inline void func_apply(void *func_rec, const scope &scope) {
    func_set_scope(func_rec, scope.value.ptr());
}

inline void func_apply(void *func_rec, const name &name) {
    func_set_name(func_rec, name.value);
}

inline void func_apply(void *func_rec, const char *docstr) {
    func_set_docstr(func_rec, docstr);
}

inline void func_apply(void *func_rec, is_method) {
    func_set_flag(func_rec, (uint32_t) func_flags::is_method);
}

inline void func_apply(void *func_rec, const arg &a) {
    func_add_arg(func_rec, a.m_name, a.m_convert, a.m_none, nullptr);
}

inline void func_apply(void *func_rec, const arg_v &a) {
    func_add_arg(func_rec, a.m_name, a.m_convert, a.m_none, a.m_value.ptr());
}

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
