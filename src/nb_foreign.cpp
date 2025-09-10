/*
    src/nb_foreign.cpp: libnanobind functionality for interfacing with other
                        binding libraries

    Copyright (c) 2025 Hudson River Trading LLC <opensource@hudson-trading.com>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#if !defined(NB_DISABLE_INTEROP)

#include "nb_internals.h"
#include "nb_ft.h"
#include "nb_abi.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// nanobind exception translator that wraps a foreign one
static void foreign_exception_translator(const std::exception_ptr &p,
                                         void *payload) {
    std::exception_ptr e = p;
    int translated = ((pymb_framework *) payload)->translate_exception(&e);
    if (!translated)
        std::rethrow_exception(e);
}

// When learning about a new foreign type, should we automatically use it?
NB_INLINE bool should_autoimport_foreign(nb_internals *internals_,
                                         pymb_binding *binding) {
    return internals_->foreign_import_all &&
           binding->framework->abi_lang == pymb_abi_lang_cpp &&
           binding->framework->abi_extra == internals_->foreign_self->abi_extra;
}

static void nb_type_import_binding(pymb_binding *binding,
                                   const std::type_info *cpptype) noexcept;

// Callback functions for other frameworks to operate on our objects
// or tell us about theirs

static void *nb_foreign_from_python(pymb_binding *binding,
                                    PyObject *pyobj,
                                    uint8_t convert,
                                    void (*keep_referenced)(void *ctx,
                                                            PyObject *obj),
                                    void *keep_referenced_ctx) noexcept {
    cleanup_list cleanup{nullptr};
    auto *td = (type_data *) binding->context;
    if (td->align == 0) { // enum
        int64_t value;
        if (keep_referenced &&
            enum_from_python(td->type, pyobj, &value, td->size,
                             convert ? uint8_t(cast_flags::convert) : 0,
                             nullptr)) {
            bytes holder{(uint8_t *) &value + NB_BIG_ENDIAN * (8 - td->size),
                         td->size};
            keep_referenced(keep_referenced_ctx, holder.ptr());
            return (void *) holder.data();
        }
        return nullptr;
    }

    void *result = nullptr;
    bool ok = nb_type_get(td->type, pyobj,
                          convert ? uint8_t(cast_flags::convert) : 0,
                          keep_referenced ? &cleanup : nullptr, &result);
    if (keep_referenced) {
        // Move temporary references from our `cleanup_list` to our caller's
        // equivalent.
        for (uint32_t idx = 1; idx < cleanup.size(); ++idx)
            keep_referenced(keep_referenced_ctx, cleanup[idx]);
        if (cleanup.size() > 1)
            cleanup.release();
    }
    return ok ? result : nullptr;
}

static PyObject *nb_foreign_to_python(pymb_binding *binding,
                                      void *cobj,
                                      enum pymb_rv_policy rvp_,
                                      PyObject *parent) noexcept {
    cleanup_list cleanup{parent};
    auto *td = (type_data *) binding->context;
    if (td->align == 0) { // enum
        int64_t key;
        switch (td->size) {
            case 1: key = *(uint8_t *) cobj; break;
            case 2: key = *(uint16_t *) cobj; break;
            case 4: key = *(uint32_t *) cobj; break;
            case 8: key = *(uint64_t *) cobj; break;
            default: return nullptr;
        }
        if (rvp_ == pymb_rv_policy_take_ownership)
            ::operator delete(cobj);
        if ((td->flags & (uint32_t) enum_flags::is_signed) && td->size < 8) {
            // sign extend
            key <<= (64 - (td->size * 8));
            key >>= (64 - (td->size * 8));
        }
        return enum_from_cpp(td->type, key, td->size);
    }

    rv_policy rvp = (rv_policy) rvp_;
    if (rvp < rv_policy::take_ownership || rvp > rv_policy::none) {
        // Future-proofing in case additional pymb_rv_policies are defined
        // later: if we don't recognize this policy, then refuse the cast
        // unless a pyobject wrapper already exists.
        rvp = rv_policy::none;
    }
    return nb_type_put(td->type, cobj, rvp, &cleanup, nullptr);
}

static int nb_foreign_keep_alive(PyObject *nurse,
                                 void *payload,
                                 void (*cb)(void*)) noexcept {
    try {
        if (cb)
            keep_alive(nurse, payload, (void (*)(void*) noexcept) cb);
        else
            keep_alive(nurse, (PyObject *) payload);
        return 0;
    } catch (const std::runtime_error& err) {
        PyErr_SetString(PyExc_RuntimeError, err.what());
        return -1;
    }
}

static int nb_foreign_translate_exception(void *eptr) noexcept {
    std::exception_ptr &e = *(std::exception_ptr *) eptr;

    // Skip the default translator (at the end of the list). It translates
    // generic STL exceptions which other frameworks might want to translate
    // differently than we do; they should get control over the behavior of
    // their functions.
    for (nb_translator_seq* cur = internals->translators.load_acquire();
         cur->next.load_relaxed(); cur = cur->next.load_acquire()) {
        if (cur->translator == internals->foreign_exception_translator) {
            // Don't call foreign translators, to avoid mutual recursion.
            // They are at the end of the list, just before the default
            // translator, so we can stop iterating when we see one.
            break;
        }
        try {
            cur->translator(e, cur->payload);
            return 1;
        } catch (...) { e = std::current_exception(); }
    }

    // Check nb::python_error and nb::builtin_exception
    try {
        std::rethrow_exception(e);
    } catch (python_error &e) {
        e.restore();
    } catch (builtin_exception &e) {
        if (!set_builtin_exception_status(e))
            PyErr_SetString(PyExc_SystemError, "foreign function threw "
                            "nanobind::next_overload()");
    } catch (...) { e = std::current_exception(); }
    return 0;
}

static void nb_foreign_add_foreign_binding(pymb_binding *binding) noexcept {
    nb_internals *internals_ = internals;
    lock_internals guard{internals_};
    if (should_autoimport_foreign(internals_, binding))
        nb_type_import_binding(binding,
                               (const std::type_info *) binding->native_type);
}

static void nb_foreign_remove_foreign_binding(pymb_binding *binding) noexcept {
    nb_internals *internals_ = internals;
    lock_internals guard{internals_};

    auto remove_from_list = [binding](void *list_head,
                                      nb_foreign_seq **to_free) -> void* {
        if (!nb_is_seq(list_head))
            return list_head == binding ? nullptr : list_head;
        nb_foreign_seq *current = nb_get_seq<pymb_binding>(list_head);
        nb_foreign_seq *prev = nullptr;
        while (current && current->value != binding) {
            prev = current;
            current = nb_load_acquire(current->next);
        }
        if (current) {
            *to_free = current;
            nb_foreign_seq *next = nb_load_acquire(current->next);
            if (!prev)
                return next ? nb_mark_seq(next) : nullptr;
            nb_store_release(prev->next, next);
        }
        return list_head;
    };

    auto remove_from_type = [=](const std::type_info *type) {
        nb_type_map_slow &type_c2p_slow = internals_->type_c2p_slow;
        auto it = type_c2p_slow.find(type);
        check(it != type_c2p_slow.end(),
              "foreign binding not registered upon removal");
        void *new_value = it->second;
        nb_foreign_seq *to_free = nullptr;
        if (nb_is_foreign(it->second)) {
            new_value = remove_from_list(nb_get_foreign(it->second), &to_free);
            if (new_value)
                it.value() = new_value = nb_mark_foreign(new_value);
            else
                type_c2p_slow.erase(it);
        } else {
            auto *t = (type_data *) it->second;
            nb_store_release(t->foreign_bindings,
                             remove_from_list(
                                     nb_load_acquire(t->foreign_bindings),
                                     &to_free));
        }
        nb_type_update_c2p_fast(type, new_value);
        PyMem_Free(to_free);
    };

    bool should_remove_auto = should_autoimport_foreign(internals_, binding);
    if (auto it = internals_->foreign_manual_imports.find(binding);
        it != internals_->foreign_manual_imports.end()) {
        remove_from_type((const std::type_info *) it->second);
        should_remove_auto &= (it->second != binding->native_type);
        internals_->foreign_manual_imports.erase(it);
    }
    if (should_remove_auto)
        remove_from_type((const std::type_info *) binding->native_type);
}

static void nb_foreign_add_foreign_framework(pymb_framework *framework)
        noexcept {
    if (framework->translate_exception &&
        framework->abi_lang == pymb_abi_lang_cpp) {
        decltype(&foreign_exception_translator) translator_to_use;
        {
            lock_internals guard{internals};
            if (!internals->foreign_exception_translator)
                internals->foreign_exception_translator =
                        foreign_exception_translator;
            translator_to_use = internals->foreign_exception_translator;
        }
        register_exception_translator(translator_to_use,
                                      framework, /*at_end=*/true);
    }
    if (!(framework->flags & pymb_framework_leak_safe))
        internals->print_leak_warnings = false;
}

