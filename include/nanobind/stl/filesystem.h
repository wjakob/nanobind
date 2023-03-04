/*
    nanobind/stl/filesystem.h: type caster for std::string

    Copyright (c) 2023 Qingnan Zhou and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>

#include <filesystem>
#include <string>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <>
struct type_caster<std::filesystem::path> {
private:
    static str unicode_from_fs_native(const std::string &w) {
        PyObject* result = PyUnicode_DecodeFSDefaultAndSize(w.c_str(),
                                                Py_ssize_t(w.size()));
        return steal<str>(result);
    }

    static str unicode_from_fs_native(const std::wstring &w) {
        PyObject* result = PyUnicode_FromWideChar(w.c_str(), Py_ssize_t(w.size()));
        return steal<str>(result);
    }

public:
    static handle from_cpp(const std::filesystem::path &path, rv_policy,
                           cleanup_list *) noexcept {
        if (auto py_str = unicode_from_fs_native(path.native())) {
            return module_::import_("pathlib")
                .attr("Path")(py_str)
                .release();
        }
        return {};
    }

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        PyObject *native = nullptr;
        if constexpr (std::is_same_v<typename std::filesystem::path::value_type,
                                     char>) {
            if (PyUnicode_FSConverter(src.ptr(), &native) != 0) {
                if (auto *c_str = PyBytes_AsString(native)) {
                    // AsString returns a pointer to the internal buffer, which
                    // must not be free'd.
                    value = c_str;
                }
            }
        } else {
            if (PyUnicode_FSDecoder(src.ptr(), &native) != 0) {
                if (auto *c_str = PyUnicode_AsWideCharString(native, nullptr)) {
                    // AsWideCharString returns a new string that must be
                    // free'd.
                    value = c_str;  // Copies the string.
                    PyMem_Free(c_str);
                }
            }
        }
        Py_XDECREF(native);
        if (PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }

    NB_TYPE_CASTER(std::filesystem::path, const_name("os.PathLike"));
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
