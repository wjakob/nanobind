NAMESPACE_BEGIN(NB_NAMESPACE)

/// Macro defining functions/constructors for nanobind::handle subclasses
#define NB_OBJECT(Name, Parent, Check)                                         \
public:                                                                        \
    Name(handle h, detail::borrow_t) : Parent(h, detail::borrow_t{}) {}        \
    Name(handle h, detail::steal_t) : Parent(h, detail::steal_t{}) {}          \
    static bool check_(handle h) { return ((bool) h) && Check(h.ptr()); }

/// Like NB_OBJECT but allow null-initialization
#define NB_OBJECT_DEFAULT(Name, Parent, Check)                                 \
    NB_OBJECT(Name, Parent, Check)                                             \
    Name() : Parent() {}

/// Helper macro to create detail::api comparison functions
#define NB_API_COMP(name, op)                                                  \
    template <typename T> bool name(const api<T> &o) const {                   \
        return detail::obj_comp(derived().ptr(), o.derived().ptr(), op);       \
    }

/// Helper macro to create detail::api unary operators
#define NB_API_OP_1(name, op)                                                  \
    auto name() const { return steal(detail::obj_op_1(derived().ptr(), op)); }

/// Helper macro to create detail::api binary operators
#define NB_API_OP_2(name, op)                                                  \
    template <typename T> auto name(const api<T> &o) const {                   \
        return steal(                                                          \
            detail::obj_op_2(derived().ptr(), o.derived().ptr(), op));         \
    }

// A few forward declarations
class object;
class handle;

template <typename T = object> T borrow(handle h);
template <typename T = object> T steal(handle h);

NAMESPACE_BEGIN(detail)

template <typename Policy> class accessor;
struct str_attr;
struct obj_attr;

struct borrow_t { };
struct steal_t { };
class object_t { };

// Standard operations provided by every nanobind object
template <typename Derived> class api : public object_t {
public:
    // item_accessor operator[](handle key) const;
    // item_accessor operator[](const char *key) const;

    accessor<obj_attr> attr(handle key) const;
    accessor<str_attr> attr(const char *key) const;

    bool is(const api& o) const { return derived().ptr() == o.derived().ptr(); }
    bool is_none() const  { return derived().ptr() == Py_None; }
    bool is_valid() const { return derived().ptr() != nullptr; }

    NB_API_COMP(equal,      Py_EQ)
    NB_API_COMP(not_equal,  Py_NE)
    NB_API_COMP(operator<,  Py_LT)
    NB_API_COMP(operator<=, Py_LE)
    NB_API_COMP(operator>,  Py_GT)
    NB_API_COMP(operator>=, Py_GE)
    NB_API_OP_1(operator-,  PyNumber_Negative)
    NB_API_OP_1(operator!,  PyNumber_Invert)
    NB_API_OP_2(operator+,  PyNumber_Add)
    NB_API_OP_2(operator+=, PyNumber_InPlaceAdd)
    NB_API_OP_2(operator-,  PyNumber_Subtract)
    NB_API_OP_2(operator-=, PyNumber_InPlaceSubtract)
    NB_API_OP_2(operator*,  PyNumber_Multiply)
    NB_API_OP_2(operator*=, PyNumber_InPlaceMultiply)
    NB_API_OP_2(operator/,  PyNumber_TrueDivide)
    NB_API_OP_2(operator/=, PyNumber_InPlaceTrueDivide)
    NB_API_OP_2(operator|,  PyNumber_Or)
    NB_API_OP_2(operator|=, PyNumber_InPlaceOr)
    NB_API_OP_2(operator&,  PyNumber_And)
    NB_API_OP_2(operator&=, PyNumber_InPlaceAnd)
    NB_API_OP_2(operator^,  PyNumber_Xor)
    NB_API_OP_2(operator^=, PyNumber_InPlaceXor)
    NB_API_OP_2(operator<<, PyNumber_Lshift)
    NB_API_OP_2(operator<<=,PyNumber_InPlaceLshift)
    NB_API_OP_2(operator>>, PyNumber_Rshift)
    NB_API_OP_2(operator>>=,PyNumber_InPlaceRshift)

private:
    const Derived &derived() const { return static_cast<const Derived &>(*this); }
};

NAMESPACE_END(detail)

class handle : public detail::api<handle> {
    friend class python_error;
    friend class detail::str_attr;
    friend class detail::obj_attr;
public:
    handle() = default;
    handle(const handle &) = default;
    handle(handle &&) noexcept = default;
    handle &operator=(const handle &) = default;
    handle &operator=(handle &&) noexcept = default;
    NB_INLINE handle(const PyObject *ptr) : m_ptr((PyObject *) ptr) { }

    const handle& inc_ref() const & noexcept { Py_XINCREF(m_ptr); return *this; }
    const handle& dec_ref() const & noexcept { Py_XDECREF(m_ptr); return *this; }

    operator bool() const { return m_ptr != nullptr; }
    PyObject *ptr() const { return m_ptr; }
    bool check_() { return m_ptr != nullptr; }

protected:
    PyObject *m_ptr = nullptr;
};

class object : public handle {
public:
    object() = default;
    object(const object &o) : handle(o) { inc_ref(); }
    object(object &&o) noexcept : handle(o) { o.m_ptr = nullptr; }
    ~object() { dec_ref(); }
    object(handle h, detail::borrow_t) : handle(h) { inc_ref(); }
    object(handle h, detail::steal_t) : handle(h) { }