// (end of callbacks)

// Advertise our existence, and the above callbacks, to other frameworks
static void register_with_pymetabind(nb_internals *internals_) {
    // caller must hold the internals lock
    if (internals_->foreign_registry)
        return;
    internals_->foreign_registry = pymb_get_registry();
    if (!internals_->foreign_registry)
        raise_python_error();

    auto *fw = new pymb_framework{};
    fw->name = "nanobind " NB_ABI_TAG;
#if defined(NB_FREE_THREADED)
    fw->flags = pymb_framework_bindings_usable_forever;
#else
    fw->flags = pymb_framework_leak_safe;
#endif
    fw->abi_lang = pymb_abi_lang_cpp;
    fw->abi_extra = NB_PLATFORM_ABI_TAG;
    fw->from_python = nb_foreign_from_python;
    fw->to_python = nb_foreign_to_python;
    fw->keep_alive = nb_foreign_keep_alive;
    fw->translate_exception = nb_foreign_translate_exception;
    fw->add_foreign_binding = nb_foreign_add_foreign_binding;
    fw->remove_foreign_binding = nb_foreign_remove_foreign_binding;
    fw->add_foreign_framework = nb_foreign_add_foreign_framework;
    internals_->foreign_self = fw;

    auto *registry = internals_->foreign_registry;
    // pymb_add_framework() will call our add_foreign_framework and
    // add_foreign_binding method for each existing other framework/binding;
    // those need to lock internals, so unlock here
    unlock_internals guard{internals_};
    pymb_add_framework(registry, fw);
}

