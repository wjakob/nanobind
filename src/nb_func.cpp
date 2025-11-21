/*
    src/nb_func.cpp: nanobind function type

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include "nb_internals.h"
#include "buffer.h"
#include "nb_ft.h"
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

/** The helpers below translate nanobind's metadata into the standard Python
    introspection hooks mandated by PEP 3107 (``__annotations__``) and PEP 362
    (``__text_signature__``). Native nanobind callables lack those fields, so we
    derive them mechanically from the C++ binding information. */

/// Maximum number of arguments supported by 'nb_vectorcall_simple'
#define NB_MAXARGS_SIMPLE 8

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

#if defined(_MSC_VER)
#  pragma warning(disable: 4706) // assignment within conditional expression
#  pragma warning(disable: 6255) // _alloca indicates failure by raising a stack overflow exception
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Forward/external declarations
extern Buffer buf;

static PyObject *nb_func_vectorcall_simple_0(PyObject *, PyObject *const *,
                                             size_t, PyObject *) noexcept;
static PyObject *nb_func_vectorcall_simple_1(PyObject *, PyObject *const *,
                                             size_t, PyObject *) noexcept;
static PyObject *nb_func_vectorcall_simple(PyObject *, PyObject *const *,
                                           size_t, PyObject *) noexcept;
static PyObject *nb_func_vectorcall_complex(PyObject *, PyObject *const *,
                                            size_t, PyObject *) noexcept;

struct signature_capture_entry {
    std::string name;
    std::string type;
};

struct signature_capture {
    std::vector<signature_capture_entry> parameters;
    std::string current_name;
    std::string current_type;
    std::string return_type;
    bool capturing_param = false;
    bool capturing_return = false;
};

/** 
 * ``find_top_level``/``trim_view`` are tiny parsing utilities that work on the
 * already rendered docstring signature. They purposely understand just enough
 * syntax to keep ``__text_signature__`` compliant with CPython's parser. 
 */
static size_t find_top_level(std::string_view text, char target) {
    int depth = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '[' || ch == '(' || ch == '{') {
            depth++;
        } else if (ch == ']' || ch == ')' || ch == '}') {
            if (depth > 0)
                depth--;
        } else if (ch == target && depth == 0) {
            return i;
        }
    }
    return std::string_view::npos;
}

static std::string_view trim_view(std::string_view text) {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && std::isspace(static_cast<unsigned char>(text[start])))
        start++;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
        end--;
    return text.substr(start, end - start);
}

/** The ``capture_*`` helpers are used by ``nb_func_render_signature`` so that we
    do not have to re-render the signature when populating the PEP metadata. */
static void capture_append(signature_capture *capture, const char *data,
                           size_t size) {
    if (!capture)
        return;

    std::string *target = nullptr;
    if (capture->capturing_param)
        target = &capture->current_type;
    else if (capture->capturing_return)
        target = &capture->return_type;

    if (target)
        target->append(data, size);
}

static void capture_append(signature_capture *capture, const char *data) {
    if (data)
        capture_append(capture, data, strlen(data));
}

static void capture_append(signature_capture *capture, char c) {
    capture_append(capture, &c, 1);
}

static void capture_start_param(signature_capture *capture,
                                std::string name) {
    if (!capture)
        return;
    capture->capturing_param = true;
    capture->current_name = std::move(name);
    capture->current_type.clear();
}

static void capture_finish_param(signature_capture *capture) {
    if (!capture || !capture->capturing_param)
        return;

    if (capture->current_type.empty())
        capture->current_type = "typing.Any";

    capture->parameters.push_back({capture->current_name, capture->current_type});
    capture->capturing_param = false;
    capture->current_name.clear();
    capture->current_type.clear();
}

/** Render a single overload's signature. When ``capture`` is provided it also
    records the pieces needed to build the PEP 3107/362 metadata later on. */
static uint32_t nb_func_render_signature(const func_data *f,
                                         bool nb_signature_mode = false,
                                         signature_capture *capture = nullptr) noexcept;
/** Populate ``__annotations__`` and ``__text_signature__`` on the descriptor so
    nanobind callables satisfy the expectations of PEP 3107/362 tooling. */
static bool nb_func_apply_introspection(nb_func *func, const func_data *f);

int nb_func_traverse(PyObject *self, visitproc visit, void *arg) {
    size_t size = (size_t) Py_SIZE(self);

    if (size) {
        func_data *f = nb_func_data(self);

        for (size_t i = 0; i < size; ++i) {
            if (f->flags & (uint32_t) func_flags::has_args) {
                for (size_t j = 0; j < f->nargs; ++j) {
                    Py_VISIT(f->args[j].value);
                }
            }
            ++f;
        }
    }

    return 0;
}

int nb_func_clear(PyObject *self) {
    size_t size = (size_t) Py_SIZE(self);

    if (size) {
        func_data *f = nb_func_data(self);

        for (size_t i = 0; i < size; ++i) {
            if (f->flags & (uint32_t) func_flags::has_args) {
                for (size_t j = 0; j < f->nargs; ++j) {
                    Py_CLEAR(f->args[j].value);
                }
            }
            ++f;
        }
    }

    return 0;
}

/// Free a function overload chain
void nb_func_dealloc(PyObject *self) {
    PyObject_GC_UnTrack(self);

    size_t size = (size_t) Py_SIZE(self);
    if (size) {
        func_data *f = nb_func_data(self);

        // Delete from registered function list
#if !defined(NB_FREE_THREADED)
        size_t n_deleted = internals->funcs.erase(self);
        check(n_deleted == 1,
              "nanobind::detail::nb_func_dealloc(\"%s\"): function not found!",
              ((f->flags & (uint32_t) func_flags::has_name) ? f->name
                                                        : "<anonymous>"));
#endif

        for (size_t i = 0; i < size; ++i) {
            if (f->flags & (uint32_t) func_flags::has_free)
                f->free_capture(f->capture);

            if (f->flags & (uint32_t) func_flags::has_args) {
                for (size_t j = 0; j < f->nargs; ++j) {
                    const arg_data &arg = f->args[j];
                    Py_XDECREF(arg.value);
                    Py_XDECREF(arg.name_py);
                    free((char *) arg.signature);
                }
            }

            if (f->flags & (uint32_t) func_flags::has_doc)
                free((char *) f->doc);

            free((char *) f->name);
            free(f->args);
            free((char *) f->descr);
            free(f->descr_types);
            free(f->signature);
            ++f;
        }
    }

    nb_func *nf = (nb_func *) self;
    // ``annotations`` and ``text_signature`` are borrowed frequently via
    // ``__getattr__``. Manage their lifetime here so cached references remain
    // valid and leak-free.
    Py_XDECREF(nf->annotations);
    Py_XDECREF(nf->text_signature);
    PyObject_GC_Del(self);
}

int nb_bound_method_traverse(PyObject *self, visitproc visit, void *arg) {
    nb_bound_method *mb = (nb_bound_method *) self;
    Py_VISIT((PyObject *) mb->func);
    Py_VISIT(mb->self);
    return 0;
}

int nb_bound_method_clear(PyObject *self) {
    nb_bound_method *mb = (nb_bound_method *) self;
    Py_CLEAR(mb->func);
    Py_CLEAR(mb->self);
    return 0;
}

void nb_bound_method_dealloc(PyObject *self) {
    nb_bound_method *mb = (nb_bound_method *) self;
    PyObject_GC_UnTrack(self);
    Py_DECREF((PyObject *) mb->func);
    Py_DECREF(mb->self);
    PyObject_GC_Del(self);
}

static arg_data method_args[2] = {
    { "self", nullptr, nullptr, nullptr, 0 },
    { nullptr, nullptr, nullptr, nullptr, 0 }
};

