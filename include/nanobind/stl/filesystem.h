/*
    nanobind/stl/filesystem.h: type caster for std::string

    Copyright (c) 2022 Qingnan Zhou and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>
#include <string>

#ifdef __has_include
#if __has_include(<filesystem>)
#include <filesystem>
#define NB_HAS_FILESYSTEM 1
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#define NB_HAS_EXPERIMENTAL_FILESYSTEM 1
#endif
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T>
struct path_caster {
private:
    static PyObject *unicode_from_fs_native(const std::string &w) {
#if !defined(PYPY_VERSION)
        return PyUnicode_DecodeFSDefaultAndSize(w.c_str(), Py_ssize_t(w.size()));
#else
        // PyPy mistakenly declares the first parameter as non-const.
        return PyUnicode_DecodeFSDefaultAndSize(const_cast<char *>(w.c_str()),
                                                Py_ssize_t(w.size()));
#endif
    }

    static PyObject *unicode_from_fs_native(const std::wstring &w) {
        return PyUnicode_FromWideChar(w.c_str(), Py_ssize_t(w.size()));
    }

public:
    static handle from_cpp(const T &path, rv_policy, cleanup_list *) noexcept {
        if (auto py_str = unicode_from_fs_native(path.native())) {
            return module_::import_("pathlib")
                .attr("Path")(steal(py_str))
                .release();
        }
        return {};
    }

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        // PyUnicode_FSConverter and PyUnicode_FSDecoder normally take care of
        // calling PyOS_FSPath themselves, but that's broken on PyPy (PyPy
        // issue #3168) so we do it ourselves instead.
        PyObject *buf = PyOS_FSPath(src.ptr());
        if (!buf) {
            PyErr_Clear();
            return false;
        }
        PyObject *native = nullptr;
        if constexpr (std::is_same_v<typename T::value_type, char>) {
            if (PyUnicode_FSConverter(buf, &native) != 0) {
                if (auto *c_str = PyBytes_AsString(native)) {
                    // AsString returns a pointer to the internal buffer, which
                    // must not be free'd.
                    value = c_str;
                }
            }
        } else if constexpr (std::is_same_v<typename T::value_type, wchar_t>) {
            if (PyUnicode_FSDecoder(buf, &native) != 0) {
                if (auto *c_str = PyUnicode_AsWideCharString(native, nullptr)) {
                    // AsWideCharString returns a new string that must be
                    // free'd.
                    value = c_str;  // Copies the string.
                    PyMem_Free(c_str);
                }
            }
        }
        Py_XDECREF(native);
        Py_DECREF(buf);
        if (PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }

    NB_TYPE_CASTER(T, const_name("os.PathLike"));
};

#if defined(NB_HAS_FILESYSTEM)
template <>
struct type_caster<std::filesystem::path>
    : public path_caster<std::filesystem::path> {};
#elif defined(NB_HAS_EXPERIMENTAL_FILESYSTEM)
template <>
struct type_caster<std::experimental::filesystem::path>
    : public path_caster<std::experimental::filesystem::path> {};
#endif

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