// Add the given `binding` to our type maps so that we can use it to satisfy
// from- and to-Python requests for the given C++ type
static void nb_type_import_binding(pymb_binding *binding,
                                   const std::type_info *cpptype) noexcept {
    // Caller must hold the internals lock
    internals->foreign_imported_any = true;

    auto add_to_list = [binding](void *list_head) -> void* {
        if (!list_head)
            return binding;
        nb_foreign_seq *seq = nb_ensure_seq<pymb_binding>(&list_head);
        while (true) {
            if (seq->value == binding)
                return list_head; // already added
            nb_foreign_seq *next = nb_load_acquire(seq->next);
            if (next == nullptr)
                break;
            seq = next;
        }
        nb_foreign_seq *next =
                (nb_foreign_seq *) PyMem_Malloc(sizeof(nb_foreign_seq));
        check(next, "add_foreign_binding_to_list(): out of memory!");
        next->value = binding;
        next->next = nullptr;
        nb_store_release(seq->next, next);
        return list_head;
    };

    auto [it, inserted] = internals->type_c2p_slow.try_emplace(
             cpptype, nb_mark_foreign(binding));
    if (!inserted) {
        if (nb_is_foreign(it->second))
            it.value() = nb_mark_foreign(add_to_list(nb_get_foreign(it->second)));
        else if (auto *t = (type_data *) it->second)
            nb_store_release(t->foreign_bindings,
                             add_to_list(nb_load_acquire(t->foreign_bindings)));
        else
            check(false, "null entry in type_c2p_slow");
    }
    nb_type_update_c2p_fast(cpptype, it->second);
}

// Learn to satisfy from- and to-Python requests for `cpptype` using the
// foreign binding provided by the given `pytype`. If cpptype is nullptr, infer
// the C++ type by looking at the binding, and require that its ABI match ours.
// Throws an exception on failure. Caller must hold the internals lock.
void nb_type_import_impl(PyObject *pytype, const std::type_info *cpptype) {
    if (!internals->foreign_registry)
        register_with_pymetabind(internals);
    pymb_framework* foreign_self = internals->foreign_self;
    pymb_binding* binding = pymb_get_binding(pytype);
#if defined(Py_LIMITED_API)
    str name_py = steal<str>(PyType_GetName((PyTypeObject *) pytype));
    const char *name = name_py.c_str();
#else
    const char *name = ((PyTypeObject *) pytype)->tp_name;
#endif
    if (!binding)
        raise("'%s' does not define a __pymetabind_binding__", name);
    if (binding->framework == foreign_self)
        raise("'%s' is already bound by this nanobind domain", name);
    if (!cpptype) {
        if (binding->framework->abi_lang != pymb_abi_lang_cpp)
            raise("'%s' is not written in C++, so you must specify a C++ type "
                  "to map it to", name);
        if (binding->framework->abi_extra != foreign_self->abi_extra)
            raise("'%s' has incompatible C++ ABI with this nanobind domain: "
                  "their '%s' vs our '%s'", name, binding->framework->abi_extra,
                  foreign_self->abi_extra);
        cpptype = (const std::type_info *) binding->native_type;
    }

    auto [it, inserted] = internals->foreign_manual_imports.try_emplace(
        (void *) binding, (void *) cpptype);
    if (!inserted) {
        auto *existing = (const std::type_info *) it->second;
        if (existing != cpptype && *existing != *cpptype)
            raise("'%s' was already mapped to C++ type '%s', so can't now "
                  "map it to '%s'",
                  name, existing->name(), cpptype->name());
    }
    nb_type_import_binding(binding, cpptype);
}

