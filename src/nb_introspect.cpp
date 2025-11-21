#include "nb_introspect.h"

#include "nb_internals.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
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

enum class signature_param_kind {
    positional_only,
    positional_or_keyword,
    keyword_only,
    var_positional,
    var_keyword
};

struct signature_param {
    std::string name;
    std::string annotation;
    signature_param_kind kind;
    PyObject *default_value = nullptr;
    bool has_annotation = false;
};

struct signature_metadata {
    std::vector<signature_param> parameters;
    std::vector<std::string> sanitized_tokens;
    std::string return_type;
};

struct param_state {
    signature_param entry;
    std::string sanitized;
    bool active = false;
    bool annotate = false;
    bool collecting = false;
    const arg_data *arg_info = nullptr;
#if PY_VERSION_HEX < 0x030A0000
    bool optional_wrap = false;
#else
    bool optional_suffix = false;
#endif
};

struct metadata_builder {
    signature_metadata &meta;
    param_state param;
    bool capturing_return = false;

    explicit metadata_builder(signature_metadata &m) : meta(m) { }

    void reset_param() { param = param_state(); }

    void start_param(std::string name, signature_param_kind kind, bool annotate,
                     bool include_token, const arg_data *arg_info) {
        param.active = true;
        param.annotate = annotate;
        param.collecting = false;
        param.entry.name = std::move(name);
        param.entry.annotation.clear();
        param.entry.kind = kind;
        param.entry.default_value = arg_info ? arg_info->value : nullptr;
        param.entry.has_annotation = annotate;
        param.arg_info = arg_info;
        param.sanitized = include_token ? param.entry.name : std::string();
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
            param.entry.annotation.append(data, size);
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

    void finish_param() {
        if (!param.active)
            return;

        const arg_data *arg_info = param.arg_info;
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
            param.entry.annotation.push_back(']');
#else
        if (param.annotate && param.optional_suffix)
            param.entry.annotation.append(" | None");
#endif

        if (param.annotate) {
            trim_inplace(param.entry.annotation);
            if (param.entry.annotation.empty())
                param.entry.annotation = "typing.Any";
        } else {
            param.entry.annotation.clear();
        }

        meta.parameters.push_back(param.entry);

        if (!param.sanitized.empty())
            meta.sanitized_tokens.emplace_back(std::move(param.sanitized));

        reset_param();
    }
};

struct inspect_cache {
    PyObject *parameter = nullptr;
    PyObject *signature = nullptr;
    PyObject *empty = nullptr;
    PyObject *kind_positional_only = nullptr;
    PyObject *kind_positional_or_keyword = nullptr;
    PyObject *kind_keyword_only = nullptr;
    PyObject *kind_var_positional = nullptr;
    PyObject *kind_var_keyword = nullptr;
};

static inspect_cache g_inspect;

static bool ensure_inspect_cache() noexcept {
    if (g_inspect.parameter)
        return true;

    PyObject *inspect = PyImport_ImportModule("inspect");
    if (!inspect)
        return false;

    PyObject *parameter = nullptr, *signature = nullptr, *empty = nullptr;
    PyObject *pos_only = nullptr, *pos_or_kw = nullptr, *kw_only = nullptr;
    PyObject *var_pos = nullptr, *var_kw = nullptr;

    parameter = PyObject_GetAttrString(inspect, "Parameter");
    signature = PyObject_GetAttrString(inspect, "Signature");
    empty = PyObject_GetAttrString(inspect, "_empty");
    if (!parameter || !signature || !empty)
        goto fail;

    pos_only = PyObject_GetAttrString(parameter, "POSITIONAL_ONLY");
    pos_or_kw = PyObject_GetAttrString(parameter, "POSITIONAL_OR_KEYWORD");
    kw_only = PyObject_GetAttrString(parameter, "KEYWORD_ONLY");
    var_pos = PyObject_GetAttrString(parameter, "VAR_POSITIONAL");
    var_kw = PyObject_GetAttrString(parameter, "VAR_KEYWORD");
    if (!pos_only || !pos_or_kw || !kw_only || !var_pos || !var_kw)
        goto fail;

    g_inspect.parameter = parameter;
    g_inspect.signature = signature;
    g_inspect.empty = empty;
    g_inspect.kind_positional_only = pos_only;
    g_inspect.kind_positional_or_keyword = pos_or_kw;
    g_inspect.kind_keyword_only = kw_only;
    g_inspect.kind_var_positional = var_pos;
    g_inspect.kind_var_keyword = var_kw;
    Py_DECREF(inspect);
    return true;

fail:
    Py_XDECREF(parameter);
    Py_XDECREF(signature);
    Py_XDECREF(empty);
    Py_XDECREF(pos_only);
    Py_XDECREF(pos_or_kw);
    Py_XDECREF(kw_only);
    Py_XDECREF(var_pos);
    Py_XDECREF(var_kw);
    Py_DECREF(inspect);
    return false;
}

static PyObject *inspect_kind_object(signature_param_kind kind) {
    switch (kind) {
        case signature_param_kind::positional_only:
            return g_inspect.kind_positional_only;
        case signature_param_kind::positional_or_keyword:
            return g_inspect.kind_positional_or_keyword;
        case signature_param_kind::keyword_only:
            return g_inspect.kind_keyword_only;
        case signature_param_kind::var_positional:
            return g_inspect.kind_var_positional;
        case signature_param_kind::var_keyword:
            return g_inspect.kind_var_keyword;
        default:
            return g_inspect.kind_positional_or_keyword;
    }
}

} // namespace

