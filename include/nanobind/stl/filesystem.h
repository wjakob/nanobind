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
        PyObject *result =
            PyUnicode_DecodeFSDefaultAndSize(w.c_str(), Py_ssize_t(w.size()));
        return steal<str>(result);
    }

    static str unicode_from_fs_native(const std::wstring &w) {
        PyObject *result =
            PyUnicode_FromWideChar(w.c_str(), Py_ssize_t(w.size()));
        return steal<str>(result);
    }

public:
    static handle from_cpp(const std::filesystem::path &path, rv_policy,
                           cleanup_list *) noexcept {
        auto py_str = unicode_from_fs_native(path.native());
        if (py_str.is_valid()) {
            try {
                return module_::import_("pathlib")
                    .attr("Path")(py_str)
                    .release();
            } catch (python_error &e) {
                e.restore();
            }
            return handle();
        }
        return handle();
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
        if constexpr (std::is_same_v<typename std::filesystem::path::value_type,
                                     char>) {
            if (PyUnicode_FSConverter(buf, &native) != 0) {
                if (auto *c_str = PyBytes_AsString(native)) {
                    // AsString returns a pointer to the internal buffer, which
                    // must not be free'd.
                    value = c_str;
                }
            }
        } else {
            if (PyUnicode_FSDecoder(buf, &native) != 0) {
                if (auto *c_str = PyUnicode_AsWideCharString(native, nullptr)) {
                    // AsWideCharString returns a new string that must be
                    // free'd.
                    value = c_str;  // Copies the string.
                    PyMem_Free(c_str);
                }
            }
        }
        Py_DECREF(buf);
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
