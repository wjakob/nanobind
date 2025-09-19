#pragma once
#include <nanobind/nanobind.h>

inline void delattr_and_ensure_destroyed(nanobind::handle scope,
                                         const char *name) {
    if (!hasattr(scope, name))
        return;
    bool destroyed = false;
    bool **destroyed_pp = new (bool*)(&destroyed);
    nanobind::detail::keep_alive(scope.attr(name).ptr(), destroyed_pp,
                                 [](void *ptr) noexcept {
                                     bool **destroyed_pp = (bool **) ptr;
                                     if (*destroyed_pp) {
                                         **destroyed_pp = true;
                                     }
                                     delete destroyed_pp;
                                 });
    delattr(scope, name);
    if (!destroyed) {
        auto collect = nanobind::module_::import_("gc").attr("collect");
        collect();
        if (!destroyed) {
            collect();
            if (!destroyed) {
                *destroyed_pp = nullptr;
                nanobind::detail::raise("Couldn't delete binding for %s in %s!",
                                        name, repr(scope).c_str());
            }
        }
    }
}