static bool set_builtin_exception_status(builtin_exception &e) {
    PyObject *o;

    switch (e.type()) {
        case exception_type::runtime_error: o = PyExc_RuntimeError; break;
        case exception_type::stop_iteration: o = PyExc_StopIteration; break;
        case exception_type::index_error: o = PyExc_IndexError; break;
        case exception_type::key_error: o = PyExc_KeyError; break;
        case exception_type::value_error: o = PyExc_ValueError; break;
        case exception_type::type_error: o = PyExc_TypeError; break;
        case exception_type::buffer_error: o = PyExc_BufferError; break;
        case exception_type::import_error: o = PyExc_ImportError; break;
        case exception_type::attribute_error: o = PyExc_AttributeError; break;
        case exception_type::next_overload: return false;
        default:
            check(false, "nanobind::detail::set_builtin_exception_status(): "
                         "invalid exception type!");
    }

    PyErr_SetString(o, e.what());
    return true;
}

void *malloc_check(size_t size) {
    void *ptr = malloc(size);
    if (!ptr)
        fail("nanobind: malloc() failed!");
    return ptr;
}

char *strdup_check(const char *s) {
    char *result;
    #if defined(_WIN32)
        result = _strdup(s);
    #else
        result = strdup(s);
    #endif
    if (!result)
        fail("nanobind: strdup() failed!");
    return result;
}

/**
 * \brief Wrap a C++ function into a Python function object
 *
 * This is an implementation detail of nanobind::cpp_function.
 */
PyObject *nb_func_new(const func_data_prelim_base *f) noexcept {
    bool has_scope       = f->flags & (uint32_t) func_flags::has_scope,
         has_name        = f->flags & (uint32_t) func_flags::has_name,
         has_args        = f->flags & (uint32_t) func_flags::has_args,
         has_var_args    = f->flags & (uint32_t) func_flags::has_var_args,
         has_var_kwargs  = f->flags & (uint32_t) func_flags::has_var_kwargs,
         can_mutate_args = f->flags & (uint32_t) func_flags::can_mutate_args,
         has_doc         = f->flags & (uint32_t) func_flags::has_doc,
         has_signature   = f->flags & (uint32_t) func_flags::has_signature,
         is_implicit     = f->flags & (uint32_t) func_flags::is_implicit,
         is_method       = f->flags & (uint32_t) func_flags::is_method,
         return_ref      = f->flags & (uint32_t) func_flags::return_ref,
         is_constructor  = false,
         is_init         = false,
         is_new          = false,
         is_setstate     = false;

    arg_data *args_in = nullptr;
    if (has_args)
        args_in = std::launder((arg_data*) ((func_data_prelim<1>*) f)->args);

    PyObject *name = nullptr;
    PyObject *func_prev = nullptr;

    char *name_cstr;
    if (has_signature) {
        name_cstr = extract_name("nanobind::detail::nb_func_new", "def ", f->name);
        has_name = *name_cstr != '\0';
    } else {
        name_cstr = strdup_check(has_name ? f->name : "");
    }

    // Check for previous overloads
    nb_internals *internals_ = internals;
    if (has_scope && has_name) {
        name = PyUnicode_InternFromString(name_cstr);
        check(name, "nb::detail::nb_func_new(\"%s\"): invalid name.", name_cstr);

        func_prev = PyObject_GetAttr(f->scope, name);
        if (func_prev) {
            if (Py_TYPE(func_prev) == internals_->nb_func ||
                Py_TYPE(func_prev) == internals_->nb_method) {
                func_data *fp = nb_func_data(func_prev);

                check((fp->flags & (uint32_t) func_flags::is_method) ==
                          (f->flags & (uint32_t) func_flags::is_method),
                      "nb::detail::nb_func_new(\"%s\"): mismatched static/"
                      "instance method flags in function overloads!",
                      name_cstr);

                /* Never append a method to an overload chain of a parent class;
                   instead, hide the parent's overloads in this case */
                if (fp->scope != f->scope)
                    Py_CLEAR(func_prev);
            } else if (name_cstr[0] == '_') {
                Py_CLEAR(func_prev);
            } else {
                check(false,
                      "nb::detail::nb_func_new(\"%s\"): cannot overload "
                      "existing non-function object of the same name!", name_cstr);
            }
        } else {
            PyErr_Clear();
        }

        is_init = strcmp(name_cstr, "__init__") == 0;
        is_new = strcmp(name_cstr, "__new__") == 0;
        is_setstate = strcmp(name_cstr, "__setstate__") == 0;

        // Is this method a constructor that takes a class binding as first parameter?
        is_constructor = is_method && (is_init || is_setstate) &&
                         strncmp(f->descr, "({%}", 4) == 0;

        // Don't use implicit conversions in copy constructors (causes infinite recursion)
        // Notes:
        //   f->nargs = C++ argument count.
        //   f->descr_types = zero-terminated array of bound types among them.
        //     Hence of size >= 2 for constructors, where f->descr_types[1] my be null.
        //   args_in = array of Python arguments (nb::arg). Non-empty if has_args.
        //   By contrast, fc->args below has size f->nargs.
        if (is_constructor && f->nargs == 2 && f->descr_types[0] &&
            f->descr_types[0] == f->descr_types[1]) {
            if (has_args) {
                args_in[0].flag &= ~(uint8_t) cast_flags::convert;
            } else {
                args_in = method_args + 1;
                has_args = true;
            }
        }
    }

    // Create a new function and destroy the old one
    Py_ssize_t prev_overloads = func_prev ? Py_SIZE(func_prev) : 0;
    nb_func *func = (nb_func *) PyType_GenericAlloc(
        is_method ? internals_->nb_method : internals_->nb_func, prev_overloads + 1);
    check(func, "nb::detail::nb_func_new(\"%s\"): alloc. failed (1).",
          name_cstr);

    make_immortal((PyObject *) func);
    func->annotations = nullptr;
    func->text_signature = nullptr;

    // Check if the complex dispatch loop is needed
    bool complex_call = can_mutate_args || has_var_kwargs || has_var_args ||
                        f->nargs > NB_MAXARGS_SIMPLE;

    if (has_args) {
        for (size_t i = is_method; i < f->nargs; ++i) {
            arg_data &a = args_in[i - is_method];
            complex_call |= a.name != nullptr || a.value != nullptr ||
                            a.flag != cast_flags::convert;
        }
    }

    uint32_t max_nargs = f->nargs;

    const char *prev_doc = nullptr;

    if (func_prev) {
        nb_func *nb_func_prev = (nb_func *) func_prev;
        complex_call |= nb_func_prev->complex_call;
        max_nargs = std::max(max_nargs, nb_func_prev->max_nargs);

        func_data *cur  = nb_func_data(func),
                  *prev = nb_func_data(func_prev);

        if (nb_func_prev->doc_uniform)
            prev_doc = prev->doc;

        memcpy(cur, prev, sizeof(func_data) * prev_overloads);
        memset(prev, 0, sizeof(func_data) * prev_overloads);

        ((PyVarObject *) func_prev)->ob_size = 0;

#if !defined(NB_FREE_THREADED)
        size_t n_deleted = internals_->funcs.erase(func_prev);
        check(n_deleted == 1,
              "nanobind::detail::nb_func_new(): internal update failed (1)!");
#endif

        Py_CLEAR(func_prev);
    }

    func->max_nargs = max_nargs;
    func->complex_call = complex_call;


    PyObject* (*vectorcall)(PyObject *, PyObject * const*, size_t, PyObject *);
    if (complex_call) {
        vectorcall = nb_func_vectorcall_complex;
    } else {
        if (f->nargs == 0 && !prev_overloads)
            vectorcall = nb_func_vectorcall_simple_0;
        else if (f->nargs == 1 && !prev_overloads)
            vectorcall = nb_func_vectorcall_simple_1;
        else
            vectorcall = nb_func_vectorcall_simple;
    }
    func->vectorcall = vectorcall;

#if !defined(NB_FREE_THREADED)
    // Register the function
    auto [it, success] = internals_->funcs.try_emplace(func, nullptr);
    check(success,
          "nanobind::detail::nb_func_new(): internal update failed (2)!");
#endif

    func_data *fc = nb_func_data(func) + prev_overloads;
    memcpy(fc, f, sizeof(func_data_prelim_base));
    if (has_doc) {
        if (fc->doc[0] == '\n')
            fc->doc++;
        if (fc->doc[0] == '\0') {
            fc->doc = nullptr;
            fc->flags &= ~(uint32_t) func_flags::has_doc;
            has_doc = false;
        } else {
            fc->doc = strdup_check(fc->doc);
        }
    }

    // Detect when an entire overload chain has the same docstring
    func->doc_uniform =
        (has_doc && ((prev_overloads == 0) ||
                     (prev_doc && strcmp(fc->doc, prev_doc) == 0)));

    if (is_constructor)
        fc->flags |= (uint32_t) func_flags::is_constructor;
    if (has_args)
        fc->flags |= (uint32_t) func_flags::has_args;

    fc->name = name_cstr;
    fc->signature = has_signature ? strdup_check(f->name) : nullptr;

    if (is_implicit) {
        check(fc->flags & (uint32_t) func_flags::is_constructor,
              "nb::detail::nb_func_new(\"%s\"): nanobind::is_implicit() "
              "should only be specified for constructors.",
              name_cstr);
        check(f->nargs == 2,
              "nb::detail::nb_func_new(\"%s\"): implicit constructors "
              "should only have one argument.",
              name_cstr);

        if (f->descr_types[1])
            implicitly_convertible(f->descr_types[1], f->descr_types[0]);
    }

    for (size_t i = 0;; ++i) {
        if (!f->descr[i]) {
            fc->descr = (char *) malloc_check(sizeof(char) * (i + 1));
            memcpy((char *) fc->descr, f->descr, (i + 1) * sizeof(char));
            break;
        }
    }

    for (size_t i = 0;; ++i) {
        if (!f->descr_types[i]) {
            fc->descr_types = (const std::type_info **)
                malloc_check(sizeof(const std::type_info *) * (i + 1));
            memcpy(fc->descr_types, f->descr_types,
                        (i + 1) * sizeof(const std::type_info *));
            break;
        }
    }

    if (has_args) {
        fc->args = (arg_data *) malloc_check(sizeof(arg_data) * f->nargs);

        if (is_method) // add implicit 'self' argument annotation
            fc->args[0] = method_args[0];
        for (size_t i = is_method; i < fc->nargs; ++i)
            fc->args[i] = args_in[i - is_method];

        for (size_t i = 0; i < fc->nargs; ++i) {
            arg_data &a = fc->args[i];
            if (a.name) {
                a.name_py = PyUnicode_InternFromString(a.name);
                a.name = PyUnicode_AsUTF8AndSize(a.name_py, nullptr);
            } else {
                a.name_py = nullptr;
            }
            if (a.value == Py_None)
                a.flag |= (uint8_t) cast_flags::accepts_none;
            a.signature = a.signature ? strdup_check(a.signature) : nullptr;
            Py_XINCREF(a.value);
        }
    }

    // Fast path for vector call object construction
    if (((is_init && is_method) || (is_new && !is_method)) &&
        nb_type_check(f->scope)) {
        type_data *td = nb_type_data((PyTypeObject *) f->scope);
        bool has_new = td->flags & (uint32_t) type_flags::has_new;

        if (is_init) {
            if (!has_new) {
                td->init = func;
            } else {
                // Keep track of whether we have a __init__ overload that
                // accepts no arguments (except self). If not, then we
                // shouldn't allow calling the type object with no arguments,
                // even though (for unpickling support) we probably do have
                // a __new__ overload that accepts no arguments (except cls).
                // This check is necessary because our type vectorcall shortcut
                // skips Python's usual logic where __init__ is always called
                // if __new__ returns an instance of the type.
                bool noargs_ok = true;
                for (uint32_t i = 1; i < fc->nargs - (uint32_t) has_var_kwargs; ++i) {
                    if (has_var_args && i == fc->nargs_pos)
                        continue; // skip `nb::args` since it can be empty
                    if (has_args && fc->args[i].value != nullptr)
                        continue; // arg with default is OK
                    noargs_ok = false;
                    break;
                }
                if (noargs_ok)
                    td->flags |= (uint32_t) type_flags::has_nullary_new;
            }
        } else if (is_new) {
            td->init = func;
            td->flags |= (uint32_t) type_flags::has_new;
        }
    }

    if (has_scope && name) {
        int rv = PyObject_SetAttr(f->scope, name, (PyObject *) func);
        check(rv == 0, "nb::detail::nb_func_new(\"%s\"): setattr. failed.",
              name_cstr);
    }

    Py_XDECREF(name);

    // Populate ``__annotations__`` / ``__text_signature__`` once so every
    // descriptor instance exposes the data expected by PEP 3107/362 consumers.
    if (!nb_func_apply_introspection(func, nb_func_data(func)))
        PyErr_Clear();

    if (return_ref) {
        return (PyObject *) func;
    } else {
        Py_DECREF(func);
        return nullptr;
    }
}