// Call `nb_type_import_binding()` for every ABI-compatible type provided by
// other C++ binding frameworks used by extension modules loaded in this
// interpreter, both those that exist now and those bound in the future.
void nb_type_enable_import_all() {
    nb_internals *internals_ = internals;
    {
        lock_internals guard{internals_};
        if (internals_->foreign_import_all)
            return;
        internals_->foreign_import_all = true;
        if (!internals_->foreign_registry) {
            // pymb_add_framework tells us about every existing type when we
            // register, so if we register with import enabled, we're done
            register_with_pymetabind(internals_);
            return;
        }
    }
    // If we enable import after registering, we have to iterate over the
    // list of types ourselves. Do this without the internals lock held so
    // we can reuse the pymb callback functions. foreign_registry and
    // foreign_self never change once they're non-null, so we can accesss them
    // without locking here.
    pymb_lock_registry(internals_->foreign_registry);
    PYMB_LIST_FOREACH(struct pymb_binding*, binding,
                      internals_->foreign_registry->bindings) {
        if (binding->framework != internals_->foreign_self &&
            pymb_try_ref_binding(binding)) {
            nb_foreign_add_foreign_binding(binding);
            pymb_unref_binding(binding);
        }
    }
    pymb_unlock_registry(internals_->foreign_registry);
}

// Expose hooks for other frameworks to use to work with the given nanobind
// type object. Caller must hold the internals lock.
void nb_type_export_impl(type_data *td) {
    if (!internals->foreign_registry)
        register_with_pymetabind(internals);

    void *foreign_bindings = nb_load_acquire(td->foreign_bindings);
    if (nb_is_seq(foreign_bindings)) {
        nb_foreign_seq *node = nb_get_seq<pymb_binding>(foreign_bindings);
        if (node->value->framework == internals->foreign_self)
            return; // already exported
    } else if (auto *binding = (pymb_binding *) foreign_bindings;
               binding && binding->framework == internals->foreign_self)
        return; // already exporte

    auto binding = (pymb_binding *) PyMem_Malloc(sizeof(pymb_binding));
    binding->framework = internals->foreign_self;
    binding->pytype = td->type_py;
    binding->native_type = td->type;
    binding->source_name = type_name(td->type);
    binding->context = td;

    if (foreign_bindings) {
        nb_foreign_seq *existing =
            nb_ensure_seq<pymb_binding>(&foreign_bindings);
        nb_foreign_seq *new_ =
            (nb_foreign_seq *) PyMem_Malloc(sizeof(nb_foreign_seq));
        new_->value = binding;
        new_->next = existing;
        foreign_bindings = nb_mark_seq(new_);
    } else {
        foreign_bindings = binding;
    }
    nb_store_release(td->foreign_bindings, foreign_bindings);
    pymb_add_binding(internals->foreign_registry, binding);
    // No need to call nb_type_update_c2p_fast: the map value (`td`) hasn't
    // changed, and a potential concurrent lookup that picked up the old value
    // of `td->foreign_bindings` is safe.
}

// Call `nb_type_export_impl()` for each type that currently exists in this
// nanobind domain and each type created in the future.
void nb_type_enable_export_all() {
    nb_internals *internals_ = internals;
    lock_internals guard{internals_};
    if (internals_->foreign_export_all)
        return;
    internals_->foreign_export_all = true;
    if (!internals_->foreign_registry)
        register_with_pymetabind(internals_);
    for (const auto& [type, value] : internals_->type_c2p_slow) {
        if (nb_is_foreign(value))
            continue;
        nb_type_export_impl((type_data *) value);
    }
}

