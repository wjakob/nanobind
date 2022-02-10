NAMESPACE_BEGIN(nanobind)

struct scope {
    PyObject *value;
    scope(handle value) : value(value.ptr()) { }
};

struct name {
    const char *value;
    name(const char *value) : value(value) { }
};

struct arg_v;
struct arg {
    constexpr explicit arg(const char *name = nullptr) : name(name), convert_(true), none_(false) { }
    template <typename T> arg_v operator=(T &&value) const;
    arg &noconvert(bool value = true) { convert_ = !value; return *this; }
    arg &none(bool value = true) { none_ = value; return *this; }

    const char *name;
    bool convert_;
    bool none_;
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
    has_args   = (1 << 0),
    has_kwargs = (1 << 1),
    is_method  = (1 << 2)
};

struct arg_data {
    const char *name;
    PyObject *value;
    bool convert;
    bool none;
};

template <size_t Size> struct func_data {
    // A small amount of space to capture data used by the function/closure
    void *capture[3];

    // Callback to clean up the 'capture' field
    void (*free_capture)(void *);

    /// Implementation of the function call
    PyObject* (*impl) (void *, PyObject**, bool *, PyObject *);

    /// Function signature description
    const char *descr;

    /// C++ types referenced by 'descr'
    const std::type_info **descr_types;

    /// Total number of function call arguments
    uint16_t nargs;

    /// Number of arguments annotated via nb::args
    uint16_t nargs_provided;

    /// Supplementary flags
    uint32_t flags;

    // ------- Extra fields -------

    const char *name;
    const char *doc;
    PyObject *scope;
    arg_data args[Size];
};

template <typename F> void func_extra_init(F &f) {
    f.flags = 0;
    f.name = nullptr;
    f.doc = nullptr;
    f.scope = nullptr;
}

template <typename F> void func_extra_apply(F &f, const scope &scope) { f.scope = scope.value; }
template <typename F> void func_extra_apply(F &f, const name &name)   { f.name = name.value; }
template <typename F> void func_extra_apply(F &f, const char *doc) { f.doc = doc; }

template <typename F> void func_extra_apply(F &f, is_method) {
    f.flags |= (uint32_t) func_flags::is_method;
}

template <typename F> void func_extra_apply(F &f, const arg &a) {
    arg_data &arg = f.args[f.nargs_provided++];
    arg.name = a.name;
    arg.value = nullptr;
    arg.convert = a.convert_;
    arg.none = a.none_;
}

template <typename F> void func_extra_apply(F &f, const arg_v &a) {
    arg_data &arg = f.args[f.nargs_provided++];
    arg.name = a.name;
    arg.value = a.m_value.ptr();
    arg.convert = a.convert_;
    arg.none = a.none_;
}

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
