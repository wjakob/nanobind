/*
    nanobind/nb_attr.h: Annotations for function and class declarations

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

NAMESPACE_BEGIN(detail)

enum cast_flags : uint8_t {
    // Enable implicit conversions (code assumes this has value 1, don't reorder..)
    convert = (1 << 0),

    // Passed to the 'self' argument in a constructor call (__init__)
    construct = (1 << 1),

    // Indicates that the function dispatcher should accept 'None' arguments
    accepts_none = (1 << 2),

    /// The target binds the value by reference or value (not as a pointer), so
    /// a 'None' argument has no valid mapping.
    none_disallowed = (1 << 3),

    // Indicates that this cast is performed by nb::cast or nb::try_cast.
    // This implies that objects added to the cleanup list may be
    // released immediately after the caster's final output value is
    // obtained, i.e., before it is used.
    manual = (1 << 4),

    /// Indicate that a type is being constructed by nb_type_vectorcall. The
    /// call dispatcher uses this hint to avoid type-checking ``self``
    trusted = (1 << 5)
};

inline constexpr uint8_t arg_flag_convert = (uint8_t) cast_flags::convert;
inline constexpr uint8_t arg_flag_accepts_none =
    (uint8_t) cast_flags::accepts_none;

template <uint8_t Flags>
inline constexpr uint8_t arg_flags_no_convert =
    (uint8_t) (Flags & (uint8_t) ~arg_flag_convert);

template <uint8_t Flags>
inline constexpr uint8_t arg_flags_accepts_none =
    (uint8_t) (Flags | arg_flag_accepts_none);

NAMESPACE_END(detail)

struct scope {
    PyObject *value;
    NB_INLINE scope(handle value) : value(value.ptr()) {}
};

struct name {
    const char *value;
    NB_INLINE name(const char *value) : value(value) {}
};

template <uint8_t Flags> struct arg_tag;
template <uint8_t Flags> struct arg_v_tag;
template <uint8_t Flags> struct arg_locked_tag;
template <uint8_t Flags> struct arg_locked_v_tag;
struct arg_locked;
struct arg_v;
struct arg_locked_v;

// Basic function argument descriptor (no default value, not locked)
struct arg {
    static constexpr uint8_t flags = detail::arg_flag_convert;

    NB_INLINE constexpr explicit arg(const char *name = nullptr) : name_(name), signature_(nullptr) { }

    // operator= can be used to provide a default value
    template <typename T> NB_INLINE arg_v operator=(T &&value) const;

    // Mutators that don't change default value or locked state
    NB_INLINE arg_tag<detail::arg_flags_no_convert<flags>> noconvert() const;
    NB_INLINE arg_tag<detail::arg_flags_accepts_none<flags>> none() const;
    NB_INLINE arg &sig(const char *value) {
        signature_ = value;
        return *this;
    }

    // After lock(), this argument is locked
    NB_INLINE arg_locked lock();

    const char *name_, *signature_;
};

// Function argument descriptor with default value (not locked)
struct arg_v : arg {
    static constexpr uint8_t flags = detail::arg_flag_convert;

    object value;
    NB_INLINE arg_v(const arg &base, object &&value)
        : arg(base), value(std::move(value)) {}

  private:
    // Inherited mutators would slice off the default, and are not generally needed
    using arg::noconvert;
    using arg::none;
    using arg::sig;
    using arg::lock;
};

template <uint8_t Flags>
struct arg_v_tag : arg_v {
    static constexpr uint8_t flags = Flags;

    NB_INLINE arg_v_tag(const arg &base, object &&value)
        : arg_v(base, std::move(value)) {}
};

// Function argument descriptor that is locked (no default value)
struct arg_locked : arg {
    static constexpr uint8_t flags = detail::arg_flag_convert;

    NB_INLINE constexpr explicit arg_locked(const char *name = nullptr) : arg(name) { }
    NB_INLINE constexpr explicit arg_locked(const arg &base) : arg(base) { }

    // operator= can be used to provide a default value
    template <typename T> NB_INLINE arg_locked_v operator=(T &&value) const;

    // Mutators must be respecified in order to not slice off the locked status
    NB_INLINE arg_locked_tag<detail::arg_flags_no_convert<flags>> noconvert() const;
    NB_INLINE arg_locked_tag<detail::arg_flags_accepts_none<flags>> none() const;
    NB_INLINE arg_locked &sig(const char *value) {
        signature_ = value;
        return *this;
    }

    // Redundant extra lock() is allowed
    NB_INLINE arg_locked &lock() { return *this; }
};

// Function argument descriptor that is potentially locked and has a default value
struct arg_locked_v : arg_locked {
    static constexpr uint8_t flags = detail::arg_flag_convert;

    object value;
    NB_INLINE arg_locked_v(const arg_locked &base, object &&value)
        : arg_locked(base), value(std::move(value)) {}

  private:
    // Inherited mutators would slice off the default, and are not generally needed
    using arg_locked::noconvert;
    using arg_locked::none;
    using arg_locked::sig;
    using arg_locked::lock;
};

template <uint8_t Flags>
struct arg_locked_v_tag : arg_locked_v {
    static constexpr uint8_t flags = Flags;

    NB_INLINE arg_locked_v_tag(const arg_locked &base, object &&value)
        : arg_locked_v(base, std::move(value)) {}
};

template <uint8_t Flags>
struct arg_tag : arg {
    static constexpr uint8_t flags = Flags;

    NB_INLINE constexpr explicit arg_tag(const char *name = nullptr) : arg(name) { }
    NB_INLINE constexpr explicit arg_tag(const arg &base) : arg(base) { }

    template <typename T> NB_INLINE arg_v_tag<Flags> operator=(T &&value) const;

    NB_INLINE arg_tag<detail::arg_flags_no_convert<Flags>> noconvert() const {
        return arg_tag<detail::arg_flags_no_convert<Flags>>(*this);
    }
    NB_INLINE arg_tag<detail::arg_flags_accepts_none<Flags>> none() const {
        return arg_tag<detail::arg_flags_accepts_none<Flags>>(*this);
    }
    NB_INLINE arg_tag &sig(const char *value) {
        signature_ = value;
        return *this;
    }
    NB_INLINE arg_locked_tag<Flags> lock() const;
};

template <uint8_t Flags>
struct arg_locked_tag : arg_locked {
    static constexpr uint8_t flags = Flags;

    NB_INLINE constexpr explicit arg_locked_tag(const char *name = nullptr) : arg_locked(name) { }
    NB_INLINE constexpr explicit arg_locked_tag(const arg &base) : arg_locked(base) { }

    template <typename T> NB_INLINE arg_locked_v_tag<Flags> operator=(T &&value) const;

    NB_INLINE arg_locked_tag<detail::arg_flags_no_convert<Flags>> noconvert() const {
        return arg_locked_tag<detail::arg_flags_no_convert<Flags>>(*this);
    }
    NB_INLINE arg_locked_tag<detail::arg_flags_accepts_none<Flags>> none() const {
        return arg_locked_tag<detail::arg_flags_accepts_none<Flags>>(*this);
    }
    NB_INLINE arg_locked_tag &sig(const char *value) {
        signature_ = value;
        return *this;
    }
    NB_INLINE const arg_locked_tag &lock() const { return *this; }
};

NB_INLINE arg_tag<detail::arg_flags_no_convert<arg::flags>>
arg::noconvert() const {
    return arg_tag<detail::arg_flags_no_convert<flags>>(*this);
}

NB_INLINE arg_tag<detail::arg_flags_accepts_none<arg::flags>>
arg::none() const {
    return arg_tag<detail::arg_flags_accepts_none<flags>>(*this);
}

NB_INLINE arg_locked arg::lock() { return arg_locked{*this}; }

NB_INLINE arg_locked_tag<detail::arg_flags_no_convert<arg_locked::flags>>
arg_locked::noconvert() const {
    return arg_locked_tag<detail::arg_flags_no_convert<flags>>(*this);
}

NB_INLINE arg_locked_tag<detail::arg_flags_accepts_none<arg_locked::flags>>
arg_locked::none() const {
    return arg_locked_tag<detail::arg_flags_accepts_none<flags>>(*this);
}

template <uint8_t Flags>
NB_INLINE arg_locked_tag<Flags>
arg_tag<Flags>::lock() const {
    return arg_locked_tag<Flags>(*this);
}

template <typename T>
inline constexpr bool is_arg_annotation_v =
    std::is_base_of_v<arg, std::decay_t<T>>;

template <typename T>
inline constexpr bool is_arg_default_annotation_v =
    std::is_base_of_v<arg_v, std::decay_t<T>> ||
    std::is_base_of_v<arg_locked_v, std::decay_t<T>>;

NAMESPACE_BEGIN(detail)

template <typename T, bool IsArg = is_arg_annotation_v<T>>
inline constexpr uint8_t arg_flags_v = arg_flag_convert;

template <typename T>
inline constexpr uint8_t arg_flags_v<T, true> = std::decay_t<T>::flags;

NAMESPACE_END(detail)

template <typename... Ts> struct call_guard {
    using type = detail::tuple<Ts...>;
};

struct dynamic_attr {};
struct is_weak_referenceable {};
struct is_method {};
struct is_implicit {};
struct is_operator {};
struct is_arithmetic {};
struct is_flag {};
struct is_str {};
struct is_final {};
struct is_generic {};
struct kw_only {};
struct lock_self {};
struct never_destruct {};

struct pooled {
    explicit pooled(uint32_t capacity = 128) : capacity(capacity) {}
    uint32_t capacity;
};

template <size_t /* Nurse */, size_t /* Patient */> struct keep_alive {};
template <typename T> struct supplement {};
template <typename T> struct intrusive_ptr {
    intrusive_ptr(void (*set_self_py)(T *, PyObject *) noexcept)
        : set_self_py(set_self_py) { }
    void (*set_self_py)(T *, PyObject *) noexcept;
};