/// Used by nb_func_vectorcall: generate an error when overload resolution fails
static NB_NOINLINE PyObject *
nb_func_error_overload(PyObject *self, PyObject *const *args_in,
                       size_t nargs_in, PyObject *kwargs_in) noexcept {
    uint32_t count = (uint32_t) Py_SIZE(self);
    func_data *f = nb_func_data(self);

    if (f->flags & (uint32_t) func_flags::is_operator)
        return not_implemented().release().ptr();

    // The buffer 'buf' is protected by 'internals.mutex'
    lock_internals guard(internals);

    buf.clear();
    buf.put_dstr(f->name);
    buf.put("(): incompatible function arguments. The following argument types "
            "are supported:\n");

    // Mask default __new__ overload created by nb::new_()
    if (strcmp(f->name, "__new__") == 0 && count > 1 && f->nargs == 1) {
        count -= 1;
        f += 1;
    }

    for (uint32_t i = 0; i < count; ++i) {
        buf.put("    ");
        buf.put_uint32(i + 1);
        buf.put(". ");
        nb_func_render_signature(f + i);
        buf.put('\n');
    }

    buf.put("\nInvoked with types: ");
    for (size_t i = 0; i < nargs_in; ++i) {
        str name = steal<str>(nb_inst_name(args_in[i]));
        buf.put_dstr(name.c_str());
        if (i + 1 < nargs_in)
            buf.put(", ");
    }

    if (kwargs_in) {
        if (nargs_in)
            buf.put(", ");
        buf.put("kwargs = { ");

        size_t nkwargs_in = (size_t) NB_TUPLE_GET_SIZE(kwargs_in);
        for (size_t j = 0; j < nkwargs_in; ++j) {
            PyObject *key   = NB_TUPLE_GET_ITEM(kwargs_in, j),
                     *value = args_in[nargs_in + j];

            const char *key_cstr = PyUnicode_AsUTF8AndSize(key, nullptr);
            buf.put_dstr(key_cstr);
            buf.put(": ");
            str name = steal<str>(nb_inst_name(value));
            buf.put_dstr(name.c_str());
            buf.put(", ");
        }
        buf.rewind(2);
        buf.put(" }");
    }

    PyErr_SetString(PyExc_TypeError, buf.get());
    return nullptr;
}

/// Used by nb_func_vectorcall: generate an error when result conversion fails
static NB_NOINLINE PyObject *nb_func_error_noconvert(PyObject *self,
                                                     PyObject *const *, size_t,
                                                     PyObject *) noexcept {
    if (PyErr_Occurred())
        return nullptr;
    func_data *f = nb_func_data(self);

    // The buffer 'buf' is protected by 'internals.mutex'
    lock_internals guard(internals);

    buf.clear();
    buf.put("Unable to convert function return value to a Python "
            "type! The signature was\n    ");
    nb_func_render_signature(f);
    PyErr_SetString(PyExc_TypeError, buf.get());
    return nullptr;
}