static bool build_signature_metadata(const func_data *f,
                                     signature_metadata &out) noexcept {
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

    const bool positional_only_section = !has_args;
    bool keyword_only_section = false;

    const std::type_info **descr_type = f->descr_types;
    uint32_t arg_index = 0;
    bool rv = false;

    auto current_kind = [&](uint32_t) {
        if (keyword_only_section)
            return signature_param_kind::keyword_only;
        return positional_only_section ? signature_param_kind::positional_only
                                       : signature_param_kind::positional_or_keyword;
    };

    auto flush_optional = [&](uint32_t index) {
        if (!has_args)
            return;
        const arg_data &arg = f->args[index];
        if (arg.flag & (uint8_t) cast_flags::accepts_none) {
#if PY_VERSION_HEX < 0x030A0000
            builder.param.entry.annotation.append("typing.Optional[");
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
                        signature_param entry;
                        entry.name = name;
                        entry.annotation = "typing.Dict[str, typing.Any]";
                        entry.kind = signature_param_kind::var_keyword;
                        entry.default_value = nullptr;
                        entry.has_annotation = true;
                        out.parameters.push_back(std::move(entry));
                        out.sanitized_tokens.emplace_back(std::string("**") + name);
                        pc += 4;
                        break;
                    }

                    if (arg_index == f->nargs_pos) {
                        if (has_var_args) {
                            std::string name = arg_name ? arg_name : "args";
                            signature_param entry;
                            entry.name = name;
                            entry.annotation = "typing.Tuple[typing.Any, ...]";
                            entry.kind = signature_param_kind::var_positional;
                            entry.default_value = nullptr;
                            entry.has_annotation = true;
                            out.parameters.push_back(std::move(entry));
                            out.sanitized_tokens.emplace_back(std::string("*") + name);
                            keyword_only_section = true;
                            pc += 5;
                            break;
                        } else {
                            out.sanitized_tokens.emplace_back("*");
                            keyword_only_section = true;
                        }
                    }

                    if (is_method && arg_index == 0) {
                        out.sanitized_tokens.emplace_back("self");
                        signature_param entry;
                        entry.name = "self";
                        entry.annotation.clear();
                        entry.kind = current_kind(arg_index);
                        entry.default_value = nullptr;
                        entry.has_annotation = false;
                        out.parameters.push_back(std::move(entry));
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

                    const arg_data *arg_info = has_args ? (f->args + arg_index) : nullptr;
                    signature_param_kind kind = current_kind(arg_index);
                    builder.start_param(param_name, kind, true, true, arg_info);
                    flush_optional(arg_index);
                }
                break;

            case '}':
                builder.finish_param();
                arg_index++;
                if (arg_index == f->nargs_pos && !has_args)
                    out.sanitized_tokens.emplace_back("/");
                break;

            case '%':
                check(*descr_type,
                      "nanobind::detail::build_signature_metadata(): missing type!");

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
          "nanobind::detail::build_signature_metadata(%s): argument inconsistency.",
          f->name);

    trim_inplace(out.return_type);
    return true;
}

