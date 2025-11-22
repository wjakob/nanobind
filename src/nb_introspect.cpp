#include "nb_introspect.h"
#include "nb_internals.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

#ifndef NB_INTROSPECT_SKIP_NB_SIG
#  define NB_INTROSPECT_SKIP_NB_SIG 1
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

namespace {

// Helper that loads inspect module objects on-demand (performance is secondary).
struct inspect_handles {
    PyObject *module = nullptr;
    PyObject *parameter = nullptr;
    PyObject *signature = nullptr;
    PyObject *empty = nullptr;
    PyObject *kinds[5] = {nullptr};

    ~inspect_handles() {
        Py_XDECREF(parameter);
        Py_XDECREF(signature);
        Py_XDECREF(empty);
        for (PyObject *&kind : kinds)
            Py_XDECREF(kind);
        Py_XDECREF(module);
    }

    bool init() {
        module = PyImport_ImportModule("inspect");
        if (!module)
            return false;

        parameter = PyObject_GetAttrString(module, "Parameter");
        signature = PyObject_GetAttrString(module, "Signature");
        empty = PyObject_GetAttrString(module, "_empty");
        if (!parameter || !signature || !empty)
            return false;

        const char *names[5] = {
            "POSITIONAL_ONLY", "POSITIONAL_OR_KEYWORD", "KEYWORD_ONLY",
            "VAR_POSITIONAL", "VAR_KEYWORD"
        };
        for (size_t i = 0; i < 5; ++i) {
            kinds[i] = PyObject_GetAttrString(parameter, names[i]);
            if (!kinds[i])
                return false;
        }

        return true;
    }
};

enum class param_kind { pos_only = 0, pos_or_kw, kw_only, var_pos, var_kw };

struct sig_param {
    std::string name, annotation;
    param_kind kind;
    PyObject *def_val = nullptr;
    bool has_anno = false;

    /// Checks equality for overload merging
    bool operator==(const sig_param &o) const {
        return name == o.name && kind == o.kind;
    }
};

struct sig_meta {
    std::vector<sig_param> params;
    std::vector<std::string> tokens;
    std::string ret_type;
};

/**
 * @brief Demangles C++ type name and strips keywords (class/struct) for cleaner display.
 * @param t The type_info to demangle.
 * @return formatted std::string.
 */
static std::string type_name_local(const std::type_info *t) {
    const char *name_in = t->name();
#if defined(__GNUG__)
    int status = 0;
    char *demangled = abi::__cxa_demangle(name_in, nullptr, nullptr, &status);
    std::string res = demangled ? demangled : name_in;
    free(demangled);
#else
    std::string res = name_in;
    for (const char *sub : {"class ", "struct ", "enum "}) {
        size_t p = 0, len = strlen(sub);
        while ((p = res.find(sub, p)) != std::string::npos) res.erase(p, len);
    }
#endif
    return res;
}

/**
 * @brief Trims whitespace from the ends of a string in-place.
 */
static void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

} // namespace

/**
 * @brief Parses nanobind function descriptor to build signature metadata.
 * @param f Function data pointer.
 * @param out Output metadata structure.
 * @return true if successful and should be used, false if skipped.
 */