struct type_slots {
    type_slots (const PyType_Slot *value) : value(value) { }
    const PyType_Slot *value;
};

struct type_slots_callback {
    using cb_t = void (*)(const detail::type_data_init *t,
                          PyType_Slot *&slots, size_t max_slots) noexcept;
    type_slots_callback(cb_t callback) : callback(callback) { }
    cb_t callback;
};

struct sig {
    const char *value;
    sig(const char *value) : value(value) { }
};

struct is_getter { };

template <typename Policy> struct call_policy final {};

NAMESPACE_BEGIN(literals)
constexpr arg operator""_a(const char *name, size_t) { return arg(name); }
NAMESPACE_END(literals)

NAMESPACE_BEGIN(detail)

enum class func_flags : uint32_t {
    /* Low 3 bits reserved for return value policy */

    /// Did the user specify a name for this function, or is it anonymous?
    has_name = (1 << 4),
    /// Did the user specify a scope in which this function should be installed?
    has_scope = (1 << 5),
    /// Did the user specify a docstring?
    has_doc = (1 << 6),
    /// Did the user specify nb::arg/arg_v annotations for all arguments?
    has_args = (1 << 7),
    /// Does the function signature contain an *args-style argument?
    has_var_args = (1 << 8),
    /// Does the function signature contain an *kwargs-style argument?
    has_var_kwargs = (1 << 9),
    /// Is this function a method of a class?
    is_method = (1 << 10),
    /// Is this function a method called __init__? (automatically generated)
    is_constructor = (1 << 11),
    /// Can this constructor be used to perform an implicit conversion?
    is_implicit = (1 << 12),
    /// Is this function an arithmetic operator?
    is_operator = (1 << 13),
    /// When the function is GCed, do we need to call func_data_init::free_capture?
    has_free = (1 << 14),
    /// Should the func_new() call return a new reference?
    return_ref = (1 << 15),
    /// Does this overload specify a custom function signature (for docstrings, typing)
    has_signature = (1 << 16),
    /// Does this function potentially modify the elements of the PyObject*[] array
    /// representing its arguments? (nb::keep_alive() or call_policy annotations)
    can_mutate_args = (1 << 17),
    /// Is this a copy constructor whose source argument must not convert?
    is_copy_constructor = (1 << 18)
};

