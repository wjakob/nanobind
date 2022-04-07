#pragma once

#include <nanobind/nanobind.h>
#include <string>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <> struct type_caster<std::string> {
    NB_TYPE_CASTER(std::string, const_name("str | bytes"));

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        Py_ssize_t size;
        const char *str = PyUnicode_AsUTF8AndSize(src.ptr(), &size);
        if (str) {
            value = std::string(str, (size_t) size);
            return true;
        }
        PyErr_Clear();
        if (!PyBytes_AsStringAndSize(src.ptr(), const_cast<char **>(&str), &size)) {
            value = std::string(str, (size_t) size);
            return true;
        }
        PyErr_Clear();
        return false;
    }

    static handle from_cpp(const std::string &value, rv_policy,
                           cleanup_list *) noexcept {
        return PyUnicode_FromStringAndSize(value.c_str(), value.size());
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