/// Used by nb_func_vectorcall: convert a C++ exception into a Python error
static NB_NOINLINE void nb_func_convert_cpp_exception() noexcept {
    std::exception_ptr e = std::current_exception();

    for (nb_translator_seq *cur = &internals->translators; cur;
         cur = cur->next) {
        try {
            // Try exception translator & forward payload
            cur->translator(e, cur->payload);
            return;
        } catch (...) {
            e = std::current_exception();
        }
    }

    PyErr_SetString(PyExc_SystemError,
                    "nanobind::detail::nb_func_error_except(): exception "
                    "could not be translated!");
}

/// Dispatch loop that is used to invoke functions created by nb_func_new
static PyObject *nb_func_vectorcall_complex(PyObject *self,
                                            PyObject *const *args_in,
                                            size_t nargsf,
                                            PyObject *kwargs_in) noexcept {
    const size_t count      = (size_t) Py_SIZE(self),
                 nargs_in   = (size_t) NB_VECTORCALL_NARGS(nargsf),
                 nkwargs_in = kwargs_in ? (size_t) NB_TUPLE_GET_SIZE(kwargs_in) : 0;

    func_data *fr = nb_func_data(self);

    const bool is_method      = fr->flags & (uint32_t) func_flags::is_method,
               is_constructor = fr->flags & (uint32_t) func_flags::is_constructor;

    PyObject *result = nullptr,
             *self_arg = (is_method && nargs_in > 0) ? args_in[0] : nullptr;

    /* The following lines allocate memory on the stack, which is very efficient
       but also potentially dangerous since it can be used to generate stack
       overflows. We refuse unrealistically large number of 'kwargs' (the
       'max_nargs' value is fine since it is specified by the bindings) */
    if (nkwargs_in > 1024) {
        PyErr_SetString(PyExc_TypeError,
                        "nanobind::detail::nb_func_vectorcall(): too many (> "
                        "1024) keyword arguments.");
        return nullptr;
    }

    // Handler routine that will be invoked in case of an error condition
    PyObject *(*error_handler)(PyObject *, PyObject *const *, size_t,
                               PyObject *) noexcept = nullptr;

    // Small array holding temporaries (implicit conversion/*args/**kwargs)
    cleanup_list cleanup(self_arg);

    // Preallocate stack memory for function dispatch
    size_t max_nargs = ((nb_func *) self)->max_nargs;
    PyObject **args = (PyObject **) alloca(max_nargs * sizeof(PyObject *));
    uint8_t *args_flags = (uint8_t *) alloca(max_nargs * sizeof(uint8_t));
    bool *kwarg_used = (bool *) alloca(nkwargs_in * sizeof(bool));

    // Ensure that keyword argument names are interned. That makes it faster
    // to compare them against pre-interned argument names in the overload chain.
    // Normal function calls will have their keyword arguments already interned,
    // but we can't rely on that; it fails for things like fn(**json.loads(...)).
    PyObject **kwnames = nullptr;

#if !defined(PYPY_VERSION) && !defined(Py_LIMITED_API)
    bool kwnames_interned = true;
    for (size_t i = 0; i < nkwargs_in; ++i) {
        PyObject *key = NB_TUPLE_GET_ITEM(kwargs_in, i);
        kwnames_interned &= ((PyASCIIObject *) key)->state.interned != 0;
    }
    if (kwargs_in && NB_LIKELY(kwnames_interned)) {
        kwnames = ((PyTupleObject *) kwargs_in)->ob_item;
        goto traverse_overloads;
    }
#endif

    kwnames = (PyObject **) alloca(nkwargs_in * sizeof(PyObject *));
    for (size_t i = 0; i < nkwargs_in; ++i) {
        PyObject *key = NB_TUPLE_GET_ITEM(kwargs_in, i);
        Py_INCREF(key);

        kwnames[i] = key;
        PyUnicode_InternInPlace(&kwnames[i]);
        PyObject *key_interned = kwnames[i];

        if (NB_LIKELY(key == key_interned)) // string was already interned
            Py_DECREF(key);
        else
            cleanup.append(key_interned);
    }

#if !defined(PYPY_VERSION) && !defined(Py_LIMITED_API)
  traverse_overloads:
#endif

    /*  The logic below tries to find a suitable overload using two passes
        of the overload chain (or 1, if there are no overloads). The first pass
        is strict and permits no implicit conversions, while the second pass
        allows them.

        The following is done per overload during a pass

        1. Copy individual arguments while checking that named positional
           arguments weren't *also* specified as kwarg. Substitute missing
           entries using keyword arguments or default argument values provided
           in the bindings, if available.

        2. Ensure that either all keyword arguments were "consumed", or that
           the function takes a kwargs argument to accept unconsumed kwargs.

        3. Any positional arguments still left get put into a tuple (for args),
           and any leftover kwargs get put into a dict.

        4. Pack everything into a vector; if we have nb::args or nb::kwargs,
           they become a tuple or dict at the end of the positional arguments.

        5. Call the function call dispatcher (func_data::impl)

        If one of these fail, move on to the next overload and keep trying
        until we get a result other than NB_NEXT_OVERLOAD.
    */

    for (size_t pass = (count > 1) ? 0 : 1; pass < 2; ++pass) {
        for (size_t k = 0; k < count; ++k) {
            const func_data *f = fr + k;

            const bool has_args       = f->flags & (uint32_t) func_flags::has_args,
                       has_var_args   = f->flags & (uint32_t) func_flags::has_var_args,
                       has_var_kwargs = f->flags & (uint32_t) func_flags::has_var_kwargs;

            // Number of C++ parameters eligible to be filled from individual
            // Python positional arguments
            size_t nargs_pos = f->nargs_pos;

            // Number of C++ parameters in total, except for a possible trailing
            // nb::kwargs. All of these are eligible to be filled from individual
            // Python arguments (keyword always, positional until index nargs_pos)
            // except for a potential nb::args, which exists at index nargs_pos
            // if has_var_args is true. We'll skip that one in the individual-args
            // loop, and go back and fill it later with the unused positionals.
            size_t nargs_step1 = f->nargs - has_var_kwargs;

            if (nargs_in > nargs_pos && !has_var_args)
                continue; // Too many positional arguments given for this overload

            if (nargs_in < nargs_pos && !has_args)
                continue; // Not enough positional arguments, insufficient
                          // keyword/default arguments to fill in the blanks

            memset(kwarg_used, 0, nkwargs_in * sizeof(bool));

            // 1. Copy individual arguments, potentially substitute kwargs/defaults
            size_t i = 0;
            for (; i < nargs_step1; ++i) {
                if (has_var_args && i == nargs_pos)
                    continue; // skip nb::args parameter, will be handled below

                PyObject *arg = nullptr;

                uint8_t arg_flag = 1;

                // If i >= nargs_pos, then this is a keyword-only parameter.
                // (We skipped any *args parameter using the test above,
                // and we set the bounds of nargs_step1 to not include any
                // **kwargs parameter.) In that case we don't want to take
                // a positional arg (which might validly exist and be
                // destined for the *args) but we do still want to look for
                // a matching keyword arg.
                if (i < nargs_in && i < nargs_pos)
                    arg = args_in[i];

                if (has_args) {
                    const arg_data &ad = f->args[i];

                    if (kwargs_in && ad.name_py) {
                        PyObject *hit = nullptr;
                        for (size_t j = 0; j < nkwargs_in; ++j) {
                            if (kwnames[j] == ad.name_py) {
                                hit = args_in[nargs_in + j];
                                kwarg_used[j] = true;
                                break;
                            }
                        }

                        if (hit) {
                            if (arg)
                                break; // conflict between keyword and positional arg.
                            arg = hit;
                        }
                    }

                    if (!arg)
                        arg = ad.value;
                    arg_flag = ad.flag;
                }

                if (!arg || (arg == Py_None && (arg_flag & cast_flags::accepts_none) == 0))
                    break;

                // Implicit conversion only active in the 2nd pass
                args_flags[i] = arg_flag & ~uint8_t(pass == 0);
                args[i] = arg;
            }

            // Skip this overload if any arguments were unavailable
            if (i != nargs_step1)
                continue;

            // Deal with remaining positional arguments
            if (has_var_args) {
                PyObject *tuple = PyTuple_New(
                    nargs_in > nargs_pos ? (Py_ssize_t) (nargs_in - nargs_pos) : 0);

                for (size_t j = nargs_pos; j < nargs_in; ++j) {
                    PyObject *o = args_in[j];
                    Py_INCREF(o);
                    NB_TUPLE_SET_ITEM(tuple, j - nargs_pos, o);
                }

                args[nargs_pos] = tuple;
                args_flags[nargs_pos] = 0;
                cleanup.append(tuple);
            }

            // Deal with remaining keyword arguments
            if (has_var_kwargs) {
                PyObject *dict = PyDict_New();
                for (size_t j = 0; j < nkwargs_in; ++j) {
                    PyObject *key = kwnames[j];
                    if (!kwarg_used[j])
                        PyDict_SetItem(dict, key, args_in[nargs_in + j]);
                }

                args[nargs_step1] = dict;
                args_flags[nargs_step1] = 0;
                cleanup.append(dict);
            } else if (kwargs_in) {
                bool success = true;
                for (size_t j = 0; j < nkwargs_in; ++j)
                    success &= kwarg_used[j];
                if (!success)
                    continue;
            }


            if (is_constructor)
                args_flags[0] |= (uint8_t) cast_flags::construct;

            rv_policy policy = (rv_policy) (f->flags & 0b111);

            try {
                result = nullptr;

                // Found a suitable overload, let's try calling it
                result = f->impl((void *) f->capture, args, args_flags,
                                 policy, &cleanup);

                if (NB_UNLIKELY(!result))
                    error_handler = nb_func_error_noconvert;
            } catch (builtin_exception &e) {
                if (!set_builtin_exception_status(e))
                    result = NB_NEXT_OVERLOAD;
            } catch (python_error &e) {
                e.restore();
            } catch (...) {
                nb_func_convert_cpp_exception();
            }

            if (result != NB_NEXT_OVERLOAD) {
                if (is_constructor && result != nullptr) {
                    nb_inst *self_arg_nb = (nb_inst *) self_arg;
                    self_arg_nb->destruct = true;
                    self_arg_nb->state = nb_inst::state_ready;
                    if (NB_UNLIKELY(self_arg_nb->intrusive))
                        nb_type_data(Py_TYPE(self_arg))
                            ->set_self_py(inst_ptr(self_arg_nb), self_arg);
                }

                goto done;
            }
        }
    }

    error_handler = nb_func_error_overload;

done:
    if (NB_UNLIKELY(cleanup.used()))
        cleanup.release();

    if (NB_UNLIKELY(error_handler))
        result = error_handler(self, args_in, nargs_in, kwargs_in);

    return result;
}