enum call_flags : uint8_t {
    /// Current dispatch pass permits implicit conversions.
    dispatch_convert = (uint8_t) cast_flags::convert,

    /// The first argument is constructor self.
    dispatch_construct = (uint8_t) cast_flags::construct,

    /// Constructor self came from nb_type_vectorcall and can be trusted.
    dispatch_trusted = (uint8_t) cast_flags::trusted,

    /// Copy constructor source argument must not implicitly convert.
    dispatch_copy_constructor = (1 << 6)
};


struct arg_data_init {
    const char *name;
    const char *signature;
    PyObject *name_py;
    PyObject *value;
    uint16_t flag;
};

struct func_data_init_base {
    // A small amount of space to capture data used by the function/closure
    void *capture[3];

    // Callback to clean up the 'capture' field
    void (*free_capture)(void *);

    /// Implementation of the function call
    PyObject *(*impl)(void *, PyObject **, uint8_t, nb_internals *,
                      cleanup_list *);

    /// Function signature description
    const char *descr;

    /// C++ types referenced by 'descr'
    const std::type_info **descr_types;

    /// Supplementary flags
    uint32_t flags;

    /// Total number of parameters accepted by the C++ function; nb::args
    /// and nb::kwargs parameters are counted as one each. If the
    /// 'has_args' flag is set, then there is one arg_data_init structure
    /// for each of these.
    uint16_t nargs;