static bool build_meta(const func_data *f, sig_meta &out) noexcept {
#if NB_INTROSPECT_SKIP_NB_SIG
    if (f->flags & (uint32_t) func_flags::has_signature) return false;
#endif

    bool is_method = f->flags & (uint32_t) func_flags::is_method;
    bool has_args = f->flags & (uint32_t) func_flags::has_args;
    bool has_vargs = f->flags & (uint32_t) func_flags::has_var_args;
    bool has_vkwargs = f->flags & (uint32_t) func_flags::has_var_kwargs;

    out.params.clear(); out.tokens.clear(); out.ret_type.clear();
    
    const std::type_info **dtypes = f->descr_types;
    uint32_t arg_idx = 0;
    bool capturing_ret = false;
    bool kw_section = false;

    // State for the parameter currently being parsed
    struct {
        sig_param p;
        std::string sanitized;
        bool active = false, annotate = false, collect_anno = false, opt_suffix = false;
    } curr;

    auto finish_param = [&]() {
        if (!curr.active) return;
        // Append default value representation if available
        if (curr.p.def_val) {
            const char *sig = curr.p.def_val ? f->args[arg_idx].signature : nullptr;
            curr.sanitized += " = ";
            if (sig) curr.sanitized += sig;
            else {
                PyObject *r = PyObject_Repr(curr.p.def_val);
                Py_ssize_t sz; const char *s = PyUnicode_AsUTF8AndSize(r, &sz);
                if (s) curr.sanitized.append(s, sz);
                Py_XDECREF(r);
            }
        }
        // Handle Optional[] suffix for Python < 3.10 or | None for >= 3.10 logic
        // Simplified here to just use Union/Optional logic or suffixes
#if PY_VERSION_HEX >= 0x030A0000
        if (curr.annotate && curr.opt_suffix) curr.p.annotation += " | None";
#else
        if (curr.opt_suffix) curr.p.annotation += "]"; // Assumes Optional[ was prepended
#endif
        trim(curr.p.annotation);
        curr.p.has_anno = !curr.p.annotation.empty();
        out.params.push_back(std::move(curr.p));
        if (!curr.sanitized.empty()) out.tokens.push_back(std::move(curr.sanitized));
        curr = {}; // reset
    };

    for (const char *pc = f->descr; *pc; ++pc) {
        switch (*pc) {
            case '@': {
                ++pc;
                if (!capturing_ret) {
                    while (*pc && *pc != '@') {
                        if (curr.active && curr.annotate)
                            curr.p.annotation.push_back(*pc);
                        ++pc;
                    }
                    if (*pc == '@')
                        ++pc;
                    while (*pc && *pc != '@')
                        ++pc;
                } else {
                    while (*pc && *pc != '@')
                        ++pc;
                    if (*pc == '@')
                        ++pc;
                    while (*pc && *pc != '@') {
                        out.ret_type.push_back(*pc);
                        ++pc;
                    }
                }
                continue;
            }
            case '{': {
                const char *aname_ptr = has_args ? f->args[arg_idx].name : nullptr;
                std::string name_val; // Renamed to avoid collision with 'name' from earlier context

                // Check for **kwargs
                if (has_vkwargs && arg_idx + 1 == f->nargs) {
                    name_val = aname_ptr ? aname_ptr : "kwargs";
                    out.params.push_back({name_val, "typing.Dict[str, typing.Any]", param_kind::var_kw, nullptr, true});
                    out.tokens.push_back("**" + name_val);
                    pc += 4; break;
                }
                
                // Check for *args or end of positional args
                if (arg_idx == f->nargs_pos) {
                    if (has_vargs) {
                        name_val = aname_ptr ? aname_ptr : "args";
                        out.params.push_back({name_val, "typing.Tuple[typing.Any, ...]", param_kind::var_pos, nullptr, true});
                        out.tokens.push_back("*" + name_val);
                        kw_section = true; pc += 5; break;
                    }
                    out.tokens.emplace_back("*");
                    kw_section = true;
                }

                // Handle 'self'
                if (is_method && arg_idx == 0) {
                    out.tokens.emplace_back("self");
                    out.params.push_back({"self", "", param_kind::pos_only, nullptr, false});
                    while (*pc != '}') { if (*pc == '%') dtypes++; pc++; }
                    arg_idx++; break;
                }

                // Regular Argument
                if (aname_ptr) {
                    name_val = aname_ptr;
                } else {
                    name_val = "arg";
                    // Only append index if more than one argument or is_method is true (for self)
                    // The original code was `if (f->nargs > 1 + (uint32_t) is_method)`
                    // which means it appends index if there are more than 1 'effective' argument.
                    // effective_arg_idx is already the index relative to non-self arguments.
                    // So, if (f->nargs > 1 && !is_method) || (f->nargs > 2 && is_method)
                    // This is equivalent to checking if effective_arg_idx is > 0 OR
                    // if there are multiple arguments (f->nargs - (uint32_t)is_method) > 1
                    if ((f->nargs - (uint32_t) is_method) > 1 ) {
                       name_val += std::to_string(arg_idx - is_method);
                    }
                }
                curr.active = true; curr.annotate = true; curr.p.name = name_val;
                curr.p.kind = kw_section ? param_kind::kw_only : (!has_args ? param_kind::pos_only : param_kind::pos_or_kw);
                curr.p.def_val = has_args ? f->args[arg_idx].value : nullptr;
                curr.sanitized = name_val;
                
                if (has_args && (f->args[arg_idx].flag & (uint8_t) cast_flags::accepts_none)) {
#if PY_VERSION_HEX < 0x030A0000
                    curr.p.annotation += "typing.Optional[";
#endif
                    curr.opt_suffix = true;
                }
                break;
            }
            case '}': 
                finish_param(); 
                if (++arg_idx == f->nargs_pos && !has_args) out.tokens.emplace_back("/");
                break;
            case '%': {
                std::string type_str;
                auto it = internals->type_c2p_slow.find(*dtypes);
                if (it != internals->type_c2p_slow.end()) {
                    handle th((PyObject *)it->second->type_py);
                    type_str = std::string(borrow<str>(th.attr("__module__")).c_str());
                    type_str += ".";
                    type_str += std::string(borrow<str>(th.attr("__qualname__")).c_str());
                } else {
                    type_str = type_name_local(*dtypes);
                }
                
                if (capturing_ret) out.ret_type += type_str;
                else if (curr.active && curr.annotate) curr.p.annotation += type_str;
                dtypes++;
                break;
            }
            case '-':
                if (pc[1] == '>') { capturing_ret = true; pc++; }
                else if (curr.active && curr.annotate) curr.p.annotation += *pc;
                break;
            case ':':
                if (curr.active) curr.collect_anno = true; // fallthrough
            default:
                if (capturing_ret) out.ret_type += *pc;
                else if (curr.active && curr.annotate) curr.p.annotation += *pc;

        }
    }
    trim(out.ret_type);
    return true;
}

