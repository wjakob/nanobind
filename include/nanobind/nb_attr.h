/*
    nanobind/nb_attr.h: Annotations for function and class declarations

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

enum cast_flags : uint32_t {
    // Enable implicit conversions
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

/// Per-argument cast flags of an argument without annotations
constexpr uint32_t arg_flags_default = cast_flags::convert;

NAMESPACE_END(detail)

struct scope {
    PyObject *value;
    NB_INLINE scope(handle value) : value(value.ptr()) {}
};

struct name {
    const char *value;
    NB_INLINE name(const char *value) : value(value) {}
};

template <uint32_t Flags, bool Locked> struct arg_v_t;

template <uint32_t Flags = detail::arg_flags_default, bool Locked = false>
struct arg_t {
    NB_INLINE constexpr explicit arg_t(const char *name = nullptr)
        : name_(name), signature_(nullptr) { }

    template <uint32_t F2, bool L2>
    NB_INLINE constexpr explicit arg_t(const arg_t<F2, L2> &base)
        : name_(base.name_), signature_(base.signature_) { }

    // operator= can be used to provide a default value. A 'None' default
    // (spelled nb::none() or nullptr) additionally marks the argument as
    // accepting 'None' (see 'cast_flags::accepts_none').
    template <typename T>
    NB_INLINE arg_v_t<Flags, Locked> operator=(T &&value) const;
    NB_INLINE auto operator=(::nanobind::none value) const;
    NB_INLINE auto operator=(std::nullptr_t) const;

    NB_INLINE constexpr auto noconvert() const {
        return arg_t<Flags & ~detail::cast_flags::convert, Locked>(*this);
    }
    NB_INLINE constexpr auto none() const {
        return arg_t<Flags | detail::cast_flags::accepts_none, Locked>(*this);
    }
    NB_INLINE constexpr auto lock() const { return arg_t<Flags, true>(*this); }
    NB_INLINE arg_t &sig(const char *value) {
        signature_ = value;
        return *this;
    }

    const char *name_, *signature_;
};

// Function argument descriptor with a default value
template <uint32_t Flags, bool Locked> struct arg_v_t : arg_t<Flags, Locked> {
    object value;

    template <uint32_t F2, bool L2>
    NB_INLINE arg_v_t(const arg_t<F2, L2> &base, object &&value)
        : arg_t<Flags, Locked>(base), value(std::move(value)) { }

  private:
    // Inherited mutators would slice off the default, and are not generally needed
    using arg_t<Flags, Locked>::noconvert;
    using arg_t<Flags, Locked>::none;
    using arg_t<Flags, Locked>::sig;
    using arg_t<Flags, Locked>::lock;
};

using arg = arg_t<>;
using arg_locked = arg_t<detail::arg_flags_default, true>;
using arg_v = arg_v_t<detail::arg_flags_default, false>;
using arg_locked_v = arg_v_t<detail::arg_flags_default, true>;

template <uint32_t Flags, bool Locked>
NB_INLINE auto arg_t<Flags, Locked>::operator=(::nanobind::none value) const {
    return arg_v_t<Flags | detail::cast_flags::accepts_none, Locked>(
        *this, std::move(value));
}
template <uint32_t Flags, bool Locked>
NB_INLINE auto arg_t<Flags, Locked>::operator=(std::nullptr_t) const {
    return operator=(::nanobind::none());
}

NAMESPACE_BEGIN(detail)

/// Compile-time properties of a function argument annotation
template <typename T> struct arg_traits {
    static constexpr bool is_arg = false, has_default = false, locked = false;
    static constexpr uint32_t flags = 0;
};
template <uint32_t F, bool L> struct arg_traits<arg_t<F, L>> {
    static constexpr bool is_arg = true, has_default = false, locked = L;
    static constexpr uint32_t flags = F;
};
template <uint32_t F, bool L> struct arg_traits<arg_v_t<F, L>> {
    static constexpr bool is_arg = true, has_default = true, locked = L;
    static constexpr uint32_t flags = F;
};

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
    /// Did the user specify a name for this function, or is it anonymous?
    has_name = (1 << 0),
    /// Did the user specify a scope in which this function should be installed?
    has_scope = (1 << 1),
    /// Did the user specify a docstring?
    has_doc = (1 << 2),
    /// Did the user specify nb::arg/arg_v annotations for all arguments?
    has_args = (1 << 3),
    /// Does the function signature contain an *args-style argument?
    has_var_args = (1 << 4),
    /// Does the function signature contain an *kwargs-style argument?
    has_var_kwargs = (1 << 5),
    /// Is this function a method of a class?
    is_method = (1 << 6),
    /// Is this function a method called __init__? (automatically generated)
    is_constructor = (1 << 7),
    /// Can this constructor be used to perform an implicit conversion?
    is_implicit = (1 << 8),
    /// Is this function an arithmetic operator?
    is_operator = (1 << 9),
    /// When the function is GCed, do we need to call func_data_init::free_capture?
    has_free = (1 << 10),
    /// Should the func_new() call return a new reference?
    return_ref = (1 << 11),
    /// Does this overload specify a custom function signature (for docstrings, typing)
    has_signature = (1 << 12),
    /// Does this function potentially modify the elements of the PyObject*[] array
    /// representing its arguments? (nb::keep_alive() or call_policy annotations)
    can_mutate_args = (1 << 13),
    /// Is this overload a copy constructor? The dispatcher then never
    /// raises the call-wide 'convert' flag: implicit conversion of the
    /// source argument would recurse infinitely
    is_copy_constructor = (1 << 14)
};

/// Describes a function argument binding
struct arg_data_init {
    /// Argument name (nullptr if unnamed)
    const char *name;

    /// Overrides the argument type in docstrings and stubs (or nullptr)
    const char *signature;

    /// Interned Python version of 'name', filled by the backend
    PyObject *name_py;

    /// Default argument value (or nullptr)
    PyObject *value;

    /// Argument-specific cast flags (see the 'cast_flags' enumeration)
    uint16_t flag;
};

/// Describes a function binding
struct func_data_init_base {
    // A small amount of space to capture data used by the function/closure
    void *capture[3];

    // Callback to clean up the 'capture' field
    void (*free_capture)(void *);

    /// Type-erased trampoline implementing the function call
    PyObject *(*impl)(void *, PyObject **, uint32_t, cleanup_list *);

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

    /// sizeof(func_data_init_base) at the extension's compile time; the
    /// argument array of func_data_init<N> begins at this offset
    uint16_t base_size;

    /// sizeof(arg_data_init) at the extension's compile time; the stride of
    /// the argument array
    uint16_t arg_stride;

    /// Function name
    const char *name;

    /// Docstring
    const char *doc;

    /// Scope (e.g. module) in which the function will be installed
    PyObject *scope;
};

/// Sized version of func_data_init_base
template<size_t Size> struct func_data_init : func_data_init_base {
    arg_data_init args[Size];
};

template<> struct func_data_init<0> : func_data_init_base {};

static_assert(sizeof(void *) != 8 || sizeof(arg_data_init) == 40,
              "frozen ABI layout of arg_data_init changed");
NB_FROZEN_OFF(arg_data_init, name, 0);
NB_FROZEN_OFF(arg_data_init, signature, 8);
NB_FROZEN_OFF(arg_data_init, name_py, 16);
NB_FROZEN_OFF(arg_data_init, value, 24);
NB_FROZEN_OFF(arg_data_init, flag, 32);

static_assert(sizeof(void *) != 8 || sizeof(func_data_init_base) == 96,
              "frozen ABI layout of func_data_init_base changed");
NB_FROZEN_OFF(func_data_init_base, capture, 0);
NB_FROZEN_OFF(func_data_init_base, free_capture, 24);
NB_FROZEN_OFF(func_data_init_base, impl, 32);
NB_FROZEN_OFF(func_data_init_base, descr, 40);
NB_FROZEN_OFF(func_data_init_base, descr_types, 48);
NB_FROZEN_OFF(func_data_init_base, flags, 56);
NB_FROZEN_OFF(func_data_init_base, nargs, 60);
NB_FROZEN_OFF(func_data_init_base, nargs_pos, 62);
NB_FROZEN_OFF(func_data_init_base, base_size, 64);
NB_FROZEN_OFF(func_data_init_base, arg_stride, 66);
NB_FROZEN_OFF(func_data_init_base, name, 72);
NB_FROZEN_OFF(func_data_init_base, doc, 80);
NB_FROZEN_OFF(func_data_init_base, scope, 88);


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

template <typename F, rv_policy::value V>
NB_INLINE void func_extra_apply(F &, rv_policy::policy_tag<V>, size_t &) {
    // Consumed at compile time via func_extra_info<...>::policy
}

template <typename F>
NB_INLINE void func_extra_apply(F &, rv_policy, size_t &) {
    static_assert(false_v<F>,
                  "nb::def() and friends require a compile-time return value "
                  "policy tag such as nb::rv_policy::move; a runtime "
                  "rv_policy value cannot be used as a function annotation.");
}

template <typename F>
NB_INLINE void func_extra_apply(F &, std::nullptr_t, size_t &) { }

// The two overloads below fill the runtime argument record. Cast flags and
// locking are handled statically in nb_func.h, which also initializes the
// record's flag field (see 'arg_flags_static').

template <typename F, uint32_t Flags, bool Locked>
NB_INLINE void func_extra_apply(F &f, const arg_t<Flags, Locked> &a,
                                size_t &index) {
    arg_data_init &ad = f.args[index++];
    ad.name = a.name_;
    ad.signature = a.signature_;
    ad.value = nullptr;
}

template <typename F, uint32_t Flags, bool Locked>
NB_INLINE void func_extra_apply(F &f, const arg_v_t<Flags, Locked> &a,
                                size_t &index) {
    arg_data_init &ad = f.args[index];
    func_extra_apply(f, (const arg_t<Flags, Locked> &) a, index);
    ad.value = a.value.ptr();
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
    : func_extra_info<Ts...> { };

// When several policy annotations are given, the last one takes precedence,
// matching the runtime traversal order in func_extra_apply().
template <rv_policy::value V, typename... Ts>
struct func_extra_info<rv_policy::policy_tag<V>, Ts...> : func_extra_info<Ts...> {
    static constexpr bool has_policy = true;
    static constexpr rv_policy::value policy =
        func_extra_info<Ts...>::has_policy ? func_extra_info<Ts...>::policy : V;
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

template <uint32_t F, typename... Ts>
struct func_extra_info<arg_t<F, true>, Ts...> : func_extra_info<Ts...> {
    static constexpr size_t nargs_locked = 1 + func_extra_info<Ts...>::nargs_locked;
};

template <uint32_t F, typename... Ts>
struct func_extra_info<arg_v_t<F, true>, Ts...> : func_extra_info<Ts...> {
    static constexpr size_t nargs_locked = 1 + func_extra_info<Ts...>::nargs_locked;
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