/// Simplified nb_func_vectorcall variant for functions w/o keyword arguments,
/// w/o default arguments, with no more than NB_MAXARGS_SIMPLE arguments, etc.
static PyObject *nb_func_vectorcall_simple(PyObject *self,
                                           PyObject *const *args_in,
                                           size_t nargsf,
                                           PyObject *kwargs_in) noexcept {
    uint8_t args_flags[NB_MAXARGS_SIMPLE];
    func_data *fr = nb_func_data(self);

    const size_t count         = (size_t) Py_SIZE(self),
                 nargs_in      = (size_t) NB_VECTORCALL_NARGS(nargsf);

    const bool is_method      = fr->flags & (uint32_t) func_flags::is_method,
               is_constructor = fr->flags & (uint32_t) func_flags::is_constructor;

    PyObject *result = nullptr,
             *self_arg = (is_method && nargs_in > 0) ? args_in[0] : nullptr;

    // Small array holding temporaries (implicit conversion/*args/**kwargs)
    cleanup_list cleanup(self_arg);

    // Handler routine that will be invoked in case of an error condition
    PyObject *(*error_handler)(PyObject *, PyObject *const *, size_t,
                               PyObject *) noexcept = nullptr;

    bool fail = kwargs_in != nullptr;
    PyObject *none_ptr = Py_None;
    for (size_t i = 0; i < nargs_in; ++i)
        fail |= args_in[i] == none_ptr;

    if (fail) { // keyword/None arguments unsupported in simple vectorcall
        error_handler = nb_func_error_overload;
        goto done;
    }

    for (size_t pass = (count > 1) ? 0 : 1; pass < 2; ++pass) {
        for (int i = 0; i < NB_MAXARGS_SIMPLE; ++i)
            args_flags[i] = (uint8_t) pass;

        if (is_constructor)
            args_flags[0] = (uint8_t) cast_flags::construct;

        for (size_t k = 0; k < count; ++k) {
            const func_data *f = fr + k;

            if (nargs_in != f->nargs)
                continue;

            try {
                result = nullptr;

                // Found a suitable overload, let's try calling it
                result = f->impl((void *) f->capture, (PyObject **) args_in,
                                 args_flags, (rv_policy) (f->flags & 0b111),
                                 &cleanup);

                if (NB_UNLIKELY(!result))
                    error_handler = nb_func_error_noconvert;
            } catch (builtin_exception &e) {
                if (!set_builtin_exception_status(e))
                    result = NB_NEXT_OVERLOAD;
            } catch (python_error &e) {
                e.restore();
            } catch (...) {
                nb_func_convert_cpp_exception();
            }

            if (result != NB_NEXT_OVERLOAD) {
                if (is_constructor && result != nullptr) {
                    nb_inst *self_arg_nb = (nb_inst *) self_arg;
                    self_arg_nb->destruct = true;
                    self_arg_nb->state = nb_inst::state_ready;
                    if (NB_UNLIKELY(self_arg_nb->intrusive))
                        nb_type_data(Py_TYPE(self_arg))
                            ->set_self_py(inst_ptr(self_arg_nb), self_arg);
                }

                goto done;
            }
        }
    }

    error_handler = nb_func_error_overload;

done:
    if (NB_UNLIKELY(cleanup.used()))
        cleanup.release();

    if (NB_UNLIKELY(error_handler))
        result = error_handler(self, args_in, nargs_in, kwargs_in);

    return result;
}

/// Simplified nb_func_vectorcall variant for non-overloaded functions with 0 args
static PyObject *nb_func_vectorcall_simple_0(PyObject *self,
                                             PyObject *const *args_in,
                                             size_t nargsf,
                                             PyObject *kwargs_in) noexcept {
    func_data *fr = nb_func_data(self);
    const size_t nargs_in = (size_t) NB_VECTORCALL_NARGS(nargsf);

    // Handler routine that will be invoked in case of an error condition
    PyObject *(*error_handler)(PyObject *, PyObject *const *, size_t,
                               PyObject *) noexcept = nullptr;

    PyObject *result = nullptr;

    if (kwargs_in == nullptr && nargs_in == 0) {
        try {
            result = fr->impl((void *) fr->capture, (PyObject **) args_in,
                              nullptr, (rv_policy) (fr->flags & 0b111), nullptr);
            if (result == NB_NEXT_OVERLOAD)
                error_handler = nb_func_error_overload;
            else if (!result)
                error_handler = nb_func_error_noconvert;
        } catch (builtin_exception &e) {
            if (!set_builtin_exception_status(e))
                error_handler = nb_func_error_overload;
        } catch (python_error &e) {
            e.restore();
        } catch (...) {
            nb_func_convert_cpp_exception();
        }
    } else {
        error_handler = nb_func_error_overload;
    }

    if (NB_UNLIKELY(error_handler))
        result = error_handler(self, args_in, nargs_in, kwargs_in);

    return result;
}

