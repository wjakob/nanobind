#include <nanobind/nanobind.h>
#include <string>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <> struct type_caster<std::string> {
    bool load(handle src, bool) noexcept {
        Py_ssize_t size;
        const char *str = PyUnicode_AsUTF8AndSize(src.ptr(), &size);
        if (!str) {
            PyErr_Clear();
            return false;
        }
        value = std::string(str, (size_t) size);
        return true;
    }

    static handle cast(const std::string &value, rv_policy /* policy */,
                       handle /* parent */) noexcept {
        return PyUnicode_FromStringAndSize(value.c_str(), value.size());
    }

    NB_TYPE_CASTER(std::string, const_name("str"));
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
