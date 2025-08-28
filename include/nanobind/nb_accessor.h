/*
    nanobind/nb_accessor.h: Accessor helper class for .attr(), operator[]

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#define NB_DECL_ACCESSOR_OP_I(name)                                            \
    template <typename T> accessor& name(const api<T> &o);

#define NB_IMPL_ACCESSOR_OP_I(name, op)                                        \
    template <typename Impl> template <typename T>                             \
    accessor<Impl>& accessor<Impl>::name(const api<T> &o) {                    \
        PyObject *res = obj_op_2(ptr(), o.derived().ptr(), op);                \
        Impl::set(m_base, m_key, res);                                         \
        return *this;                                                          \
    }

template <typename Impl> class accessor : public api<accessor<Impl>> {
    template <typename T> friend void nanobind::del(accessor<T> &);
    template <typename T> friend void nanobind::del(accessor<T> &&);
public:
    static constexpr auto Name = const_name("object");

    template <typename Key>
    accessor(handle obj, Key &&key)
        : m_base(obj.ptr()), m_key(std::move(key)) { }
    accessor(const accessor &) = delete;
    accessor(accessor &&) = delete;
    ~accessor() {
        if constexpr (Impl::cache_dec_ref)
            Py_XDECREF(m_cache);
    }

    template <typename T> accessor& operator=(T &&value);

    template <typename T, enable_if_t<std::is_base_of_v<object, T>> = 0>
    operator T() const { return borrow<T>(ptr()); }
    NB_INLINE PyObject *ptr() const {
        Impl::get(m_base, m_key, &m_cache);
        return m_cache;
    }
    NB_INLINE handle base() const { return m_base; }
    NB_INLINE object key() const { return steal(Impl::key(m_key)); }

    NB_DECL_ACCESSOR_OP_I(operator+=)
    NB_DECL_ACCESSOR_OP_I(operator-=)
    NB_DECL_ACCESSOR_OP_I(operator*=)
    NB_DECL_ACCESSOR_OP_I(operator/=)
    NB_DECL_ACCESSOR_OP_I(operator%=)
    NB_DECL_ACCESSOR_OP_I(operator|=)
    NB_DECL_ACCESSOR_OP_I(operator&=)
    NB_DECL_ACCESSOR_OP_I(operator^=)
    NB_DECL_ACCESSOR_OP_I(operator<<=)
    NB_DECL_ACCESSOR_OP_I(operator>>=)

private:
    NB_INLINE void del () { Impl::del(m_base, m_key); }

private:
    PyObject *m_base;
    mutable PyObject *m_cache{nullptr};
    typename Impl::key_type m_key;
};

struct str_attr {
    static constexpr bool cache_dec_ref = true;
    using key_type = const char *;

    NB_INLINE static void get(PyObject *obj, const char *key, PyObject **cache) {
        detail::getattr_or_raise(obj, key, cache);
    }

    NB_INLINE static void set(PyObject *obj, const char *key, PyObject *v) {
        setattr(obj, key, v);
    }

    NB_INLINE static PyObject *key(const char *key) {
        return PyUnicode_InternFromString(key);
    }
};

struct obj_attr {
    static constexpr bool cache_dec_ref = true;
    using key_type = handle;

    NB_INLINE static void get(PyObject *obj, handle key, PyObject **cache) {
        detail::getattr_or_raise(obj, key.ptr(), cache);
    }

    NB_INLINE static void set(PyObject *obj, handle key, PyObject *v) {
        setattr(obj, key.ptr(), v);
    }

    NB_INLINE static PyObject *key(handle key) {
        Py_INCREF(key.ptr());
        return key.ptr();
    }
};

struct str_item {
    static constexpr bool cache_dec_ref = true;
    using key_type = const char *;

    NB_INLINE static void get(PyObject *obj, const char *key, PyObject **cache) {
        detail::getitem_or_raise(obj, key, cache);
    }

    NB_INLINE static void set(PyObject *obj, const char *key, PyObject *v) {
        setitem(obj, key, v);
    }

    NB_INLINE static void del(PyObject *obj, const char *key) {
        delitem(obj, key);
    }
};

struct obj_item {
    static constexpr bool cache_dec_ref = true;
    using key_type = handle;

    NB_INLINE static void get(PyObject *obj, handle key, PyObject **cache) {
        detail::getitem_or_raise(obj, key.ptr(), cache);
    }

    NB_INLINE static void set(PyObject *obj, handle key, PyObject *v) {
        setitem(obj, key.ptr(), v);
    }

    NB_INLINE static void del(PyObject *obj, handle key) {
        delitem(obj, key.ptr());
    }
};

struct num_item {
    static constexpr bool cache_dec_ref = true;
    using key_type = Py_ssize_t;

    NB_INLINE static void get(PyObject *obj, Py_ssize_t index, PyObject **cache) {
        detail::getitem_or_raise(obj, index, cache);
    }

    NB_INLINE static void set(PyObject *obj, Py_ssize_t index, PyObject *v) {
        setitem(obj, index, v);
    }

    NB_INLINE static void del(PyObject *obj, Py_ssize_t index) {
        delitem(obj, index);
    }
};

struct num_item_list {
    #if defined(Py_GIL_DISABLED)
          static constexpr bool cache_dec_ref = true;
    #else
          static constexpr bool cache_dec_ref = false;
    #endif

    using key_type = Py_ssize_t;

    NB_INLINE static void get(PyObject *obj, Py_ssize_t index, PyObject **cache) {
        #if defined(Py_GIL_DISABLED)
            *cache = PyList_GetItemRef(obj, index);
        #else
            *cache = NB_LIST_GET_ITEM(obj, index);
        #endif
    }

    NB_INLINE static void set(PyObject *obj, Py_ssize_t index, PyObject *v) {
#if defined(Py_LIMITED_API) || defined(NB_FREE_THREADED)
        Py_INCREF(v);
        PyList_SetItem(obj, index, v);
#else
        PyObject *old = NB_LIST_GET_ITEM(obj, index);
        Py_INCREF(v);
        NB_LIST_SET_ITEM(obj, index, v);
        Py_DECREF(old);
#endif
    }

    NB_INLINE static void del(PyObject *obj, Py_ssize_t index) {
        delitem(obj, index);
    }
};

struct num_item_tuple {
    static constexpr bool cache_dec_ref = false;
    using key_type = Py_ssize_t;

    NB_INLINE static void get(PyObject *obj, Py_ssize_t index, PyObject **cache) {
        *cache = NB_TUPLE_GET_ITEM(obj, index);
    }

    template <typename...Ts> static void set(Ts...) {
        static_assert(false_v<Ts...>, "tuples are immutable!");
    }
};

template <typename D> accessor<obj_attr> api<D>::attr(handle key) const {
    return { derived(), borrow(key) };
}

template <typename D> accessor<str_attr> api<D>::attr(const char *key) const {
    return { derived(), key };
}

template <typename D> accessor<str_attr> api<D>::doc() const {
    return { derived(), "__doc__" };
}

template <typename D> accessor<obj_item> api<D>::operator[](handle key) const {
    return { derived(), borrow(key) };
}

template <typename D> accessor<str_item> api<D>::operator[](const char *key) const {
    return { derived(), key };
}

template <typename D>
template <typename T, enable_if_t<std::is_arithmetic_v<T>>>
accessor<num_item> api<D>::operator[](T index) const {
    return { derived(), (Py_ssize_t) index };
}

NB_IMPL_ACCESSOR_OP_I(operator+=, PyNumber_InPlaceAdd)
NB_IMPL_ACCESSOR_OP_I(operator%=, PyNumber_InPlaceRemainder)
NB_IMPL_ACCESSOR_OP_I(operator-=, PyNumber_InPlaceSubtract)
NB_IMPL_ACCESSOR_OP_I(operator*=, PyNumber_InPlaceMultiply)
NB_IMPL_ACCESSOR_OP_I(operator/=, PyNumber_InPlaceTrueDivide)
NB_IMPL_ACCESSOR_OP_I(operator|=, PyNumber_InPlaceOr)
NB_IMPL_ACCESSOR_OP_I(operator&=, PyNumber_InPlaceAnd)
NB_IMPL_ACCESSOR_OP_I(operator^=, PyNumber_InPlaceXor)
NB_IMPL_ACCESSOR_OP_I(operator<<=,PyNumber_InPlaceLshift)
NB_IMPL_ACCESSOR_OP_I(operator>>=,PyNumber_InPlaceRshift)

NAMESPACE_END(detail)

template <typename T, detail::enable_if_t<std::is_arithmetic_v<T>>>
detail::accessor<detail::num_item_list> list::operator[](T index) const {
    return { derived(), (Py_ssize_t) index };
}

template <typename T, detail::enable_if_t<std::is_arithmetic_v<T>>>
detail::accessor<detail::num_item_tuple> tuple::operator[](T index) const {
    return { derived(), (Py_ssize_t) index };
}

template <typename... Args> str str::format(Args&&... args) {
    return steal<str>(
        derived().attr("format")((detail::forward_t<Args>) args...).release());
}

NAMESPACE_END(NB_NAMESPACE)
