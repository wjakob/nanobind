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

#ifndef NB_INTROSPECT_SKIP_NB_SIG
#  define NB_INTROSPECT_SKIP_NB_SIG 1
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

namespace {

/* PEP 3107/362 helpers:
   - NB_INTROSPECT_SKIP_NB_SIG (default 1) controls whether overloads that
     used nb::sig skip PEP metadata (1) or are parsed like regular overloads (0).
   - Only merge overloads when parameter order, names, kinds, and sanitized
     tokens align; otherwise surface empty annotations and None for
     __signature__/__text_signature__.
   - For compatible overloads, annotations/return types are merged (Union when
     multiple concrete values, typing.Any when unspecified) and reused by all
     three exported attributes. */

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
        // Append annotation data for the active parameter when annotation
        // collection is enabled. Previously this required 'collecting' to be
        // true (set when a ':' token was encountered). However, type tokens
        // produced by '%' in the descriptor should always contribute to the
        // parameter annotation even if a ':' token wasn't present in the
        // descriptor. Relax the condition so that active/annotate is enough
        // to accept these type tokens.
        if (param.active && param.annotate)
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
                param.entry.has_annotation = false;
            else
                param.entry.has_annotation = true;
        } else {
            param.entry.annotation.clear();
            param.entry.has_annotation = false;
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

// Build a single overload's parameter/return description; skip when nb::sig is present.
static bool build_signature_metadata(const func_data *f,
                                     signature_metadata &out) noexcept {
#if NB_INTROSPECT_SKIP_NB_SIG
    if (f->flags & (uint32_t) func_flags::has_signature)
        return false;
#endif

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

static bool signatures_are_compatible(const std::vector<signature_metadata> &metas);

enum class metadata_state {
    skip,
    empty,
    incompatible,
    compatible
};

// Traverse all overloads, classify compatibility, and short-circuit on skip.
static metadata_state collect_signature_metadata(nb_func *func, const func_data *f,
                                                 std::vector<signature_metadata> &out) noexcept {
    Py_ssize_t overloads = Py_SIZE((PyObject *) func);
    if (overloads < 1)
        overloads = 1;

    out.clear();
    out.reserve((size_t) overloads);

    for (Py_ssize_t i = 0; i < overloads; ++i) {
        signature_metadata meta;
        if (!build_signature_metadata(f + i, meta))
            return metadata_state::skip;
        out.emplace_back(std::move(meta));
    }

    if (out.empty())
        return metadata_state::empty;

    if (!signatures_are_compatible(out))
        return metadata_state::incompatible;

    return metadata_state::compatible;
}

// Overloads are mergeable only when layout and tokens match exactly.
static bool signatures_are_compatible(const std::vector<signature_metadata> &metas) {
    if (metas.empty())
        return true;

    const signature_metadata &first = metas.front();
    for (size_t i = 1; i < metas.size(); ++i) {
        const signature_metadata &other = metas[i];
        if (first.parameters.size() != other.parameters.size())
            return false;

        for (size_t j = 0; j < first.parameters.size(); ++j) {
            const signature_param &a = first.parameters[j];
            const signature_param &b = other.parameters[j];
            if (a.name != b.name || a.kind != b.kind)
                return false;
        }

        if (first.sanitized_tokens.size() != other.sanitized_tokens.size())
            return false;

        for (size_t j = 0; j < first.sanitized_tokens.size(); ++j) {
            if (first.sanitized_tokens[j] != other.sanitized_tokens[j])
                return false;
        }
    }

    return true;
}

static void append_unique_annotation(std::vector<std::string> &values,
                                     const std::string &value) {
    if (value.empty())
        return;

    for (const auto &existing : values) {
        if (existing == value)
            return;
    }

    values.push_back(value);
}

static std::string merge_annotation_values(const std::vector<std::string> &values) {
    std::string result;
    if (values.empty())
        return result;

    for (const auto &entry : values) {
        if (entry == "typing.Any")
            return entry;
    }

    if (values.size() == 1)
        return values[0];

    result = "typing.Union[";
    for (size_t i = 0; i < values.size(); ++i) {
        result += values[i];
        if (i + 1 < values.size())
            result += ", ";
    }
    result += "]";
    return result;
}

static PyObject *build_annotation_dict(const std::vector<signature_metadata> &metas) {
    struct annotation_bucket {
        std::string name;
        std::vector<std::string> values;
    };

    std::vector<annotation_bucket> merged;
    std::vector<std::string> return_values;

    if (!metas.empty()) {
        merged.reserve(metas.front().parameters.size());
        return_values.reserve(metas.size());
        for (const auto &p : metas.front().parameters)
            merged.push_back(annotation_bucket{p.name, {}});
    }

    auto find_or_create = [&](const std::string &name) -> annotation_bucket & {
        for (auto &entry : merged) {
            if (entry.name == name)
                return entry;
        }
        merged.push_back(annotation_bucket{name, {}});
        return merged.back();
    };

    for (const auto &meta : metas) {
        for (const auto &entry : meta.parameters) {
            if (!entry.has_annotation)
                continue;
            annotation_bucket &slot = find_or_create(entry.name);
            append_unique_annotation(slot.values, entry.annotation);
        }

        if (!meta.return_type.empty())
            append_unique_annotation(return_values, meta.return_type);
    }

    PyObject *annotations = PyDict_New();
    if (!annotations)
        return nullptr;

    for (const auto &entry : merged) {
        std::string merged_value = merge_annotation_values(entry.values);
        if (merged_value.empty())
            merged_value = "typing.Any";
        PyObject *value = PyUnicode_FromString(merged_value.c_str());
        if (!value ||
            PyDict_SetItemString(annotations, entry.name.c_str(), value) < 0) {
            Py_XDECREF(value);
            Py_DECREF(annotations);
            return nullptr;
        }
        Py_DECREF(value);
    }

    std::string merged_return = merge_annotation_values(return_values);
    if (!merged_return.empty()) {
        PyObject *ret = PyUnicode_FromString(merged_return.c_str());
        if (!ret || PyDict_SetItemString(annotations, "return", ret) < 0) {
            Py_XDECREF(ret);
            Py_DECREF(annotations);
            return nullptr;
        }
        Py_DECREF(ret);
    }

    return annotations;
}

// Render the compact text signature string from sanitized tokens.
static PyObject *build_text_signature_from_meta(const signature_metadata &meta) {
    std::string signature = "(";
    for (size_t i = 0; i < meta.sanitized_tokens.size(); ++i) {
        signature += meta.sanitized_tokens[i];
        if (i + 1 < meta.sanitized_tokens.size())
            signature += ", ";
    }
    signature += ")";

    return PyUnicode_FromString(signature.c_str());
}

// Materialize inspect.Signature using cached inspect objects.
static PyObject *build_signature_object(const signature_metadata &meta) {
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
        if (PyTuple_SetItem(args, 0, name) != 0) {
            Py_DECREF(kind);
            Py_DECREF(args);
            Py_DECREF(param_list);
            return nullptr;
        }
        if (PyTuple_SetItem(args, 1, kind) != 0) {
            Py_DECREF(kind);
            Py_DECREF(args);
            Py_DECREF(param_list);
            return nullptr;
        }

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
            if (!ann ||
                PyDict_SetItemString(kwargs, "annotation", ann) != 0) {
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
        if (PyList_SetItem(param_list, i, param_obj) != 0) {
            Py_DECREF(param_list);
            return nullptr;
        }
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

static PyObject *return_none() noexcept {
    Py_INCREF(Py_None);
    return Py_None;
}

// __annotations__: empty dict on skip/incompat, merged dict otherwise.
PyObject *nb_introspect_annotations(nb_func *func, const func_data *f) noexcept {
    std::vector<signature_metadata> metas;
    switch (collect_signature_metadata(func, f, metas)) {
        case metadata_state::skip:
        case metadata_state::empty:
        case metadata_state::incompatible:
            return PyDict_New();
        case metadata_state::compatible:
            return build_annotation_dict(metas);
    }
    return PyDict_New();
}

// __text_signature__: AttributeError on skip/incompat, compact text otherwise.
PyObject *nb_introspect_text_signature(nb_func *func, const func_data *f) noexcept {
    std::vector<signature_metadata> metas;
    switch (collect_signature_metadata(func, f, metas)) {
        case metadata_state::skip:
        case metadata_state::empty:
        case metadata_state::incompatible:
            PyErr_SetString(PyExc_AttributeError, "__text_signature__");
            return nullptr;
        case metadata_state::compatible:
            return build_text_signature_from_meta(metas.front());
    }
    return return_none();
}

// __signature__: AttributeError on skip/incompat, inspect.Signature otherwise.
PyObject *nb_introspect_signature(nb_func *func, const func_data *f) noexcept {
    std::vector<signature_metadata> metas;
    switch (collect_signature_metadata(func, f, metas)) {
        case metadata_state::skip:
        case metadata_state::empty:
        case metadata_state::incompatible:
            PyErr_SetString(PyExc_AttributeError, "__signature__");
            return nullptr;
        case metadata_state::compatible:
            return build_signature_object(metas.front());
    }
    return return_none();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
