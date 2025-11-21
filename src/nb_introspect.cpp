#include "nb_introspect.h"

#include "nb_internals.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

namespace {

/* This file keeps the PEP 3107/362 plumbing self-contained so nb_func only
   exposes the hooks. We translate nanobind's signature metadata directly
   into annotations/text signatures without touching nb.__nb_signature__ or
   cached Python objects to avoid ABI changes. */

static char *dup_string(const char *s) {
#if defined(_WIN32)
    char *result = _strdup(s);
#else
    char *result = strdup(s);
#endif
    if (!result)
        fail("nanobind: strdup() failed!");
    return result;
}

static char *type_name_local(const std::type_info *t) {
    const char *name_in = t->name();
#if defined(__GNUG__)
    int status = 0;
    char *name = abi::__cxa_demangle(name_in, nullptr, nullptr, &status);
    if (!name)
        return dup_string(name_in);
#else
    char *name = dup_string(name_in);
    auto strexc = [](char *str, const char *sub) {
        size_t len = strlen(sub);
        if (len == 0)
            return;
        char *p = str;
        while ((p = strstr(p, sub)))
            memmove(p, p + len, strlen(p + len) + 1);
    };
    strexc(name, "class ");
    strexc(name, "struct ");
    strexc(name, "enum ");
#endif
    return name;
}

NB_INLINE void trim_inplace(std::string &value) {
    size_t start = 0;
    while (start < value.size() && std::isspace((unsigned char) value[start]))
        ++start;
    size_t end = value.size();
    while (end > start && std::isspace((unsigned char) value[end - 1]))
        --end;
    if (start != 0 || end != value.size())
        value = value.substr(start, end - start);
}

struct param_state {
    std::string name;
    std::string annotation;
    std::string sanitized;
    bool active = false;
    bool annotate = false;
    bool collecting = false;
#if PY_VERSION_HEX < 0x030A0000
    bool optional_wrap = false;
#else
    bool optional_suffix = false;
#endif
};

struct metadata_builder {
    pep_metadata &meta;
    param_state param;
    bool capturing_return = false;

    explicit metadata_builder(pep_metadata &m) : meta(m) { }

    void reset_param() { param = param_state(); }

    void start_param(std::string name, bool annotate, bool include_token) {
        param.active = true;
        param.annotate = annotate;
        param.collecting = false;
        param.name = std::move(name);
        param.annotation.clear();
        param.sanitized = include_token ? param.name : std::string();
#if PY_VERSION_HEX < 0x030A0000
        param.optional_wrap = false;
#else
        param.optional_suffix = false;
#endif
    }

    void append_annotation(const char *data, size_t size) {
        if (size == 0)
            return;
        if (capturing_return) {
            meta.return_type.append(data, size);
            return;
        }
        if (param.active && param.annotate && param.collecting)
            param.annotation.append(data, size);
    }

    void append_annotation(char c) { append_annotation(&c, 1); }

    void append_sanitized(const char *data) {
        if (param.active && !param.sanitized.empty())
            param.sanitized.append(data);
    }

    void append_sanitized(const char *data, size_t size) {
        if (param.active && !param.sanitized.empty())
            param.sanitized.append(data, size);
    }

    void finish_param(const arg_data *arg_info) {
        if (!param.active)
            return;

        if (arg_info && arg_info->value) {
            if (arg_info->signature) {
                append_sanitized(" = ");
                append_sanitized(arg_info->signature);
            } else {
                PyObject *repr = PyObject_Repr(arg_info->value);
                if (repr) {
                    Py_ssize_t size = 0;
                    const char *cstr = PyUnicode_AsUTF8AndSize(repr, &size);
                    if (cstr) {
                        append_sanitized(" = ");
                        append_sanitized(cstr, (size_t) size);
                    }
                    Py_DECREF(repr);
                } else {
                    PyErr_Clear();
                }
            }
        }

#if PY_VERSION_HEX < 0x030A0000
        if (param.optional_wrap)
            param.annotation.push_back(']');
#else
        if (param.optional_suffix)
            param.annotation.append(" | None");
#endif

        trim_inplace(param.annotation);
        if (param.annotate && !param.name.empty()) {
            if (param.annotation.empty())
                param.annotation = "typing.Any";
            meta.parameters.push_back({param.name, param.annotation});
        }

        if (!param.sanitized.empty())
            meta.sanitized_tokens.emplace_back(std::move(param.sanitized));

        reset_param();
    }
};

} // namespace

