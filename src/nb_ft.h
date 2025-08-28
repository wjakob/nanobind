/*
    src/nb_ft.h: implementation details related to free-threaded Python

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#if !defined(Py_GIL_DISABLED)
/// Trivial implementations for non-free-threaded Python
inline void make_immortal(PyObject *) noexcept { }
inline void nb_enable_try_inc_ref(PyObject *) noexcept { }
inline bool nb_try_inc_ref(PyObject *obj) noexcept {
    if (Py_REFCNT(obj) > 0) {
        Py_INCREF(obj);
        return true;
    }
    return false;
}
#else
extern void make_immortal(PyObject *op) noexcept;

#if PY_VERSION_HEX >= 0x030E00A5
/// Sufficiently recent CPython versions provide an API for the following operations
inline void nb_enable_try_inc_ref(PyObject *obj) noexcept {
    PyUnstable_EnableTryIncRef(obj);
}
inline bool nb_try_inc_ref(PyObject *obj) noexcept {
    return PyUnstable_TryIncRef(obj);
}
#else
/// Otherwise, nanabind ships with a low-level implementation
extern void nb_enable_try_inc_ref(PyObject *) noexcept;
extern bool nb_try_inc_ref(PyObject *obj) noexcept;
#endif
#endif
