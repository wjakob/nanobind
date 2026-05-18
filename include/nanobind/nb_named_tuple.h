/*
    nanobind/nb_named_tuple.h: bind C++ structs as Python NamedTuple classes

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.

    This header is opt-in: it is NOT pulled in by <nanobind/nanobind.h>. Users
    must include it explicitly when they want to expose C++ structs as Python
    NamedTuples.

    Typing information (field types, defaults, Optional, nesting) is recorded
    by stubgen via the sentinel attribute ``__nb_named_tuple__`` set on each
    registered class -- it does not live in runtime ``__annotations__``.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <functional>
#include <utility>
#include <vector>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// Sentinel attribute set on every Python class registered via
/// ``nanobind::named_tuple<T>``. Stubgen looks for this name (see Task 2 in
/// the spec) to identify NamedTuple-bound types reliably. Do not change the
/// spelling without updating the stubgen pass.
inline constexpr const char *named_tuple_sentinel_attr = "__nb_named_tuple__";

/// Per-type runtime state. Each ``T`` gets its own static instance through
/// template instantiation. ``cls`` is a strong (incref'd) handle to the
/// Python class produced by ``collections.namedtuple``.
template <typename T> struct named_tuple_registry {
    using reader_fn = std::function<handle(const T &, rv_policy, cleanup_list *)>;
    using writer_fn = std::function<bool(T &, handle, uint8_t, cleanup_list *)>;

    static handle &cls() {
        static handle value;
        return value;
    }
    static std::vector<reader_fn> &readers() {
        static std::vector<reader_fn> value;
        return value;
    }
    static std::vector<writer_fn> &writers() {
        static std::vector<writer_fn> value;
        return value;
    }
};

/// Generic type caster shared by every C++ struct exposed via
/// ``nanobind::named_tuple<T>``. Users opt a type ``T`` into this caster
/// with the ``NB_NAMED_TUPLE_CASTER(T)`` macro (or via the
/// ``NB_NAMED_TUPLE`` convenience macro which expands to it).
template <typename T> struct named_tuple_caster {
    NB_TYPE_CASTER(T, const_name("tuple"))

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        auto &writers = named_tuple_registry<T>::writers();
        size_t n = writers.size();
        if (!named_tuple_registry<T>::cls().is_valid())
            return false;

        PyObject *temp;
        PyObject **items = seq_get_with_size(src.ptr(), n, &temp);
        if (!items)
            return false;

        bool ok = true;
        for (size_t i = 0; i < n; ++i) {
            if (!writers[i](value, handle(items[i]), flags, cleanup)) {
                ok = false;
                break;
            }
        }
        Py_XDECREF(temp);
        return ok;
    }

    template <typename T_>
    static handle from_cpp(T_ &&src, rv_policy policy, cleanup_list *cleanup) noexcept {
        auto &readers = named_tuple_registry<T>::readers();
        handle cls = named_tuple_registry<T>::cls();
        if (!cls.is_valid()) {
            PyErr_SetString(PyExc_RuntimeError,
                "nanobind: NamedTuple type was used before its module-level "
                "registration ran. Make sure the binding code is reached "
                "before any C++ -> Python conversion.");
            return handle();
        }

        size_t n = readers.size();
        object args = steal(PyTuple_New((Py_ssize_t) n));
        if (!args.is_valid())
            return handle();

        const T &ref = static_cast<const T &>(src);
        for (size_t i = 0; i < n; ++i) {
            object item = steal(readers[i](ref, policy, cleanup));
            if (!item.is_valid())
                return handle();
            NB_TUPLE_SET_ITEM(args.ptr(), (Py_ssize_t) i, item.release().ptr());
        }

        return PyObject_Call(cls.ptr(), args.ptr(), nullptr);
    }
};

NAMESPACE_END(detail)

/// Helper class that registers a C++ struct ``T`` as a Python NamedTuple.
/// Use the ``NB_NAMED_TUPLE`` macro for the common case; instantiate this
/// class directly when you need custom field names, computed properties,
/// or other escape-hatch behaviour.
template <typename T> class named_tuple {
public:
    using reader_fn = typename detail::named_tuple_registry<T>::reader_fn;
    using writer_fn = typename detail::named_tuple_registry<T>::writer_fn;

    named_tuple(handle scope, const char *name)
        : scope_(scope), name_(name) { }

    named_tuple(const named_tuple &) = delete;
    named_tuple &operator=(const named_tuple &) = delete;
    named_tuple(named_tuple &&) = delete;
    named_tuple &operator=(named_tuple &&) = delete;

    /// Register a field. The field name appears in the Python NamedTuple's
    /// ``_fields`` tuple and becomes an attribute on each instance.
    template <typename F> named_tuple &def_rw(const char *name, F T::*member) {
        field_names_.push_back(name);
        readers_.emplace_back(
            [member](const T &v, rv_policy policy, detail::cleanup_list *cl) -> handle {
                return detail::make_caster<F>::from_cpp(v.*member, policy, cl);
            });
        writers_.emplace_back(
            [member](T &v, handle h, uint8_t flags, detail::cleanup_list *cl) -> bool {
                detail::make_caster<F> c;
                if (!c.from_python(h, detail::flags_for_local_caster<F>(flags), cl))
                    return false;
                v.*member = c.operator detail::cast_t<F>();
                return true;
            });
        return *this;
    }

    ~named_tuple() {
        if (!built_)
            finalize();
    }

private:
    void finalize() {
        built_ = true;

        object collections = module_::import_("collections");
        object factory = collections.attr("namedtuple");

        list field_list;
        for (const char *fn : field_names_)
            field_list.append(str(fn));

        object cls = factory(str(name_), field_list);
        cls.attr(detail::named_tuple_sentinel_attr) = bool_(true);

        // Register into the per-T runtime registry. The class is owned by
        // ``scope_`` (set as an attribute below) and the registry holds an
        // extra strong reference so it stays alive for the program lifetime.
        scope_.attr(name_) = cls;

        cls.inc_ref();
        detail::named_tuple_registry<T>::cls() = handle(cls);
        detail::named_tuple_registry<T>::readers() = std::move(readers_);
        detail::named_tuple_registry<T>::writers() = std::move(writers_);
    }

    handle scope_;
    const char *name_;
    std::vector<const char *> field_names_;
    std::vector<reader_fn> readers_;
    std::vector<writer_fn> writers_;
    bool built_ = false;
};

NAMESPACE_END(NB_NAMESPACE)

// ---------------------------------------------------------------------------
// File-scope macro: opt a C++ struct into the named-tuple type caster.
// ---------------------------------------------------------------------------
#define NB_NAMED_TUPLE_CASTER(Type)                                            \
    namespace nanobind { namespace detail {                                    \
    template <> struct type_caster<Type> : named_tuple_caster<Type> { };       \
    } }

// ---------------------------------------------------------------------------
// In-module macro: declare and finalize a NamedTuple binding in one call.
// Expands to ``nanobind::named_tuple<Type>(scope, "Type").def_rw(...)...``.
// Supports up to 16 fields; use the helper API directly for more.
// ---------------------------------------------------------------------------

#define NB_NT_EXPAND(x) x
#define NB_NT_CAT_(a, b) a##b
#define NB_NT_CAT(a, b) NB_NT_CAT_(a, b)

#define NB_NT_NARG_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13,    \
                    _14, _15, _16, N, ...) N
#define NB_NT_NARG(...) NB_NT_EXPAND(NB_NT_NARG_(                              \
    __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

#define NB_NT_F1(T, x)        .def_rw(#x, &T::x)
#define NB_NT_F2(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F1(T, __VA_ARGS__))
#define NB_NT_F3(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F2(T, __VA_ARGS__))
#define NB_NT_F4(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F3(T, __VA_ARGS__))
#define NB_NT_F5(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F4(T, __VA_ARGS__))
#define NB_NT_F6(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F5(T, __VA_ARGS__))
#define NB_NT_F7(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F6(T, __VA_ARGS__))
#define NB_NT_F8(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F7(T, __VA_ARGS__))
#define NB_NT_F9(T, x, ...)   .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F8(T, __VA_ARGS__))
#define NB_NT_F10(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F9(T, __VA_ARGS__))
#define NB_NT_F11(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F10(T, __VA_ARGS__))
#define NB_NT_F12(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F11(T, __VA_ARGS__))
#define NB_NT_F13(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F12(T, __VA_ARGS__))
#define NB_NT_F14(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F13(T, __VA_ARGS__))
#define NB_NT_F15(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F14(T, __VA_ARGS__))
#define NB_NT_F16(T, x, ...)  .def_rw(#x, &T::x) NB_NT_EXPAND(NB_NT_F15(T, __VA_ARGS__))

#define NB_NT_DEFS(T, ...)                                                     \
    NB_NT_EXPAND(NB_NT_CAT(NB_NT_F, NB_NT_NARG(__VA_ARGS__))(T, __VA_ARGS__))

#define NB_NAMED_TUPLE(scope, Type, ...)                                       \
    ::nanobind::named_tuple<Type>((scope), #Type) NB_NT_DEFS(Type, __VA_ARGS__)