    /// Number of parameters to the C++ function that may be filled from
    /// Python positional arguments without additional ceremony.
    /// nb::args and nb::kwargs parameters are not counted in this total, nor
    /// are any parameters after nb::args or after a nb::kw_only annotation.
    /// The parameters counted here may be either named (nb::arg("name")) or
    /// unnamed (nb::arg()).  If unnamed, they are effectively positional-only.
    /// nargs_pos is always <= nargs.
    uint16_t nargs_pos;

    // ------- Extra fields -------

    const char *name;
    const char *doc;
    PyObject *scope;
};

template<size_t Size> struct func_data_init : func_data_init_base {
    arg_data_init args[Size];
};

template<> struct func_data_init<0> : func_data_init_base {};

// Freeze guards: arg_data_init / func_data_init_base are baked into compiled
// extensions and may only grow by appending fields. Pin the 64-bit layout so an
// accidental reorder or resize fails the build (other ABIs are not constrained).
#define NB_FROZEN_OFF(S, F, V)                                                  \
    static_assert(sizeof(void *) != 8 || offsetof(S, F) == (V),                \
                  "frozen ABI layout of " #S "::" #F " changed")
static_assert(sizeof(void *) != 8 || sizeof(arg_data_init) == 40,
              "frozen ABI size of arg_data_init changed");
NB_FROZEN_OFF(arg_data_init, name, 0);
NB_FROZEN_OFF(arg_data_init, signature, 8);
NB_FROZEN_OFF(arg_data_init, name_py, 16);
NB_FROZEN_OFF(arg_data_init, value, 24);
NB_FROZEN_OFF(arg_data_init, flag, 32);
static_assert(sizeof(void *) != 8 || sizeof(func_data_init_base) == 88,
              "frozen ABI size of func_data_init_base changed");
NB_FROZEN_OFF(func_data_init_base, capture, 0);
NB_FROZEN_OFF(func_data_init_base, free_capture, 24);
NB_FROZEN_OFF(func_data_init_base, impl, 32);
NB_FROZEN_OFF(func_data_init_base, descr, 40);
NB_FROZEN_OFF(func_data_init_base, descr_types, 48);
NB_FROZEN_OFF(func_data_init_base, flags, 56);
NB_FROZEN_OFF(func_data_init_base, nargs, 60);
NB_FROZEN_OFF(func_data_init_base, nargs_pos, 62);
NB_FROZEN_OFF(func_data_init_base, name, 64);
NB_FROZEN_OFF(func_data_init_base, doc, 72);
NB_FROZEN_OFF(func_data_init_base, scope, 80);
#undef NB_FROZEN_OFF


template <typename F>
NB_INLINE void func_extra_apply(F &f, const name &name, size_t &) {
    f.name = name.value;
    f.flags |= (uint32_t) func_flags::has_name;
}

template <typename F>
NB_INLINE void func_extra_apply(F &f, const scope &scope, size_t &) {
    f.scope = scope.value;
    f.flags |= (uint32_t) func_flags::has_scope;
}

template <typename F>
NB_INLINE void func_extra_apply(F &f, const sig &s, size_t &) {
    f.flags |= (uint32_t) func_flags::has_signature;
    f.name = s.value;
}

template <typename F>
NB_INLINE void func_extra_apply(F &f, const char *doc, size_t &) {
    f.doc = doc;
    f.flags |= (uint32_t) func_flags::has_doc;
}

template <typename F>
NB_INLINE void func_extra_apply(F &f, is_method, size_t &) {
    f.flags |= (uint32_t) func_flags::is_method;
}

template <typename F>
NB_INLINE void func_extra_apply(F &, is_getter, size_t &) { }

template <typename F>
NB_INLINE void func_extra_apply(F &f, is_implicit, size_t &) {
    f.flags |= (uint32_t) func_flags::is_implicit;
}

template <typename F>
NB_INLINE void func_extra_apply(F &f, is_operator, size_t &) {
    f.flags |= (uint32_t) func_flags::is_operator;
}

template <typename F, rv_policy::value Policy>
NB_INLINE void func_extra_apply(F &f, rv_policy::policy_tag<Policy>, size_t &) {
    f.flags = (f.flags & (uint32_t) ~0b111) | (uint32_t) Policy;
}

template <typename F>
NB_INLINE void func_extra_apply(F &, std::nullptr_t, size_t &) { }

template <typename F, typename Arg, enable_if_t<is_arg_annotation_v<Arg>> = 0>
NB_INLINE void func_extra_apply(F &f, const Arg &a, size_t &index) {
    arg_data_init &ad = f.args[index++];
    ad.flag = arg_flags_v<Arg>;
    ad.name = a.name_;
    ad.signature = a.signature_;
    if constexpr (is_arg_default_annotation_v<Arg>)
        ad.value = a.value.ptr();
    else
        ad.value = nullptr;
}

template <typename F>
NB_INLINE void func_extra_apply(F &, kw_only, size_t &) {}

template <typename F>
NB_INLINE void func_extra_apply(F &, lock_self, size_t &) {}

template <typename F, typename... Ts>
NB_INLINE void func_extra_apply(F &, call_guard<Ts...>, size_t &) {}

template <typename F, size_t Nurse, size_t Patient>
NB_INLINE void func_extra_apply(F &f, nanobind::keep_alive<Nurse, Patient>, size_t &) {
    f.flags |= (uint32_t) func_flags::can_mutate_args;
}

template <typename F, typename Policy>
NB_INLINE void func_extra_apply(F &f, call_policy<Policy>, size_t &) {
    f.flags |= (uint32_t) func_flags::can_mutate_args;
}

template <typename... Ts> struct func_extra_info {
    using call_guard = void;
    static constexpr bool pre_post_hooks = false;
    static constexpr size_t nargs_locked = 0;
    static constexpr bool has_policy = false;
    static constexpr rv_policy::value policy = rv_policy::automatic_v;
};

template <typename T, typename... Ts> struct func_extra_info<T, Ts...>
    : func_extra_info<Ts...> {
    static constexpr size_t nargs_locked =
        func_extra_info<Ts...>::nargs_locked +
        (std::is_base_of_v<arg_locked, std::decay_t<T>> ? 1 : 0);
};

template <rv_policy::value Policy, typename... Ts>
struct func_extra_info<rv_policy::policy_tag<Policy>, Ts...>
    : func_extra_info<Ts...> {
    static_assert(!func_extra_info<Ts...>::has_policy,
                  "return value policy can only be specified once!");
    static constexpr bool has_policy = true;
    static constexpr rv_policy::value policy = Policy;
};

template <typename... Ts>
struct func_extra_info<rv_policy, Ts...> : func_extra_info<Ts...> {
    static_assert(sizeof...(Ts) == (size_t) -1,
                  "return value policy must be specified as an rv_policy tag "
                  "(e.g. nb::rv_policy::reference), not as a runtime rv_policy object");
};

template <typename... Cs, typename... Ts>
struct func_extra_info<call_guard<Cs...>, Ts...> : func_extra_info<Ts...> {
    static_assert(std::is_same_v<typename func_extra_info<Ts...>::call_guard, void>,
                  "call_guard<> can only be specified once!");
    using call_guard = nanobind::call_guard<Cs...>;
};

template <size_t Nurse, size_t Patient, typename... Ts>
struct func_extra_info<nanobind::keep_alive<Nurse, Patient>, Ts...> : func_extra_info<Ts...> {
    static constexpr bool pre_post_hooks = true;
};

template <typename Policy, typename... Ts>
struct func_extra_info<call_policy<Policy>, Ts...> : func_extra_info<Ts...> {
    static constexpr bool pre_post_hooks = true;
};

template <typename... Ts>
struct func_extra_info<lock_self, Ts...> : func_extra_info<Ts...> {
    static constexpr size_t nargs_locked = 1 + func_extra_info<Ts...>::nargs_locked;
};

NB_INLINE void process_precall(PyObject **, size_t, detail::cleanup_list *, void *) { }

template <size_t NArgs, typename Policy>
NB_INLINE void
process_precall(PyObject **args, std::integral_constant<size_t, NArgs> nargs,
                detail::cleanup_list *cleanup, call_policy<Policy> *) {
    Policy::precall(args, nargs, cleanup);
}

NB_INLINE void process_postcall(PyObject **, size_t, PyObject *, void *) { }

template <size_t NArgs, size_t Nurse, size_t Patient>
NB_INLINE void
process_postcall(PyObject **args, std::integral_constant<size_t, NArgs>,
                 PyObject *result, nanobind::keep_alive<Nurse, Patient> *) {
    static_assert(Nurse != Patient,
                  "keep_alive with the same argument as both nurse and patient "
                  "doesn't make sense");
    static_assert(Nurse <= NArgs && Patient <= NArgs,
                  "keep_alive template parameters must be in the range "
                  "[0, number of C++ function arguments]");
    keep_alive(Nurse   == 0 ? result : args[Nurse - 1],
               Patient == 0 ? result : args[Patient - 1]);
}

template <size_t NArgs, typename Policy>
NB_INLINE void
process_postcall(PyObject **args, std::integral_constant<size_t, NArgs> nargs,
                 PyObject *&result, call_policy<Policy> *) {
    // result_guard avoids leaking a reference to the return object
    // if postcall throws an exception
    object result_guard = steal(result);
    Policy::postcall(args, nargs, result);
    result_guard.release();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