/// Simplified nb_func_vectorcall variant for non-overloaded functions with 1 arg
static PyObject *nb_func_vectorcall_simple_1(PyObject *self,
                                             PyObject *const *args_in,
                                             size_t nargsf,
                                             PyObject *kwargs_in) noexcept {
    func_data *fr = nb_func_data(self);
    const size_t nargs_in = (size_t) NB_VECTORCALL_NARGS(nargsf);
    bool is_constructor = fr->flags & (uint32_t) func_flags::is_constructor;

    // Handler routine that will be invoked in case of an error condition
    PyObject *(*error_handler)(PyObject *, PyObject *const *, size_t,
                               PyObject *) noexcept = nullptr;

    PyObject *result = nullptr;

    if (kwargs_in == nullptr && nargs_in == 1 && args_in[0] != Py_None) {
        PyObject *arg = args_in[0];
        cleanup_list cleanup(arg);
        uint8_t args_flags[1] = {
            (uint8_t) (is_constructor ? (1 | (uint8_t) cast_flags::construct) : 1)
        };

        try {
            result = fr->impl((void *) fr->capture, (PyObject **) args_in,
                              args_flags, (rv_policy) (fr->flags & 0b111), &cleanup);
            if (result == NB_NEXT_OVERLOAD) {
                error_handler = nb_func_error_overload;
            } else if (!result) {
                error_handler = nb_func_error_noconvert;
            } else if (is_constructor) {
                nb_inst *arg_nb = (nb_inst *) arg;
                arg_nb->destruct = true;
                arg_nb->state = nb_inst::state_ready;
                if (NB_UNLIKELY(arg_nb->intrusive))
                    nb_type_data(Py_TYPE(arg))
                        ->set_self_py(inst_ptr(arg_nb), arg);
            }
        } catch (builtin_exception &e) {
            if (!set_builtin_exception_status(e))
                error_handler = nb_func_error_overload;
        } catch (python_error &e) {
            e.restore();
        } catch (...) {
            nb_func_convert_cpp_exception();
        }

        if (NB_UNLIKELY(cleanup.used()))
            cleanup.release();
    } else {
        error_handler = nb_func_error_overload;
    }

    if (NB_UNLIKELY(error_handler))
        result = error_handler(self, args_in, nargs_in, kwargs_in);

    return result;
}

static PyObject *nb_bound_method_vectorcall(PyObject *self,
                                            PyObject *const *args_in,
                                            size_t nargsf,
                                            PyObject *kwargs_in) noexcept {
    nb_bound_method *mb = (nb_bound_method *) self;
    size_t nargs = (size_t) NB_VECTORCALL_NARGS(nargsf);
    const size_t buf_size = 5;
    PyObject **args, *args_buf[buf_size], *temp = nullptr, *result;
    bool alloc = false;

    if (NB_LIKELY(nargsf & NB_VECTORCALL_ARGUMENTS_OFFSET)) {
        args = (PyObject **) (args_in - 1);
        temp = args[0];
    } else {
        size_t size = nargs + 1;
        if (kwargs_in)
            size += NB_TUPLE_GET_SIZE(kwargs_in);

        if (size < buf_size) {
            args = args_buf;
        } else {
            args = (PyObject **) PyMem_Malloc(size * sizeof(PyObject *));
            if (!args)
                return PyErr_NoMemory();
            alloc = true;
        }

        if (size > 1)
            memcpy(args + 1, args_in, sizeof(PyObject *) * (size - 1));
    }

    args[0] = mb->self;
    result = mb->func->vectorcall((PyObject *) mb->func, args, nargs + 1, kwargs_in);
    args[0] = temp;

    if (NB_UNLIKELY(alloc))
        PyMem_Free(args);

    return result;
}

PyObject *nb_method_descr_get(PyObject *self, PyObject *inst, PyObject *) {
    if (inst) {
        /* Return a bound method. This should be avoidable in most cases via the
           'CALL_METHOD' opcode and vector calls. Pytest rewrites the bytecode
           in a way that breaks this optimization :-/ */

        nb_bound_method *mb =
            PyObject_GC_New(nb_bound_method, internals->nb_bound_method);
        mb->func = (nb_func *) self;
        mb->self = inst;
        mb->vectorcall = nb_bound_method_vectorcall;

        Py_INCREF(self);
        Py_INCREF(inst);

        return (PyObject *) mb;
    } else {
        Py_INCREF(self);
        return self;
    }
}

/// Render the function signature of a single function. Callers must hold the
/// 'internals' mutex.
static uint32_t nb_func_render_signature(const func_data *f,
                                         bool nb_signature_mode,
                                         signature_capture *capture) noexcept {
    const bool is_method      = f->flags & (uint32_t) func_flags::is_method,
               has_args       = f->flags & (uint32_t) func_flags::has_args,
               has_var_args   = f->flags & (uint32_t) func_flags::has_var_args,
               has_var_kwargs = f->flags & (uint32_t) func_flags::has_var_kwargs,
               has_signature  = f->flags & (uint32_t) func_flags::has_signature;

    if (capture) {
        // When the caller requests a ``signature_capture`` (PEP 3107/362
        // plumbing), ensure the structure starts in a clean state so we don't
        // leak information across overloads.
        capture->parameters.clear();
        capture->current_name.clear();
        capture->current_type.clear();
        capture->return_type.clear();
        capture->capturing_param = false;
        capture->capturing_return = false;
    }

    nb_internals *internals_ = internals;
    if (has_signature) {
        const char *s = f->signature;

        if (!nb_signature_mode) {
            // go to last line of manually provided signature, strip away 'def ' prefix
            const char *p = strrchr(s, '\n');
            s = p ? (p + 1) : s;
            if (strncmp(s, "def ", 4) == 0)
                s += 4;
        }

        buf.put_dstr(s);
        return 0;
    }

    if (nb_signature_mode)
        buf.put("def ");

    const std::type_info **descr_type = f->descr_types;
    bool rv = false;

    uint32_t arg_index = 0, n_default_args = 0;
    buf.put_dstr(f->name);

    for (const char *pc = f->descr; *pc != '\0'; ++pc) {
        char c = *pc;

        switch (c) {
            case '@':
                // Handle types that differ depending on whether they appear
                // in an argument or a return value position
                pc++;
                if (!rv) {
                    while (*pc && *pc != '@')
                        buf.put(*pc++);
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
                        buf.put(*pc++);
                }
                break;

            case '{':
                {
                    const char *arg_name = has_args ? f->args[arg_index].name : nullptr;

                    // Argument name
                    if (has_var_kwargs && arg_index + 1 == f->nargs) {
                        buf.put("**");
                        buf.put_dstr(arg_name ? arg_name : "kwargs");
                        if (capture) {
                            capture_start_param(capture,
                                                std::string(arg_name ? arg_name : "kwargs"));
                            capture->current_type = "typing.Dict[str, typing.Any]";
                            capture_finish_param(capture);
                        }
                        pc += 4; // strlen("dict")
                        break;
                    }

                    if (arg_index == f->nargs_pos) {
                        buf.put("*");
                        if (has_var_args) {
                            buf.put_dstr(arg_name ? arg_name : "args");
                            if (capture) {
                                capture_start_param(capture,
                                                    std::string(arg_name ? arg_name : "args"));
                                capture->current_type = "typing.Tuple[typing.Any, ...]";
                                capture_finish_param(capture);
                            }
                            pc += 5; // strlen("tuple")
                            break;
                        } else {
                            buf.put(", ");
                            // fall through to render the first keyword-only arg
                        }
                    }

                    if (is_method && arg_index == 0) {
                        buf.put("self");
                        if (capture)
                            capture->capturing_param = false;

                        // Skip over type
                        while (*pc != '}') {
                            if (*pc == '%')
                                descr_type++;
                            pc++;
                        }
                        arg_index++;
                        continue;
                    } else if (arg_name) {
                        buf.put_dstr(arg_name);
                        if (capture)
                            capture_start_param(capture, std::string(arg_name));
                    } else {
                        std::string generated = "arg";
                        if (f->nargs > 1 + (uint32_t) is_method)
                            generated += std::to_string(arg_index - is_method);
                        buf.put_dstr(generated.c_str());
                        if (capture)
                            capture_start_param(capture, generated);
                    }

                    buf.put(": ");
                    capture_append(capture, ": ", 2);
                    if (has_args && f->args[arg_index].flag &
                                        (uint8_t) cast_flags::accepts_none) {
#if PY_VERSION_HEX < 0x030A0000
                            buf.put("typing.Optional[");
                            capture_append(capture, "typing.Optional[");
                        #else
                            // See below
                        #endif
                    }
                }
                break;

            case '}':
                // Default argument
                if (has_args) {
                    if (f->args[arg_index].flag & (uint8_t) cast_flags::accepts_none) {
                        #if PY_VERSION_HEX < 0x030A0000
                            buf.put(']');
                            capture_append(capture, ']');
                        #else
                            buf.put(" | None");
                            capture_append(capture, " | None");
                        #endif
                    }

                    if (f->args[arg_index].value) {
                        const arg_data &arg = f->args[arg_index];
                        if (nb_signature_mode) {
                            buf.put(" = \\");
                            if (arg.signature)
                                buf.put('=');
                            buf.put_uint32(n_default_args++);
                        } else if (arg.signature) {
                            buf.put(" = ");
                            buf.put_dstr(arg.signature);
                        } else {
                            PyObject *o = arg.value, *str;

                            {
                                unlock_internals guard2(internals_);
                                str = PyObject_Repr(o);
                            }

                            if (str) {
                                Py_ssize_t size = 0;
                                const char *cstr =
                                    PyUnicode_AsUTF8AndSize(str, &size);
                                if (!cstr) {
                                    PyErr_Clear();
                                } else {
                                    buf.put(" = ");
                                    buf.put(cstr, (size_t) size);
                                }
                                Py_DECREF(str);
                            } else {
                                PyErr_Clear();
                            }
                        }
                    }
                }

                capture_finish_param(capture);
                arg_index++;

                if (arg_index == f->nargs_pos && !has_args)
                    buf.put(", /");

                break;

            case '%':
                check(*descr_type,
                      "nb::detail::nb_func_render_signature(): missing type!");

                if (!(is_method && arg_index == 0)) {
                    bool found = false;
                    auto it = internals_->type_c2p_slow.find(*descr_type);

                    if (it != internals_->type_c2p_slow.end()) {
                        handle th((PyObject *) it->second->type_py);
                        str module = borrow<str>(th.attr("__module__"));
                        str qualname = borrow<str>(th.attr("__qualname__"));
                        buf.put_dstr(module.c_str());
                        capture_append(capture, module.c_str());
                        buf.put('.');
                        capture_append(capture, '.');
                        buf.put_dstr(qualname.c_str());
                        capture_append(capture, qualname.c_str());
                        found = true;
                    }
                    if (!found) {
                        if (nb_signature_mode)
                            buf.put('"');
                        char *name = type_name(*descr_type);
                        buf.put_dstr(name);
                        capture_append(capture, name);
                        free(name);
                        if (nb_signature_mode)
                            buf.put('"');
                    }
                }

                descr_type++;
                break;

            case '-':
                if (pc[1] == '>') {
                    rv = true;
                    if (capture) {
                        capture->capturing_return = true;
                        capture->return_type.clear();
                    }
                }
                buf.put(c);
                break;


            default:
                buf.put(c);
                break;
        }
    }

    check(arg_index == f->nargs && !*descr_type,
          "nanobind::detail::nb_func_render_signature(%s): arguments inconsistent.",
          f->name);

    return n_default_args;
}