bool nb_collect_pep_metadata(const func_data *f, pep_metadata &out) noexcept {
    if (f->flags & (uint32_t) func_flags::has_signature)
        return false;

    const bool is_method      = f->flags & (uint32_t) func_flags::is_method,
               has_args       = f->flags & (uint32_t) func_flags::has_args,
               has_var_args   = f->flags & (uint32_t) func_flags::has_var_args,
               has_var_kwargs = f->flags & (uint32_t) func_flags::has_var_kwargs;

    out.parameters.clear();
    out.sanitized_tokens.clear();
    out.return_type.clear();

    nb_internals *internals_ = internals;
    metadata_builder builder(out);

    const std::type_info **descr_type = f->descr_types;
    uint32_t arg_index = 0;
    bool rv = false;

    auto flush_optional = [&](uint32_t index) {
        if (!has_args)
            return;
        const arg_data &arg = f->args[index];
        if (arg.flag & (uint8_t) cast_flags::accepts_none) {
#if PY_VERSION_HEX < 0x030A0000
            builder.param.annotation.append("typing.Optional[");
            builder.param.optional_wrap = true;
#else
            builder.param.optional_suffix = true;
#endif
        }
    };

    for (const char *pc = f->descr; *pc != '\0'; ++pc) {
        char c = *pc;

        switch (c) {
            case '@':
                pc++;
                if (!rv) {
                    while (*pc && *pc != '@')
                        builder.append_annotation(*pc++);
                    if (*pc == '@')
                        pc++;
                    while (*pc && *pc != '@')
                        pc++;
                } else {
                    while (*pc && *pc != '@')
                        pc++;
                    if (*pc == '@')
                        pc++;
                    while (*pc && *pc != '@')
                        builder.append_annotation(*pc++);
                }
                break;

            case '{':
                {
                    const char *arg_name = has_args ? f->args[arg_index].name : nullptr;

                    if (has_var_kwargs && arg_index + 1 == f->nargs) {
                        std::string name = arg_name ? arg_name : "kwargs";
                        out.parameters.push_back({name, "typing.Dict[str, typing.Any]"});
                        out.sanitized_tokens.emplace_back(std::string("**") + name);
                        pc += 4;
                        break;
                    }

                    if (arg_index == f->nargs_pos) {
                        if (has_var_args) {
                            std::string name = arg_name ? arg_name : "args";
                            out.parameters.push_back({name, "typing.Tuple[typing.Any, ...]"});
                            out.sanitized_tokens.emplace_back(std::string("*") + name);
                            pc += 5;
                            break;
                        } else {
                            out.sanitized_tokens.emplace_back("*");
                        }
                    }

                    if (is_method && arg_index == 0) {
                        out.sanitized_tokens.emplace_back("self");
                        while (*pc != '}') {
                            if (*pc == '%')
                                descr_type++;
                            pc++;
                        }
                        arg_index++;
                        break;
                    }

                    std::string param_name;
                    if (arg_name) {
                        param_name = arg_name;
                    } else {
                        param_name = "arg";
                        if (f->nargs > 1 + (uint32_t) is_method)
                            param_name += std::to_string(arg_index - is_method);
                    }

                    builder.start_param(param_name, true, true);
                    builder.param.collecting = false;
                    flush_optional(arg_index);
                }
                break;

            case '}':
                builder.finish_param(has_args ? (f->args + arg_index) : nullptr);
                arg_index++;
                if (arg_index == f->nargs_pos && !has_args)
                    out.sanitized_tokens.emplace_back("/");
                break;

            case '%':
                check(*descr_type,
                      "nanobind::detail::nb_collect_pep_metadata(): missing type!");

                if (!(is_method && arg_index == 0)) {
                    bool found = false;
                    auto it = internals_->type_c2p_slow.find(*descr_type);

                    if (it != internals_->type_c2p_slow.end()) {
                        handle th((PyObject *) it->second->type_py);
                        str module = borrow<str>(th.attr("__module__"));
                        str qualname = borrow<str>(th.attr("__qualname__"));
                        builder.append_annotation(module.c_str(), strlen(module.c_str()));
                        builder.append_annotation('.');
                        builder.append_annotation(qualname.c_str(), strlen(qualname.c_str()));
                        found = true;
                    }

                    if (!found) {
                        char *name = type_name_local(*descr_type);
                        builder.append_annotation(name, strlen(name));
                        free(name);
                    }
                }

                descr_type++;
                break;

            case '-':
                if (pc[1] == '>') {
                    rv = true;
                    builder.capturing_return = true;
                    out.return_type.clear();
                    ++pc;
                    break;
                }
                builder.append_annotation(c);
                break;

            default:
                if (builder.param.active && c == ':') {
                    builder.param.collecting = true;
                    break;
                }
                builder.append_annotation(c);
                break;
        }
    }

    check(arg_index == f->nargs && !*descr_type,
          "nanobind::detail::nb_collect_pep_metadata(%s): argument inconsistency.",
          f->name);

    return true;
}

PyObject *nb_introspect_annotations(nb_func *, const func_data *f) noexcept {
    pep_metadata meta;
    if (!nb_collect_pep_metadata(f, meta)) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject *annotations = PyDict_New();
    if (!annotations)
        return nullptr;

    for (const auto &entry : meta.parameters) {
        PyObject *value = PyUnicode_FromString(entry.annotation.c_str());
        if (!value || PyDict_SetItemString(annotations, entry.name.c_str(), value) < 0) {
            Py_XDECREF(value);
            Py_DECREF(annotations);
            return nullptr;
        }
        Py_DECREF(value);
    }

    trim_inplace(meta.return_type);
    if (!meta.return_type.empty()) {
        PyObject *ret = PyUnicode_FromString(meta.return_type.c_str());
        if (!ret || PyDict_SetItemString(annotations, "return", ret) < 0) {
            Py_XDECREF(ret);
            Py_DECREF(annotations);
            return nullptr;
        }
        Py_DECREF(ret);
    }

    return annotations;
}

PyObject *nb_introspect_text_signature(nb_func *, const func_data *f) noexcept {
    pep_metadata meta;
    if (!nb_collect_pep_metadata(f, meta)) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    std::string signature = "(";
    for (size_t i = 0; i < meta.sanitized_tokens.size(); ++i) {
        signature += meta.sanitized_tokens[i];
        if (i + 1 < meta.sanitized_tokens.size())
            signature += ", ";
    }
    signature += ")";

    return PyUnicode_FromString(signature.c_str());
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
