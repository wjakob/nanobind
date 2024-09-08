/*
    src/implicit.cpp: functions for registering implicit conversions

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/trampoline.h>
#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

void implicitly_convertible(const std::type_info *src,
                            const std::type_info *dst) noexcept {
    nb_internals *internals_ = internals;
    type_data *t = nb_type_c2p(internals_, dst);
    check(t, "nanobind::detail::implicitly_convertible(src=%s, dst=%s): "
             "destination type unknown!", type_name(src), type_name(dst));

    lock_internals guard(internals_);
    size_t size = 0;

    if (t->flags & (uint32_t) type_flags::has_implicit_conversions) {
        while (t->implicit.cpp && t->implicit.cpp[size])
            size++;
    } else {
        t->implicit.cpp = nullptr;
        t->implicit.py = nullptr;
        t->flags |= (uint32_t) type_flags::has_implicit_conversions;
    }

    void **data = (void **) PyMem_Malloc(sizeof(void *) * (size + 2));

    if (size)
        memcpy(data, t->implicit.cpp, size * sizeof(void *));
    data[size] = (void *) src;
    data[size + 1] = nullptr;
    PyMem_Free(t->implicit.cpp);
    t->implicit.cpp = (decltype(t->implicit.cpp)) data;
}

void implicitly_convertible(bool (*predicate)(PyTypeObject *, PyObject *,
                                              cleanup_list *),
                            const std::type_info *dst) noexcept {
    nb_internals *internals_ = internals;
    type_data *t = nb_type_c2p(internals_, dst);
    check(t, "nanobind::detail::implicitly_convertible(src=<predicate>, dst=%s): "
             "destination type unknown!", type_name(dst));

    lock_internals guard(internals_);
    size_t size = 0;

    if (t->flags & (uint32_t) type_flags::has_implicit_conversions) {
        while (t->implicit.py && t->implicit.py[size])
            size++;
    } else {
        t->implicit.cpp = nullptr;
        t->implicit.py = nullptr;
        t->flags |= (uint32_t) type_flags::has_implicit_conversions;
    }

    void **data = (void **) PyMem_Malloc(sizeof(void *) * (size + 2));
    if (size)
        memcpy(data, t->implicit.py, size * sizeof(void *));
    data[size] = (void *) predicate;
    data[size + 1] = nullptr;
    PyMem_Free(t->implicit.py);
    t->implicit.py = (decltype(t->implicit.py)) data;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