static PyObject *nb_func_get_name(PyObject *self) {
    func_data *f = nb_func_data(self);
    const char *name = "";
    if (f->flags & (uint32_t) func_flags::has_name)
        name = f->name;
    return PyUnicode_FromString(name);
}

/** Build PEP-compliant introspection metadata directly on the ``nb_func``
    descriptor. PEP 3107 expects ``__annotations__`` describing the arguments,
    while PEP 362 relies on ``__text_signature__`` to recover parameter order.
    We derive both from nanobind's canonical signature rendering so that
    downstream Python tooling (inspect, Sphinx, typing) can treat nanobind
    callables just like builtins. */
static bool nb_func_apply_introspection(nb_func *func, const func_data *f) {
    std::string rendered_signature;
    {
        lock_internals guard(internals);
        buf.clear();
        nb_func_render_signature(f, false);
        rendered_signature = buf.get();
    }

    auto paren_open = rendered_signature.find('(');
    auto paren_close = rendered_signature.rfind(')');
    std::vector<std::pair<std::string, std::string>> params;
    /** ``sanitized_tokens`` mirrors the textual argument list but strips all
        annotations; this mirrors what CPython does for builtin functions so
        that ``inspect`` can rehydrate the signature. */
    std::vector<std::string> sanitized_tokens;
    if (paren_open != std::string::npos && paren_close != std::string::npos &&
        paren_close > paren_open + 1) {
        std::string_view params_view(
            rendered_signature.c_str() + paren_open + 1,
            paren_close - paren_open - 1
        );
        size_t token_start = 0;
        int depth = 0;
        auto flush_token = [&](size_t end) {
            std::string_view token = trim_view(params_view.substr(token_start, end - token_start));
            token_start = end + 1;
            if (token.empty())
                return;

            if (token == "/" || token == "*") {
                sanitized_tokens.emplace_back(std::string(token));
                return;
            }

            if (token[0] == '*') {
                bool kw = token.size() > 1 && token[1] == '*';
                std::string_view name_view = trim_view(token.substr(kw ? 2 : 1));
                if (name_view.empty())
                    return;
                std::string type = kw ? "typing.Dict[str, typing.Any]"
                                      : "typing.Tuple[typing.Any, ...]";
                params.emplace_back(std::string(name_view), std::move(type));
                std::string sanitized = kw ? "**" : "*";
                sanitized.append(name_view.begin(), name_view.end());
                sanitized_tokens.emplace_back(std::move(sanitized));
                return;
            }

            size_t eq_pos = find_top_level(token, '=');
            std::string_view before = trim_view(token.substr(0, eq_pos));
            size_t colon_pos = find_top_level(before, ':');
            std::string_view name_view = trim_view(before.substr(0, colon_pos));
            if (name_view.empty() || name_view == "self")
                return;
            std::string_view type_view;
            if (colon_pos != std::string_view::npos)
                type_view = trim_view(before.substr(colon_pos + 1));

            std::string type = type_view.empty() ? "typing.Any" : std::string(type_view);
            params.emplace_back(std::string(name_view), std::move(type));

            std::string sanitized(name_view.begin(), name_view.end());
            if (eq_pos != std::string_view::npos) {
                std::string_view default_view = trim_view(token.substr(eq_pos + 1));
                if (!default_view.empty()) {
                    sanitized += " = ";
                    sanitized.append(default_view.begin(), default_view.end());
                }
            }
            sanitized_tokens.emplace_back(std::move(sanitized));
        };

        for (size_t i = 0; i < params_view.size(); ++i) {
            char ch = params_view[i];
            if (ch == '[' || ch == '(' || ch == '{') {
                depth++;
            } else if (ch == ']' || ch == ')' || ch == '}') {
                if (depth > 0)
                    depth--;
            } else if (ch == ',' && depth == 0) {
                flush_token(i);
            }
        }
        flush_token(params_view.size());
    }

    PyObject *annotations = PyDict_New();
    if (!annotations)
        return false;

    for (const auto &entry : params) {
        PyObject *value = PyUnicode_FromString(entry.second.c_str());
        if (!value || PyDict_SetItemString(annotations, entry.first.c_str(), value) < 0) {
            Py_XDECREF(value);
            Py_DECREF(annotations);
            return false;
        }
        Py_DECREF(value);
    }

    auto arrow_pos = rendered_signature.rfind("->");
    if (arrow_pos != std::string::npos) {
        std::string_view return_view
            = trim_view(std::string_view(rendered_signature).substr(arrow_pos + 2));
        if (!return_view.empty()) {
            PyObject *ret = PyUnicode_FromString(std::string(return_view).c_str());
            if (!ret || PyDict_SetItemString(annotations, "return", ret) < 0) {
                Py_XDECREF(ret);
                Py_DECREF(annotations);
                return false;
            }
            Py_DECREF(ret);
        }
    }

    Py_XDECREF(func->annotations);
    func->annotations = annotations;

    /** ``inspect`` rejects text signatures containing annotations, so emit the
        minimal positional structure while keeping the actual typing data in
        ``__annotations__``. */
    std::string clean_params;
    for (size_t i = 0; i < sanitized_tokens.size(); ++i) {
        clean_params += sanitized_tokens[i];
        if (i + 1 < sanitized_tokens.size())
            clean_params += ", ";
    }

    Py_XDECREF(func->text_signature);
    func->text_signature = PyUnicode_FromString(("(" + clean_params + ")").c_str());

    return true;
}

