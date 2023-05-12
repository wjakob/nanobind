/*
    nanobind/stl/shared_ptr.h: Type caster for std::shared_ptr<T>

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <memory>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// shared_ptr deleter that reduces the reference count of a Python object
struct py_deleter {
    void operator()(void *) noexcept {
        // Don't run the deleter if the interpreter has been shut down
        if (!Py_IsInitialized())
            return;
        gil_scoped_acquire guard;
        Py_DECREF(o);
    }

    PyObject *o;
};

/**
 * Create a std::shared_ptr for `ptr` that owns a reference to the Python
 * object `h`; if `ptr` is non-null, then the refcount of `h` is incremented
 * before creating the shared_ptr and decremented by its deleter.
 *
 * Usually this is instantiated with T = void, to reduce template bloat.
 * But if the pointee type uses enable_shared_from_this, we instantiate
 * with T = that type, in order to allow its internal weak_ptr to share
 * ownership with the shared_ptr we're creating.
 *
 * The next two functions are simultaneously marked as 'inline' (to avoid
 * linker errors) and 'NB_NOINLINE' (to avoid them being inlined into every
 * single shared_ptr type_caster, which would enlarge the binding size)
 */
template <typename T>
inline NB_NOINLINE std::shared_ptr<T>
shared_from_python(T *ptr, handle h) noexcept {
    if (ptr)
        return std::shared_ptr<T>(ptr, py_deleter{ h.inc_ref().ptr() });
    else
        return std::shared_ptr<T>(nullptr);
}

inline NB_NOINLINE void shared_from_cpp(std::shared_ptr<void> &&ptr,
                                        PyObject *o) noexcept {
    keep_alive(o, new std::shared_ptr<void>(std::move(ptr)),
               [](void *p) noexcept { delete (std::shared_ptr<void> *) p; });
}

template <typename T> struct type_caster<std::shared_ptr<T>> {
    using Value = std::shared_ptr<T>;
    using Caster = make_caster<T>;
    static_assert(Caster::IsClass,
                  "Binding 'shared_ptr<T>' requires that 'T' can also be bound "
                  "by nanobind. It appears that you specified a type which "
                  "would undergo conversion/copying, which is not allowed.");

    static constexpr auto Name = Caster::Name;
    static constexpr bool IsClass = true;

    template <typename T_> using Cast = movable_cast_t<T_>;

    Value value;

    bool from_python(handle src, uint8_t flags,
                     cleanup_list *cleanup) noexcept {
        Caster caster;
        if (!caster.from_python(src, flags, cleanup))
            return false;

        T *ptr = caster.operator T *();
        if constexpr (has_shared_from_this_v<T>) {
            if (ptr) {
                if (auto sp = ptr->weak_from_this().lock()) {
                    // There is already a C++ shared_ptr for this object. Use it.
                    value = std::static_pointer_cast<T>(std::move(sp));
                    return true;
                }
            }
            // Otherwise create a new one. Use shared_from_python<T>(...)
            // so that future calls to ptr->shared_from_this() can share
            // ownership with it.
            value = shared_from_python(ptr, src);
        } else {
            value = std::static_pointer_cast<T>(
                shared_from_python(static_cast<void *>(ptr), src));
        }
        return true;
    }

    static handle from_cpp(const Value *value, rv_policy policy,
                           cleanup_list *cleanup) noexcept {
        if (!value)
            return (PyObject *) nullptr;
        return from_cpp(*value, policy, cleanup);
    }

    static handle from_cpp(const Value &value, rv_policy,
                           cleanup_list *cleanup) noexcept {
        bool is_new = false;
        handle result;

        T *ptr = value.get();
        const std::type_info *type = &typeid(T);

        constexpr bool has_type_hook =
            !std::is_base_of_v<std::false_type, type_hook<T>>;
        if constexpr (has_type_hook)
            type = type_hook<T>::get(ptr);

        if constexpr (!std::is_polymorphic_v<T>) {
            result = nb_type_put(type, ptr, rv_policy::reference,
                                 cleanup, &is_new);
        } else {
            const std::type_info *type_p =
                (!has_type_hook && ptr) ? &typeid(*ptr) : nullptr;

            result = nb_type_put_p(type, type_p, ptr, rv_policy::reference,
                                   cleanup, &is_new);
        }

        if (is_new)
            shared_from_cpp(std::static_pointer_cast<void>(value),
                            result.ptr());

        return result;
    }

    explicit operator Value *() { return &value; }
    explicit operator Value &() { return value; }
    explicit operator Value &&() && { return (Value &&) value; }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
