/*
    nanobind/nb_types.h: nb::dict/str/list/..: C++ wrappers for Python types

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

/// Macro defining functions/constructors for nanobind::handle subclasses
#define NB_OBJECT(Type, Parent, Check)                                         \
public:                                                                        \
    static constexpr auto Name = detail::const_name(#Type);                    \
    NB_INLINE Type(handle h, detail::borrow_t)                                 \
        : Parent(h, detail::borrow_t{}) {}                                     \
    NB_INLINE Type(handle h, detail::steal_t)                                  \
        : Parent(h, detail::steal_t{}) {}                                      \
    NB_INLINE static bool check_(handle h) {                                   \
        return Check(h.ptr());                                                 \
    }

/// Like NB_OBJECT but allow null-initialization
#define NB_OBJECT_DEFAULT(Type, Parent, Check)                                 \
    NB_OBJECT(Type, Parent, Check)                                             \
    Type() : Parent() {}

/// Helper macro to create detail::api comparison functions
#define NB_API_COMP(name, op)                                                  \
    template <typename T> NB_INLINE bool name(const api<T> &o) const {         \
        return detail::obj_comp(derived().ptr(), o.derived().ptr(), op);       \
    }

/// Helper macro to create detail::api unary operators
#define NB_API_OP_1(name, op)                                                  \
    NB_INLINE auto name() const {                                              \
        return steal(detail::obj_op_1(derived().ptr(), op));                   \
    }

/// Helper macro to create detail::api binary operators
#define NB_API_OP_2(name, op)                                                  \
    template <typename T> NB_INLINE auto name(const api<T> &o) const {         \
        return steal(                                                          \
            detail::obj_op_2(derived().ptr(), o.derived().ptr(), op));         \
    }

// A few forward declarations
class object;
class handle;
class iterator;

template <typename T = object> T borrow(handle h);
template <typename T = object> T steal(handle h);

NAMESPACE_BEGIN(detail)

template <typename Impl> class accessor;
struct str_attr; struct obj_attr;
struct str_item; struct obj_item; struct num_item;
struct num_item_list; struct num_item_tuple;
class args_proxy; class kwargs_proxy;
struct borrow_t { };
struct steal_t { };
class api_tag { };

// Standard operations provided by every nanobind object
template <typename Derived> class api : public api_tag {
public:
    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

    NB_INLINE bool is(const api& o) const { return derived().ptr() == o.derived().ptr(); }
    NB_INLINE bool is_none() const  { return derived().ptr() == Py_None; }
    NB_INLINE bool is_valid() const { return derived().ptr() != nullptr; }
    NB_INLINE handle inc_ref() const & noexcept;
    NB_INLINE handle dec_ref() const & noexcept;
    iterator begin() const;
    iterator end() const;

    NB_INLINE handle type() const;
    NB_INLINE operator handle() const;

    accessor<obj_attr> attr(handle key) const;
    accessor<str_attr> attr(const char *key) const;

    accessor<obj_item> operator[](handle key) const;
    accessor<str_item> operator[](const char *key) const;
    template <typename T, enable_if_t<std::is_arithmetic_v<T>> = 1>
    accessor<num_item> operator[](T key) const;
    args_proxy operator*() const;

    template <rv_policy policy = rv_policy::automatic_reference,
              typename... Args>
    object operator()(Args &&...args) const;

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
};

NAMESPACE_END(detail)

class handle : public detail::api<handle> {
    friend class python_error;
    friend struct detail::str_attr;
    friend struct detail::obj_attr;
    friend struct detail::str_item;
    friend struct detail::obj_item;
    friend struct detail::num_item;
public:
    static constexpr auto Name = detail::const_name("handle");

    handle() = default;
    handle(const handle &) = default;
    handle(handle &&) noexcept = default;
    handle &operator=(const handle &) = default;
    handle &operator=(handle &&) noexcept = default;
    NB_INLINE handle(const PyObject *ptr) : m_ptr((PyObject *) ptr) { }
    NB_INLINE handle(const PyTypeObject *ptr) : m_ptr((PyObject *) ptr) { }

    const handle& inc_ref() const & noexcept { Py_XINCREF(m_ptr); return *this; }
    const handle& dec_ref() const & noexcept { Py_XDECREF(m_ptr); return *this; }

    NB_INLINE operator bool() const { return m_ptr != nullptr; }
    NB_INLINE PyObject *ptr() const { return m_ptr; }
    NB_INLINE static bool check_(handle) { return true; }

protected:
    PyObject *m_ptr = nullptr;
};

class object : public handle {
public:
    static constexpr auto Name = detail::const_name("object");

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

template <typename T> NB_INLINE T borrow(handle h) {
    return { h, detail::borrow_t() };
}

template <typename T> NB_INLINE T steal(handle h) {
    return { h, detail::steal_t() };
}

inline bool hasattr(handle obj, const char *key) noexcept {
    return PyObject_HasAttrString(obj.ptr(), key);
}

inline bool hasattr(handle obj, handle key) noexcept {
    return PyObject_HasAttr(obj.ptr(), key.ptr());
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

class module_ : public object {
public:
    NB_OBJECT(module_, object, PyModule_CheckExact);

    template <typename Func, typename... Extra>
    module_ &def(const char *name_, Func &&f, const Extra &...extra);

    /// Import and return a module or throws `python_error`.
    static NB_INLINE module_ import_(const char *name) {
        return steal<module_>(detail::module_import(name));
    }

    /// Import and return a module or throws `python_error`.
    NB_INLINE module_ def_submodule(const char *name,
                                    const char *doc = nullptr) {
        return borrow<module_>(detail::module_new_submodule(m_ptr, name, doc));
    }
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
        : object(detail::str_from_obj(h.ptr()), detail::steal_t{}) { }

    explicit str(const char *c)
        : object(detail::str_from_cstr(c), detail::steal_t{}) { }

    explicit str(const char *c, size_t n)
        : object(detail::str_from_cstr_and_size(c, n), detail::steal_t{}) { }

    const char *c_str() { return PyUnicode_AsUTF8AndSize(m_ptr, nullptr); }
};

class tuple : public object {
    NB_OBJECT_DEFAULT(tuple, object, PyTuple_Check)
    size_t size() const { return PyTuple_GET_SIZE(m_ptr); }
    template <typename T, detail::enable_if_t<std::is_arithmetic_v<T>> = 1>
    detail::accessor<detail::num_item_tuple> operator[](T key) const;
};

class list : public object {
    NB_OBJECT(list, object, PyList_Check)
    list() : object(PyList_New(0), detail::steal_t()) { }
    size_t size() const { return PyList_GET_SIZE(m_ptr); }

    template <typename T> void append(T &&value);

    template <typename T, detail::enable_if_t<std::is_arithmetic_v<T>> = 1>
    detail::accessor<detail::num_item_list> operator[](T key) const;
};

class dict : public object {
    NB_OBJECT(dict, object, PyDict_Check)
    dict() : object(PyDict_New(), detail::steal_t()) { }
    size_t size() const { return PyDict_GET_SIZE(m_ptr); }
};

class sequence : public object {
    NB_OBJECT_DEFAULT(sequence, object, PySequence_Check)
};

class args : public tuple {
    NB_OBJECT_DEFAULT(args, tuple, PyTuple_Check)
};

class kwargs : public dict {
    NB_OBJECT_DEFAULT(kwargs, dict, PyDict_Check)
};

class iterator : public object {
public:
    using difference_type = Py_ssize_t;
    using value_type = handle;
    using reference = const handle;
    using pointer = const handle *;

    NB_OBJECT_DEFAULT(iterator, object, PyIter_Check)

    iterator& operator++() {
        m_value = steal(detail::obj_iter_next(m_ptr));
        return *this;
    }

    iterator operator++(int) {
        iterator rv = *this;
        m_value = steal(detail::obj_iter_next(m_ptr));
        return rv;
    }

    handle operator*() const {
        if (is_valid() & !m_value.is_valid())
            m_value = steal(detail::obj_iter_next(m_ptr));
        return m_value;
    }

    pointer operator->() const { operator*(); return &m_value; }

    static iterator sentinel() { return {}; }

    friend bool operator==(const iterator &a, const iterator &b) { return a->ptr() == b->ptr(); }
    friend bool operator!=(const iterator &a, const iterator &b) { return a->ptr() != b->ptr(); }

private:
    mutable object m_value;
};


template <typename T>
NB_INLINE bool isinstance(handle obj) noexcept {
    if constexpr (std::is_base_of_v<handle, T>)
        return T::check_(obj);
    else
        return detail::nb_type_isinstance(obj.ptr(), &typeid(detail::intrinsic_t<T>));
}

NB_INLINE str repr(handle h) { return steal<str>(detail::obj_repr(h.ptr())); }
NB_INLINE size_t len(handle h) { return detail::obj_len(h.ptr()); }
NB_INLINE size_t len(const tuple &t) { return PyTuple_GET_SIZE(t.ptr()); }
NB_INLINE size_t len(const list &t) { return PyList_GET_SIZE(t.ptr()); }
NB_INLINE size_t len(const dict &t) { return PyDict_GET_SIZE(t.ptr()); }

inline void print(handle value, handle end = handle(), handle file = handle()) {
    detail::print(value.ptr(), end.ptr(), file.ptr());
}

inline void print(const char *str, handle end = handle(), handle file = handle()) {
    print(nanobind::str(str), end, file);
}

/// Check if it's safe to issue to issue Python operations (GIL held, python not finalizing)
inline bool ready() {
    return PyGILState_Check() && !_Py_IsFinalizing();
}

/// Retrieve the Python type object associated with a C++ class
template <typename T> handle type() {
    return detail::nb_type_lookup(&typeid(detail::intrinsic_t<T>));
}

inline object none() { return borrow(Py_None); }
inline dict builtins() { return borrow<dict>(PyEval_GetBuiltins()); }

inline iterator iter(handle h) {
    return steal<iterator>(detail::obj_iter(h.ptr()));
}

NAMESPACE_BEGIN(detail)
template <typename Derived> NB_INLINE api<Derived>::operator handle() const {
    return derived().ptr();
}

template <typename Derived> NB_INLINE handle api<Derived>::type() const {
    return (PyObject *) Py_TYPE(derived().ptr());
}

template <typename Derived>
NB_INLINE handle api<Derived>::inc_ref() const &noexcept {
    return operator handle().inc_ref();
}

template <typename Derived> iterator api<Derived>::begin() const {
    return iter(*this);
}

template <typename Derived> iterator api<Derived>::end() const {
    return iterator::sentinel();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

#undef NB_API_COMP
#undef NB_API_OP_1
#undef NB_API_OP_2