    handle release() {
      handle temp(m_ptr);
      m_ptr = nullptr;
      return temp;
    }

    object& operator=(const object &o) {
        handle temp(m_ptr);
        o.inc_ref();
        m_ptr = o.m_ptr;
        temp.dec_ref();
        return *this;
    }

    object& operator=(object &&o) noexcept {
        handle temp(m_ptr);
        m_ptr = o.m_ptr;
        o.m_ptr = nullptr;
        temp.dec_ref();
        return *this;
    }
};

template <typename T> T borrow(handle h) {
    return { h, detail::borrow_t() };
}

template <typename T> T steal(handle h) {
    return { h, detail::steal_t() };
}

inline object getattr(handle obj, const char *key) {
    return steal(detail::getattr(obj.ptr(), key));
}

inline object getattr(handle obj, handle key) {
    return steal(detail::getattr(obj.ptr(), key.ptr()));
}

inline object getattr(handle obj, const char *key, handle def) noexcept {
    return steal(detail::getattr(obj.ptr(), key, def.ptr()));
}

inline object getattr(handle obj, handle key, handle value) noexcept {
    return steal(detail::getattr(obj.ptr(), key.ptr(), value.ptr()));
}

inline void setattr(handle obj, const char *key, handle value) {
    detail::setattr(obj.ptr(), key, value.ptr());
}

inline void setattr(handle obj, handle key, handle value) {
    detail::setattr(obj.ptr(), key.ptr(), value.ptr());
}

inline object none() { return borrow(Py_None); }

class module_ : public object {
public:
    NB_OBJECT(module_, object, PyModule_CheckExact);

    template <typename Func, typename... Extra>
    module_ &def(const char *name_, Func &&f, const Extra &...extra);
};

class capsule : public object {
    NB_OBJECT_DEFAULT(capsule, object, PyCapsule_CheckExact)

    capsule(const void *ptr, void (*free)(void *) = nullptr) {
        m_ptr = detail::capsule_new(ptr, free);
    }

    void *data() const { return PyCapsule_GetPointer(m_ptr, nullptr); }
};

class str : public object {
    NB_OBJECT_DEFAULT(str, object, PyUnicode_Check)

    explicit str(handle h)
        : object(detail::str_from_obj(h.ptr()), detail::steal_t{}) {}

    explicit str(const char *c)
        : object(detail::str_from_cstr(c), detail::steal_t{}) {}

    explicit str(const char *c, size_t n)
        : object(detail::str_from_cstr_and_size(c, n), detail::steal_t{}) {}

    const char *c_str() { return PyUnicode_AsUTF8AndSize(m_ptr, nullptr); }
};

class tuple : public object {
    NB_OBJECT_DEFAULT(tuple, object, PyTuple_Check)
};

class dict : public object {
    NB_OBJECT_DEFAULT(dict, object, PyDict_Check)
};

class list : public object {
    NB_OBJECT_DEFAULT(list, object, PyList_Check)
};

class args : public tuple {
    NB_OBJECT_DEFAULT(args, tuple, PyTuple_Check)
};

class kwargs : public dict {
    NB_OBJECT_DEFAULT(kwargs, dict, PyDict_Check)
};

template <typename T>
bool isinstance(const T &obj) {
    if constexpr (std::is_base_of_v<handle, T>)
        return T::check_(obj);
    else
        detail::raise("isinstance(): unsupported case!");
}

NAMESPACE_BEGIN(detail)

template <typename Policy> class accessor : public api<accessor<Policy>> {
public:
    template <typename Key>
    accessor(handle obj, Key &&key) : m_obj(obj), m_key(std::move(key)) { }
    accessor(const accessor &) = delete;
    accessor(accessor &&) = delete;

    template <typename T> accessor& operator=(T &&value) {
        Policy::set(m_obj, m_key, cast((detail::forward_t<T>) value));
        return *this;
    }

    template <typename T, enable_if_t<std::is_base_of_v<object, T>> = 0>
    operator T() const { return get(); }
    PyObject *ptr() const { return get().ptr(); }

private:
    object &get() const {
        Policy::get(m_obj, m_key, m_cache);
        return m_cache;
    }

private:
    handle m_obj;
    typename Policy::key_type m_key;
    mutable object m_cache;
};

struct str_attr {
    using key_type = const char *;

    static void get(handle obj, const char *key, object &out) {
        detail::getattr_maybe(obj.ptr(), key, &out.m_ptr);
    }

    static void set(handle obj, const char *key, handle val) {
        setattr(obj, key, val);
    }
};

struct obj_attr {
    using key_type = object;

    static void get(handle obj, handle key, object &out) {
        detail::getattr_maybe(obj.ptr(), key.ptr(), &out.m_ptr);
    }

    static void set(handle obj, handle key, handle val) {
        setattr(obj, key.ptr(), val);
    }
};

template <typename D> accessor<obj_attr> api<D>::attr(handle key) const {
    return { derived(), borrow(key) };
}

template <typename D> accessor<str_attr> api<D>::attr(const char *key) const {
    return { derived(), key };
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

#undef NB_OBJECT
#undef NB_OBJECT_DEFAULT
#undef NB_API_COMP
#undef NB_API_OP_1
#undef NB_API_OP_2
