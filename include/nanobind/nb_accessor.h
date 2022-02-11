NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename Policy> class accessor : public api<accessor<Policy>> {
public:
    template <typename Key>
    accessor(handle obj, Key &&key) : m_obj(obj), m_key(std::move(key)) { }
    accessor(const accessor &) = delete;
    accessor(accessor &&) = delete;

    template <typename T> accessor& operator=(T &&value);

    template <typename T, enable_if_t<std::is_base_of_v<object, T>> = 0>
    operator T() const { return get(); }
    NB_INLINE PyObject *ptr() const { return get().ptr(); }

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

    NB_INLINE static void get(handle obj, const char *key, object &out) {
        detail::getattr_maybe(obj.ptr(), key, &out.m_ptr);
    }

    NB_INLINE static void set(handle obj, const char *key, handle val) {
        setattr(obj.ptr(), key, val.ptr());
    }
};

struct obj_attr {
    using key_type = object;

    NB_INLINE static void get(handle obj, handle key, object &out) {
        detail::getattr_maybe(obj.ptr(), key.ptr(), &out.m_ptr);
    }

    NB_INLINE static void set(handle obj, handle key, handle val) {
        setattr(obj.ptr(), key.ptr(), val.ptr());
    }
};

struct str_item {
    using key_type = const char *;

    NB_INLINE static void get(handle obj, const char *key, object &out) {
        detail::getitem_maybe(obj.ptr(), key, &out.m_ptr);
    }

    NB_INLINE static void set(handle obj, const char *key, handle val) {
        setitem(obj.ptr(), key, val.ptr());
    }
};

struct obj_item {
    using key_type = object;

    NB_INLINE static void get(handle obj, handle key, object &out) {
        detail::getitem_maybe(obj.ptr(), key.ptr(), &out.m_ptr);
    }

    NB_INLINE static void set(handle obj, handle key, handle val) {
        setitem(obj.ptr(), key.ptr(), val.ptr());
    }
};

struct num_item {
    using key_type = Py_ssize_t;

    NB_INLINE static void get(handle obj, Py_ssize_t key, object &out) {
        detail::getitem_maybe(obj.ptr(), key, &out.m_ptr);
    }

    NB_INLINE static void set(handle obj, Py_ssize_t key, handle val) {
        setitem(obj.ptr(), key, val.ptr());
    }
};

template <typename D> accessor<obj_attr> api<D>::attr(handle key) const {
    return { derived(), borrow(key) };
}

template <typename D> accessor<str_attr> api<D>::attr(const char *key) const {
    return { derived(), key };
}

template <typename D> accessor<obj_item> api<D>::operator[](handle key) const {
    return { derived(), borrow(key) };
}

template <typename D> accessor<str_item> api<D>::operator[](const char *key) const {
    return { derived(), key };
}

template <typename D>
template <typename T, enable_if_t<std::is_arithmetic_v<T>>>
accessor<num_item> api<D>::operator[](T key) const {
    return { derived(), (Py_ssize_t) key };
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