static PyObject *nb_func_get_qualname(PyObject *self) {
    func_data *f = nb_func_data(self);
    if ((f->flags & (uint32_t) func_flags::has_scope) &&
        (f->flags & (uint32_t) func_flags::has_name)) {
        PyObject *scope_name = PyObject_GetAttrString(f->scope, "__qualname__");
        if (scope_name) {
            return PyUnicode_FromFormat("%U.%s", scope_name, f->name);
        } else {
            PyErr_Clear();
            return PyUnicode_FromString(f->name);
        }
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject *nb_func_get_module(PyObject *self) {
    func_data *f = nb_func_data(self);
    if (f->flags & (uint32_t) func_flags::has_scope) {
        return PyObject_GetAttrString(
            f->scope, PyModule_Check(f->scope) ? "__name__" : "__module__");
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

PyObject *nb_func_get_nb_signature(PyObject *self, void *) {
    PyObject *docstr = nullptr, *item = nullptr, *sigstr = nullptr,
             *defaults = nullptr;

    func_data *f = nb_func_data(self);
    uint32_t count = (uint32_t) Py_SIZE(self);
    PyObject *result = PyTuple_New(count);
    if (!result)
        return nullptr;

    for (uint32_t i = 0; i < count; ++i) {
        docstr = item = sigstr = defaults = nullptr;

        const func_data *fi = f + i;
        if ((fi->flags & (uint32_t) func_flags::has_doc) &&
            (!((nb_func *) self)->doc_uniform || i == 0)) {
            docstr = PyUnicode_FromString(fi->doc);
        } else {
            docstr = Py_None;
            Py_INCREF(docstr);
        }

        // The buffer 'buf' is protected by 'internals.mutex'
        lock_internals guard(internals);

        buf.clear();
        uint32_t n_default_args = nb_func_render_signature(fi, true);

        item = PyTuple_New(3);
        sigstr = PyUnicode_FromString(buf.get());
        if (n_default_args) {
            defaults = PyTuple_New(n_default_args);
        } else {
            defaults = Py_None;
            Py_INCREF(defaults);
        }

        if (!docstr || !sigstr || !item || !defaults)
            goto fail;

        if (n_default_args) {
            size_t pos = 0;
            for (uint32_t j = 0; j < fi->nargs; ++j) {
                const arg_data &arg = fi->args[j];
                PyObject *value = arg.value;
                if (!value)
                    continue;
                if (arg.signature) {
                    value = PyUnicode_FromString(arg.signature);
                    if (!value)
                        goto fail;
                } else {
                    Py_INCREF(value);
                }
                NB_TUPLE_SET_ITEM(defaults, pos, value);
                pos++;
            }

            check(pos == n_default_args,
                  "__nb_signature__: default argument counting inconsistency!");
        }

        NB_TUPLE_SET_ITEM(item, 0, sigstr);
        NB_TUPLE_SET_ITEM(item, 1, docstr);
        NB_TUPLE_SET_ITEM(item, 2, defaults);
        NB_TUPLE_SET_ITEM(result, (Py_ssize_t) i, item);
    }

    return result;

fail:
    Py_XDECREF(docstr);
    Py_XDECREF(sigstr);
    Py_XDECREF(defaults);
    Py_XDECREF(item);
    Py_DECREF(result);
    return nullptr;
}

PyObject *nb_func_get_doc(PyObject *self, void *) {
    func_data *f = nb_func_data(self);
    uint32_t count = (uint32_t) Py_SIZE(self);

    // The buffer 'buf' is protected by 'internals.mutex'
    lock_internals guard(internals);

    buf.clear();

    bool doc_found = false;

    for (uint32_t i = 0; i < count; ++i) {
        const func_data *fi = f + i;
        nb_func_render_signature(fi);
        buf.put('\n');
        doc_found |= (fi->flags & (uint32_t) func_flags::has_doc) != 0;
    }

    if (doc_found) {
        if (((nb_func *) self)->doc_uniform) {
            buf.put('\n');
            buf.put_dstr(f->doc);
            buf.put('\n');
        } else {
            buf.put("\nOverloaded function.\n");
            for (uint32_t i = 0; i < count; ++i) {
                const func_data *fi = f + i;

                buf.put('\n');
                buf.put_uint32(i + 1);
                buf.put(". ``");
                nb_func_render_signature(fi);
                buf.put("``\n\n");

                if (fi->flags & (uint32_t) func_flags::has_doc) {
                    buf.put_dstr(fi->doc);
                    buf.put('\n');
                }
            }
        }
    }

    if (buf.size() > 0) // remove last newline
        buf.rewind(1);

    return PyUnicode_FromString(buf.get());
}

// PyGetSetDef entry for __module__ is ignored in Python 3.8
PyObject *nb_func_getattro(PyObject *self, PyObject *name_) {
    const char *name = PyUnicode_AsUTF8AndSize(name_, nullptr);

    if (!name)
        return nullptr;
    else if (strcmp(name, "__module__") == 0)
        return nb_func_get_module(self);
    else if (strcmp(name, "__name__") == 0)
        return nb_func_get_name(self);
    else if (strcmp(name, "__qualname__") == 0)
        return nb_func_get_qualname(self);
    else if (strcmp(name, "__doc__") == 0)
        return nb_func_get_doc(self, nullptr);
    else if (strcmp(name, "__annotations__") == 0) {
        // PEP 3107: expose the cached annotations dict to CPython callers.
        nb_func *func = (nb_func *) self;
        if (func->annotations) {
            Py_INCREF(func->annotations);
            return func->annotations;
        }
        Py_INCREF(Py_None);
        return Py_None;
    } else if (strcmp(name, "__text_signature__") == 0) {
        // PEP 362: the sanitized text signature feeds ``inspect.signature``.
        nb_func *func = (nb_func *) self;
        if (func->text_signature) {
            Py_INCREF(func->text_signature);
            return func->text_signature;
        }
        Py_INCREF(Py_None);
        return Py_None;
    }
    else
        return PyObject_GenericGetAttr(self, name_);
}

PyObject *nb_bound_method_getattro(PyObject *self, PyObject *name_) {
    bool passthrough = false;
    if (const char *name = PyUnicode_AsUTF8AndSize(name_, nullptr)) {
        // These attributes do exist on nb_bound_method (because they
        // exist on every type) but we want to take their special handling
        // from nb_func_getattro instead.
        passthrough = (strcmp(name, "__doc__") == 0 ||
                       strcmp(name, "__module__") == 0);
    }
    if (!passthrough) {
        if (PyObject* res = PyObject_GenericGetAttr(self, name_))
            return res;
        PyErr_Clear();
    }
    nb_func *func = ((nb_bound_method *) self)->func;
    return nb_func_getattro((PyObject *) func, name_);
}

/// Excise a substring from 's'
static void strexc(char *s, const char *sub) {
    size_t len = strlen(sub);
    if (len == 0)
        return;

    char *p = s;
    while ((p = strstr(p, sub)))
        memmove(p, p + len, strlen(p + len) + 1);
}

/// Return a readable string representation of a C++ type
NB_NOINLINE char *type_name(const std::type_info *t) {
    const char *name_in = t->name();

#if defined(__GNUG__)
    int status = 0;
    char *name = abi::__cxa_demangle(name_in, nullptr, nullptr, &status);
    if (!name)
        return strdup_check(name_in);
#else
    char *name = strdup_check(name_in);
    strexc(name, "class ");
    strexc(name, "struct ");
    strexc(name, "enum ");
#endif
    strexc(name, "nanobind::");
    return name;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
