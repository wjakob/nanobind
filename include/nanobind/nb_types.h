NAMESPACE_BEGIN(nanobind)

namespace detail {
    struct borrow_t { };
    struct steal_t { };
};

class handle {
public:
    handle() = default;
    handle(const handle &) = default;
    handle(handle &&) noexcept = default;
    handle &operator=(const handle &) = default;
    handle &operator=(handle &&) noexcept = default;
    handle(PyObject *ptr) : m_ptr(ptr) { }

    const handle& inc_ref() const & { Py_XINCREF(m_ptr); return *this; }
    const handle& dec_ref() const & { Py_XDECREF(m_ptr); return *this; }
    bool check() const { return m_ptr != nullptr; }
    PyObject *ptr() const { return m_ptr; }
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

template <typename T> T reinterpret_borrow(handle h) {
    return { h, detail::borrow_t() };
}

template <typename T> T reinterpret_steal(handle h) {
    return { h, detail::steal_t() };
}

inline object getattr(handle obj, const char *name, handle default_) {
    if (PyObject *result = PyObject_GetAttrString(obj.ptr(), name))
        return reinterpret_steal<object>(result);
    PyErr_Clear();
    return reinterpret_borrow<object>(default_);
}

inline object none() {
    return reinterpret_borrow<object>(Py_None);
}

class module_ : public object {
public:
    using object::object;
    using object::operator=;

    template <typename Func, typename... Extra>
    module_ &def(const char *name_, Func &&f, const Extra &...extra);
};

class capsule : public object {
public:
    using object::object;
    using object::operator=;

    capsule(const void *ptr, void (*free)(void *)) {
        m_ptr = detail::capsule_new(ptr, free);
    }
};

using module = module_;

NAMESPACE_END(nanobind)