// Invoke `attempt(closure, binding)` for each foreign binding `binding`
// that claims `type` and was not supplied by us, until one of them returns
// non-null. Return that first non-null value, or null if all attempts failed.
// Requires that a previous call to nb_type_c2p() have been made for `type`.
void *nb_type_try_foreign(nb_internals *internals_,
                          const std::type_info *type,
                          void* (*attempt)(void *closure,
                                           pymb_binding *binding),
                          void *closure) noexcept {
    // It is not valid to reuse the lookup made by a previous nb_type_c2p(),
    // because some bindings could have been removed between then and now.
#if defined(NB_FREE_THREADED)
    auto per_thread_guard = nb_type_lock_c2p_fast(internals_);
    nb_type_map_fast &type_c2p_fast = *per_thread_guard;
#else
    nb_type_map_fast &type_c2p_fast = internals_->type_c2p_fast;
#endif

    // We assume nb_type_c2p already ran for this type, so that there's
    // no need to handle a cache miss here.
    void *foreign_bindings = nullptr;
    if (void *result = type_c2p_fast.lookup(type); nb_is_foreign(result))
        foreign_bindings = nb_get_foreign(result);
    else if (auto *t = (type_data *) result)
        foreign_bindings = nb_load_acquire(t->foreign_bindings);
    if (!foreign_bindings)
        return nullptr;

    if (NB_LIKELY(!nb_is_seq(foreign_bindings))) {
        // Single foreign binding - check that it's not our own
        auto *binding = (pymb_binding *) foreign_bindings;
        if (binding->framework != internals_->foreign_self &&
            pymb_try_ref_binding(binding)) {
#if defined(NB_FREE_THREADED)
            // attempt() might execute Python code; drop the map mutex
            // to avoid a deadlock
            per_thread_guard = {};
#endif
            void *result = attempt(closure, binding);
            pymb_unref_binding(binding);
            return result;
        }
        return nullptr;
    }

    // Multiple foreign bindings - try all except our own.
#if !defined(NB_FREE_THREADED)
    nb_foreign_seq *current = nb_get_seq<pymb_binding>(foreign_bindings);
    while (current) {
        auto *binding = current->value;
        if (binding->framework != internals_->foreign_self &&
            pymb_try_ref_binding(binding)) {
            void *result = attempt(closure, binding);
            pymb_unref_binding(binding);
            if (result)
                return result;
        }
        current = current->next;
    }
    return nullptr;
#else
    // In free-threaded mode, this is tricky: we need to drop the
    // per_thread_guard before calling attempt(), but once we do so,
    // any of these bindings that might be in the middle of getting deleted
    // can be concurrently removed from the linked list, which would interfere
    // with our iteration. Copy the binding pointers out of the list to avoid
    // this problem.

    // Count the number of foreign bindings we might see
    size_t len = 0;
    nb_foreign_seq *current = nb_get_seq<pymb_binding>(foreign_bindings);
    while (current) {
        ++len;
        current = nb_load_acquire(current->next);
    }

    // Allocate temporary storage for that many pointers
    pymb_binding **scratch =
        (pymb_binding **) alloca(len * sizeof(pymb_binding*));
    pymb_binding **scratch_tail = scratch;

    // Iterate again, taking out strong references and saving pointers to
    // our scratch storage. Concurrency notes:
    // - If bindings are removed while we iterate, we may either visit them
    //   (and do nothing since try_ref returns false) or skip them. Binding
    //   removal will lock all c2p_fast maps in between when it modifies the
    //   linked list and when it deallocates the removed node, so we're safe
    //   from concurrent deallocation as long as we hold the lock.
    // - If bindings are added at the front of the list while we iterate,
    //   they don't impact us since we're working with a local copy of the
    //   head ptr `foreign_bindings`.
    // - If bindings are added at the rear of the list while we iterate,
    //   we may either include them (if we didn't use some of the scratch
    //   slots we allocated previously) or not, but we'll always decref
    //   everything we incref.
    current = nb_get_seq<pymb_binding>(foreign_bindings);
    while (current && scratch != scratch_tail + len) {
        auto *binding = current->value;
        if (binding->framework != internals_->foreign_self &&
            pymb_try_ref_binding(binding))
            *scratch_tail++ = binding;
        current = nb_load_acquire(current->next);
    }

    // Drop the lock and proceed using only our saved binding pointers.
    // Since we obtained strong references to them, there is no remaining
    // concurrent-destruction hazard.
    per_thread_guard = {};
    void *result = nullptr;
    while (scratch != scratch_tail) {
        if (!result)
            result = attempt(closure, *scratch);
        pymb_unref_binding(*scratch);
        ++scratch;
    }
    return result;
#endif
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

#endif /* !defined(NB_DISABLE_INTEROP) */