PyObject *nb_introspect_annotations(nb_func *, const func_data *f) noexcept {
    signature_metadata meta;
    if (!build_signature_metadata(f, meta)) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject *annotations = PyDict_New();
    if (!annotations)
        return nullptr;

    for (const auto &entry : meta.parameters) {
        if (!entry.has_annotation)
            continue;
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
    signature_metadata meta;
    if (!build_signature_metadata(f, meta)) {
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

PyObject *nb_introspect_signature(nb_func *, const func_data *f) noexcept {
    signature_metadata meta;
    if (!build_signature_metadata(f, meta)) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (!ensure_inspect_cache())
        return nullptr;

    Py_ssize_t count = (Py_ssize_t) meta.parameters.size();
    PyObject *param_list = PyList_New(count);
    if (!param_list)
        return nullptr;

    for (Py_ssize_t i = 0; i < count; ++i) {
        const signature_param &entry = meta.parameters[(size_t) i];
        PyObject *name = PyUnicode_FromString(entry.name.c_str());
        if (!name) {
            Py_DECREF(param_list);
            return nullptr;
        }

        PyObject *kind = inspect_kind_object(entry.kind);
        Py_INCREF(kind);

        PyObject *args = PyTuple_New(2);
        if (!args) {
            Py_DECREF(name);
            Py_DECREF(kind);
            Py_DECREF(param_list);
            return nullptr;
        }
        PyTuple_SET_ITEM(args, 0, name);
        PyTuple_SET_ITEM(args, 1, kind);

        PyObject *kwargs = PyDict_New();
        if (!kwargs) {
            Py_DECREF(args);
            Py_DECREF(param_list);
            return nullptr;
        }

        if (entry.default_value) {
            PyObject *value = entry.default_value;
            Py_INCREF(value);
            if (PyDict_SetItemString(kwargs, "default", value) != 0) {
                Py_DECREF(value);
                Py_DECREF(kwargs);
                Py_DECREF(args);
                Py_DECREF(param_list);
                return nullptr;
            }
            Py_DECREF(value);
        }

        if (entry.has_annotation && !entry.annotation.empty()) {
            PyObject *ann = PyUnicode_FromString(entry.annotation.c_str());
            if (!ann || PyDict_SetItemString(kwargs, "annotation", ann) != 0) {
                Py_XDECREF(ann);
                Py_DECREF(kwargs);
                Py_DECREF(args);
                Py_DECREF(param_list);
                return nullptr;
            }
            Py_DECREF(ann);
        }

        PyObject *param_obj = PyObject_Call(g_inspect.parameter, args, kwargs);
        Py_DECREF(args);
        Py_DECREF(kwargs);
        if (!param_obj) {
            Py_DECREF(param_list);
            return nullptr;
        }
        PyList_SET_ITEM(param_list, i, param_obj);
    }

    PyObject *parameters_tuple = PyList_AsTuple(param_list);
    Py_DECREF(param_list);
    if (!parameters_tuple)
        return nullptr;

    PyObject *kwargs = PyDict_New();
    if (!kwargs) {
        Py_DECREF(parameters_tuple);
        return nullptr;
    }

    if (PyDict_SetItemString(kwargs, "parameters", parameters_tuple) != 0) {
        Py_DECREF(parameters_tuple);
        Py_DECREF(kwargs);
        return nullptr;
    }
    Py_DECREF(parameters_tuple);

    PyObject *return_annotation;
    if (meta.return_type.empty()) {
        return_annotation = g_inspect.empty;
        Py_INCREF(return_annotation);
    } else {
        return_annotation = PyUnicode_FromString(meta.return_type.c_str());
        if (!return_annotation) {
            Py_DECREF(kwargs);
            return nullptr;
        }
    }

    if (PyDict_SetItemString(kwargs, "return_annotation", return_annotation) != 0) {
        Py_DECREF(return_annotation);
        Py_DECREF(kwargs);
        return nullptr;
    }
    Py_DECREF(return_annotation);

    PyObject *empty_args = PyTuple_New(0);
    if (!empty_args) {
        Py_DECREF(kwargs);
        return nullptr;
    }

    PyObject *result = PyObject_Call(g_inspect.signature, empty_args, kwargs);
    Py_DECREF(empty_args);
    Py_DECREF(kwargs);
    return result;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