/**
 * @brief Collects and merges metadata for all overloads.
 * @param func The nanobind function object.
 * @param f The function data array.
 * @return A merged dict (annotations) or specific error/empty states.
 */
static std::vector<sig_meta> collect_metas(nb_func *func, const func_data *f) {
    std::vector<sig_meta> metas;
    Py_ssize_t count = Py_SIZE((PyObject *) func);
    if (count < 1) count = 1;

    for (Py_ssize_t i = 0; i < count; ++i) {
        sig_meta m;
        if (build_meta(f + i, m)) metas.push_back(std::move(m));
        else return {}; // Skip detected
    }
    
    // Check compatibility: params must match exactly, tokens must match
    if (!metas.empty()) {
        const auto &base = metas[0];
        for (size_t i = 1; i < metas.size(); ++i) {
            if (metas[i].params != base.params || metas[i].tokens != base.tokens) return {}; 
        }
    }
    return metas;
}

/**
 * @brief Constructs the __annotations__ dictionary.
 */
PyObject *nb_introspect_annotations(nb_func *func, const func_data *f) noexcept {
    auto metas = collect_metas(func, f);
    if (metas.empty()) return PyDict_New();

    PyObject *dict = PyDict_New();
    if (!dict) return nullptr;

    auto merge_vals = [](const std::vector<std::string> &vals) -> std::string {
        if (vals.empty()) return "";
        if (std::find(vals.begin(), vals.end(), "typing.Any") != vals.end()) return "typing.Any";
        if (vals.size() == 1) return vals[0];
        std::string res = "typing.Union[";
        for (size_t i = 0; i < vals.size(); ++i) res += (i ? ", " : "") + vals[i];
        return res + "]";
    };

    // Merge logic: iterate params of first overload (structure is identical)
    for (size_t i = 0; i < metas[0].params.size(); ++i) {
        const auto &p_base = metas[0].params[i];

        
        std::vector<std::string> distinct;
        for (const auto &m : metas) {
            const auto &val = m.params[i].annotation;
            if (!val.empty() && std::find(distinct.begin(), distinct.end(), val) == distinct.end())
                distinct.push_back(val);
        }
        
        std::string merged = merge_vals(distinct);
        if (merged.empty()) merged = "typing.Any";
        
        PyObject *s = PyUnicode_FromString(merged.c_str());
        if (s) { PyDict_SetItemString(dict, p_base.name.c_str(), s); Py_DECREF(s); }
    }

    // Merge return type
    std::vector<std::string> distinct_rets;
    for (const auto &m : metas) {
        if (!m.ret_type.empty() && std::find(distinct_rets.begin(), distinct_rets.end(), m.ret_type) == distinct_rets.end())
            distinct_rets.push_back(m.ret_type);
    }
    std::string ret = merge_vals(distinct_rets);
    if (!ret.empty()) {
        PyObject *s = PyUnicode_FromString(ret.c_str());
        if (s) { PyDict_SetItemString(dict, "return", s); Py_DECREF(s); }
    }
    return dict;
}

/**
 * @brief Constructs the __text_signature__ string.
 */
PyObject *nb_introspect_text_signature(nb_func *func, const func_data *f) noexcept {
    auto metas = collect_metas(func, f);
    if (metas.empty()) {
        PyErr_SetString(PyExc_AttributeError, "__text_signature__");
        return nullptr;
    }
    std::string sig = "(";
    const auto &toks = metas[0].tokens;
    for (size_t i = 0; i < toks.size(); ++i) sig += (i ? ", " : "") + toks[i];
    sig += ")";
    return PyUnicode_FromString(sig.c_str());
}

/**
 * @brief Constructs the inspect.Signature object.
 */
PyObject *nb_introspect_signature(nb_func *func, const func_data *f) noexcept {
    auto metas = collect_metas(func, f);
    if (metas.empty()) {
        PyErr_SetString(PyExc_AttributeError, "__signature__");
        return nullptr;
    }

    inspect_handles handles;
    if (!handles.init())
        return nullptr;

    const auto &meta = metas[0];
    PyObject *plist = PyList_New(meta.params.size());
    if (!plist) return nullptr;

    for (size_t i = 0; i < meta.params.size(); ++i) {
        const auto &p = meta.params[i];
        PyObject *kwargs = PyDict_New();
        if (!kwargs) {
            Py_DECREF(plist);
            return nullptr;
        }
        PyObject *name = PyUnicode_FromString(p.name.c_str());
        if (!name) {
            Py_DECREF(kwargs);
            Py_DECREF(plist);
            return nullptr;
        }
        PyObject *kind = handles.kinds[(int) p.kind];

        if (p.def_val) PyDict_SetItemString(kwargs, "default", p.def_val);
        if (p.has_anno && !p.annotation.empty()) {
             PyObject *a = PyUnicode_FromString(p.annotation.c_str());
             if (a) { PyDict_SetItemString(kwargs, "annotation", a); Py_DECREF(a); }
             else {
                 Py_DECREF(name);
                 Py_DECREF(kwargs);
                 Py_DECREF(plist);
                 return nullptr;
             }
        }

        PyObject *args = PyTuple_Pack(2, name, kind);
        Py_DECREF(name);
        if (!args) {
            Py_DECREF(kwargs);
            Py_DECREF(plist);
            return nullptr;
        }
        PyObject *param_obj = PyObject_Call(handles.parameter, args, kwargs);

        PyList_SetItem(plist, i, param_obj); // Steals ref to param_obj
        Py_DECREF(args);
        Py_DECREF(kwargs);
        if (!param_obj) {
            Py_DECREF(plist);
            return nullptr;
        }
    }

    PyObject *sig_kwargs = PyDict_New();
    PyObject *params_tuple = PyList_AsTuple(plist);
    if (!sig_kwargs || !params_tuple) {
        Py_XDECREF(sig_kwargs);
        Py_XDECREF(params_tuple);
        Py_DECREF(plist);
        return nullptr;
    }
    if (PyDict_SetItemString(sig_kwargs, "parameters", params_tuple) != 0) {
        Py_DECREF(params_tuple);
        Py_DECREF(plist);
        Py_DECREF(sig_kwargs);
        return nullptr;
    }
    
    if (!meta.ret_type.empty()) {
        PyObject *ret = PyUnicode_FromString(meta.ret_type.c_str());
        if (!ret || PyDict_SetItemString(sig_kwargs, "return_annotation", ret) != 0) {
            Py_XDECREF(ret);
            Py_DECREF(params_tuple);
            Py_DECREF(plist);
            Py_DECREF(sig_kwargs);
            return nullptr;
        }
        Py_DECREF(ret);
    } else {
        if (PyDict_SetItemString(sig_kwargs, "return_annotation", handles.empty) != 0) {
            Py_DECREF(params_tuple);
            Py_DECREF(plist);
            Py_DECREF(sig_kwargs);
            return nullptr;
        }
    }

    PyObject *empty = PyTuple_New(0);
    if (!empty) {
        Py_DECREF(params_tuple);
        Py_DECREF(plist);
        Py_DECREF(sig_kwargs);
        return nullptr;
    }
    PyObject *result = PyObject_Call(handles.signature, empty, sig_kwargs);
    
    Py_DECREF(plist);
    Py_DECREF(params_tuple);
    Py_DECREF(sig_kwargs);
    Py_DECREF(empty);
    return result;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
