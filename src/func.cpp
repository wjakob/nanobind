#include "internals.h"
#include "buffer.h"

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Forward/external declarations
extern Buffer buf;
static PyObject *nb_func_vectorcall_simple(PyObject *, PyObject *const *,
                                           size_t, PyObject *);
static PyObject *nb_func_vectorcall_complex(PyObject *, PyObject *const *,
                                            size_t, PyObject *);

/// Free a function overload chain
void nb_func_dealloc(PyObject *self) {
    Py_ssize_t size = Py_SIZE(self);

    if (size) {
        func_record *f = nb_func_get(self);

        // Delete from registered function list
        PyObject *self_key = ptr_to_key(self);
        int rv = PySet_Discard(internals_get().funcs, self_key);
        Py_DECREF(self_key);
        if (rv != 1) {
            const char *name = (f->flags & (uint16_t) func_flags::has_name)
                                   ? f->name : "<anonymous>";
            fail("nanobind::detail::nb_func_dealloc(\"%s\"): function not found!",
                 name);
        }

        for (Py_ssize_t i = 0; i < size; ++i) {
            if (f->flags & (uint16_t) func_flags::has_free)
                f->free(f->capture);

            if (f->flags & (uint16_t) func_flags::has_args) {
                for (size_t i = 0; i< f->nargs; ++i) {
                    Py_XDECREF(f->args[i].value);
                    Py_XDECREF(f->args[i].name_py);
                }
            }

            free(f->args);
            free((char *) f->descr);
            free(f->descr_types);
            free(f->signature);
            ++f;
        }
    }

    PyObject_Free(self);
}

/**
 * \brief Wrap a C++ function into a Python function object
 *
 * This is an implementation detail of nanobind::cpp_function.
 */
PyObject *nb_func_new(const void *in_) noexcept {
    func_data<0> *f = (func_data<0> *) in_;

    const bool has_scope      = f->flags & (uint16_t) func_flags::has_scope,
               has_name       = f->flags & (uint16_t) func_flags::has_name,
               has_args       = f->flags & (uint16_t) func_flags::has_args,
               has_var_args   = f->flags & (uint16_t) func_flags::has_var_args,
               has_var_kwargs = f->flags & (uint16_t) func_flags::has_var_kwargs,
               is_method      = f->flags & (uint16_t) func_flags::is_method,
               return_ref     = f->flags & (uint16_t) func_flags::return_ref;

    PyObject *name = nullptr;
    PyObject *func_prev = nullptr;
    internals &internals = internals_get();

    // Check for previous overloads
    if (has_scope && has_name) {
        name = PyUnicode_FromString(f->name);
        if (!name)
            fail("nb::detail::nb_func_new(\"%s\"): invalid name.", f->name);

        func_prev = PyObject_GetAttr(f->scope, name);
        if (func_prev) {
            if (Py_IS_TYPE(func_prev, internals.nb_func) ||
                Py_IS_TYPE(func_prev, internals.nb_meth)) {
                func_record *fp = nb_func_get(func_prev);

                if ((fp->flags & (uint16_t) func_flags::is_method) !=
                    (f ->flags & (uint16_t) func_flags::is_method))
                    fail("nb::detail::nb_func_new(\"%s\"): mismatched static/"
                         "instance method flags in function overloads!", f->name);

                /* Never append a method to an overload chain of a parent class;
                   instead, hide the parent's overloads in this case */
                if (fp->scope != f->scope)
                    Py_CLEAR(func_prev);
            } else if (f->name[0] == '_') {
                Py_CLEAR(func_prev);
            } else {
                fail("nb::detail::nb_func_new(\"%s\"): cannot overload "
                     "existing non-function object of the same name!", f->name);
            }
        } else {
            PyErr_Clear();
        }
    }

    // Create a new function and destroy the old one
    Py_ssize_t to_copy = func_prev ? Py_SIZE(func_prev) : 0;
    nb_func *func = (nb_func *) PyType_GenericAlloc(
        is_method ? internals.nb_meth : internals.nb_func, to_copy + 1);
    if (!func)
        fail("nb::detail::nb_func_new(\"%s\"): alloc. failed (1).",
             has_name ? f->name : "<anonymous>");

    func->max_nargs_pos = f->nargs - has_var_args - has_var_kwargs;
    func->is_complex = has_args || has_var_args || has_var_kwargs;

    if (func_prev) {
        func->is_complex |= ((nb_func *) func_prev)->is_complex;
        func->max_nargs_pos = std::max(func->max_nargs_pos,
                                       ((nb_func *) func_prev)->max_nargs_pos);

        func_record *cur  = nb_func_get(func),
                    *prev = nb_func_get(func_prev);

        memcpy(cur, prev, sizeof(func_record) * to_copy);
        memset(prev, 0, sizeof(func_record) * to_copy);

        ((PyVarObject *) func_prev)->ob_size = 0;

        PyObject *func_prev_key = ptr_to_key(func_prev);
        int rv = PySet_Discard(internals.funcs, func_prev_key);
        if (rv != 1)
            fail("nanobind::detail::nb_func_new(): internal update failed (1)!");

        Py_CLEAR(func_prev_key);
        Py_CLEAR(func_prev);
    }

    func->vectorcall = func->is_complex ? nb_func_vectorcall_complex
                                        : nb_func_vectorcall_simple;

    /// Register the function
    PyObject *func_key = ptr_to_key(func);
    if (PySet_Add(internals.funcs, func_key))
        fail("nanobind::detail::nb_func_new(): internal update failed (2)!");
    Py_DECREF(func_key);

    func_record *fc = nb_func_get(func) + to_copy;
    memcpy(fc, f, sizeof(func_data<0>));
    if (has_name) {
        fc->flags |= strcmp(fc->name, "__init__") == 0
                         ? (uint16_t) func_flags::is_constructor
                         : (uint16_t) 0;
    } else {
        fc->name = "<anonymous>";
    }

    for (size_t i = 0;; ++i) {
        if (!f->descr[i]) {
            fc->descr = (char *) malloc(sizeof(char) * (i + 1));
            memcpy((char *) fc->descr, f->descr, (i + 1) * sizeof(char));
            break;
        }
    }

    for (size_t i = 0;; ++i) {
        if (!f->descr_types[i]) {
            fc->descr_types = (const std::type_info **)
                malloc(sizeof(const std::type_info *) * (i + 1));
            memcpy(fc->descr_types, f->descr_types,
                        (i + 1) * sizeof(const std::type_info *));
            break;
        }
    }

    if (has_args) {
        arg_data *args_in = std::launder((arg_data *) f->args);
        fc->args =
            (arg_data *) malloc(sizeof(arg_data) * (f->nargs + is_method));

        if (is_method) // add implicit 'self' argument annotation
            fc->args[0] = arg_data{ "self", nullptr, nullptr, false, false };
        for (size_t i = is_method; i < fc->nargs; ++i)
            fc->args[i] = args_in[i - is_method];

        for (size_t i = 0; i < fc->nargs; ++i) {
            arg_data &a = fc->args[i];
            if (a.name)
                a.name_py = PyUnicode_InternFromString(a.name);
            Py_XINCREF(a.value);
        }
    }

    if (has_scope && name) {
        int rv = PyObject_SetAttr(f->scope, name, (PyObject *) func);
        if (rv)
            fail("nb::detail::nb_func_new(\"%s\"): setattr. failed.", f->name);
    }

    Py_XDECREF(name);

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
    const size_t count = (size_t) Py_SIZE(self);
    func_record *f = nb_func_get(self);

    buf.clear();
    buf.put_dstr(f->name);
    buf.put("(): incompatible function arguments. The following argument types "
            "are supported:\n");

    for (size_t i = 0; i < count; ++i) {
        buf.put("    ");
        buf.put_uint32(i + 1);
        buf.put(". ");
        buf.put_dstr(f[i].signature ? f[i].signature : "[[ missing signature ]]");
        buf.put("\n");
    }

    buf.put("\nInvoked with types: ");
    for (size_t i = 0; i < nargs_in; ++i) {
        PyTypeObject *t = Py_TYPE(args_in[i]);
        buf.put_dstr(t->tp_name);
        if (i + 1 < nargs_in)
            buf.put(", ");
    }

    if (kwargs_in) {
        if (nargs_in)
            buf.put(", ");
        buf.put("kwargs = { ");

        size_t nkwargs_in = (ssize_t) PyTuple_GET_SIZE(kwargs_in);
        for (size_t j = 0; j < nkwargs_in; ++j) {
            PyObject *key   = PyTuple_GET_ITEM(kwargs_in, j),
                     *value = args_in[nargs_in + j];

            const char *key_cstr = PyUnicode_AsUTF8AndSize(key, nullptr);
            buf.put_dstr(key_cstr);
            buf.put(": ");
            buf.put_dstr(Py_TYPE(value)->tp_name);
            buf.put(", ");
        }
        buf.rewind(2);
        buf.put(" }");
    }

    PyErr_SetString(PyExc_TypeError, buf.get());
    return nullptr;
}

/// Used by nb_func_vectorcall: generate an error when result conversion fails
static NB_NOINLINE PyObject *nb_func_error_noconvert(const func_record *f) noexcept {
    buf.clear();
    buf.put("Unable to convert function return value to a Python "
            "type! The signature was\n    ");
    buf.put_dstr(f->signature ? f->signature
                              : "[[ missing signature ]]");
    PyErr_SetString(PyExc_TypeError, buf.get());
    return nullptr;
}

/// Used by nb_func_vectorcall: convert a C++ exception into a Python error
static NB_NOINLINE PyObject *nb_func_error_except() {
    std::exception_ptr e = std::current_exception();
    for (auto const &et : internals_get().exception_translators) {
        try {
            et(e);
            return nullptr;
        } catch (...) {
            e = std::current_exception();
#if defined(__GLIBCXX__)
        } catch (abi::__forced_unwind&) {
            throw;
#endif
        }
    }

    PyErr_SetString(PyExc_SystemError,
                    "nanobind::detail::nb_func_error_except(): exception "
                    "could not be translated!");

    return nullptr;
}

/// Dispatch loop that is used to invoke functions created by nb_func_new
PyObject *nb_func_vectorcall_complex(PyObject *self, PyObject *const *args_in,
                                     size_t nargsf, PyObject *kwargs_in) {
    const size_t count      = (size_t) Py_SIZE(self),
                 nargs_in   = (size_t) PyVectorcall_NARGS(nargsf),
                 nkwargs_in = kwargs_in ? (size_t) PyTuple_GET_SIZE(kwargs_in) : 0;

    func_record *fr = nb_func_get(self);
    PyObject *parent = nullptr;
    bool is_constructor = false;

    if (nargs_in > 0) {
        parent = args_in[0];
        if (fr->flags & (uint16_t) func_flags::is_constructor) {
            is_constructor =
                strcmp(Py_TYPE(Py_TYPE(parent))->tp_name, "nb_type") == 0;

            if (is_constructor && ((nb_inst *) parent)->destruct) {
                PyErr_SetString(PyExc_RuntimeError,
                                "nanobind::detail::nb_func_vectorcall(): the __init__ "
                                "method should only be called once!");
                return nullptr;
            }
        }
    }

    // Preallocate stack memory for function dispatch
    size_t max_nargs_pos = ((nb_func *) self)->max_nargs_pos;
    PyObject **args    = (PyObject **) alloca(max_nargs_pos * sizeof(PyObject *));
    bool *args_convert = (bool *) alloca(max_nargs_pos * sizeof(bool));
    bool *kwarg_used   = (bool *) alloca(nkwargs_in * sizeof(bool));

    object extra_args, extra_kwargs;

    /*  The logic below tries to find a suitable overload using two passes
        of the overload chain (or 1, if there are no overloads). The first pass
        is strict and permits no implicit conversions, while the second pass
        allows them.

        The following is done per overload during a pass

        1. Copy positional arguments while checking that named positional
           arguments weren't *also* specified as kwarg. Substitute missing
           entries using keyword arguments or default argument values provided
           in the bindings, if available.

        3. Ensure that either all keyword arguments were "consumed", or that
           the function takes a kwargs argument to accept unconsumed kwargs.

        4. Any positional arguments still left get put into a tuple (for args),
           and any leftover kwargs get put into a dict.

        5. Pack everything into a vector; if we have nb::args or nb::kwargs, they are an
           extra tuple or dict at the end of the positional arguments.

        6. Call the function call dispatcher (func_record::impl)

        If one of these fail, move on to the next overload and keep trying
        until we get a result other than NB_NEXT_OVERLOAD.
    */

    PyObject *result = NB_NEXT_OVERLOAD;
    for (int pass = (count > 1) ? 0 : 1; pass < 2; ++pass) {
        for (size_t k = 0; k < count; ++k) {
            const func_record *f = fr + k;

            const bool has_args       = f->flags & (uint16_t) func_flags::has_args,
                       has_var_args   = f->flags & (uint16_t) func_flags::has_var_args,
                       has_var_kwargs = f->flags & (uint16_t) func_flags::has_var_kwargs;

            /// Number of positional arguments
            size_t nargs_pos = f->nargs - has_var_args - has_var_kwargs;

            if (nargs_in > nargs_pos && !has_var_args)
                continue; // Too many positional arguments given for this overload

            if (nargs_in < nargs_pos && !has_args)
                continue; // Not enough positional arguments, insufficient
                          // keyword/default arguments to fill in the blanks

            memset(kwarg_used, 0, nkwargs_in * sizeof(bool));

            // 1. Copy positional arguments, potentially substitute kwargs/defaults
            size_t i = 0;
            for (; i < nargs_pos; ++i) {
                PyObject *arg = nullptr;
                bool arg_convert  = pass == 1,
                     arg_none     = false;

                if (i < nargs_in)
                    arg = args_in[i];

                if (has_args) {
                    const arg_data &ad = f->args[i];

                    if (kwargs_in && ad.name_py) {
                        PyObject *hit = nullptr;
                        for (size_t j = 0; j < nkwargs_in; ++j) {
                            PyObject *key = PyTuple_GET_ITEM(kwargs_in, j);
                            if (key == ad.name_py) {
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

                    arg_convert &= ad.convert;
                    arg_none = ad.none;
                }

                if (!arg || (arg == Py_None && !arg_none))
                    break;

                args[i] = arg;
                args_convert[i] = arg_convert;
            }

            // Skip this overload if positional arguments were unavailable
            if (i != nargs_pos)
                continue;

            // Deal with remaining positional arguments
            if (has_var_args) {
                extra_args = steal(PyTuple_New(
                    nargs_in > nargs_pos ? (nargs_in - nargs_pos) : 0));

                for (size_t j = nargs_pos; j < nargs_in; ++j) {
                    PyObject *o = args_in[j];
                    Py_INCREF(o);
                    PyTuple_SET_ITEM(extra_args.ptr(), j - nargs_pos, o);
                }

                args[nargs_pos] = extra_args.ptr();
                args_convert[nargs_pos] = false;
            }

            // Deal with remaining keyword arguments
            if (has_var_kwargs) {
                PyObject *dict = PyDict_New();
                for (size_t j = 0; j < nkwargs_in; ++j) {
                    PyObject *key   = PyTuple_GET_ITEM(kwargs_in, j);
                    if (!kwarg_used[j])
                        PyDict_SetItem(dict, key, args_in[nargs_in + j]);
                }

                extra_kwargs = steal(dict);
                args[nargs_pos + has_var_args] = dict;
                args_convert[nargs_pos + has_var_args] = false;
            } else if (kwargs_in) {
                bool success = true;
                for (size_t j = 0; j < nkwargs_in; ++j)
                    success = kwarg_used[j];
                if (!success)
                    continue;
            }

            try {
                // Found a suitable overload, let's try calling it
                result = f->impl((void *) f->capture, args, args_convert,
                                 (rv_policy) (f->flags & 0b111), parent);
            } catch (next_overload &) {
                result = NB_NEXT_OVERLOAD;
            } catch (python_error &e) {
                e.restore();
                return nullptr;
            } catch (...) {
                return nb_func_error_except();
            }

            if (!result)
                return nb_func_error_noconvert(f);

            if (result != NB_NEXT_OVERLOAD) {
                if (is_constructor)
                    ((nb_inst *) parent)->destruct = true;
                return result;
            }
        }
    }

    return nb_func_error_overload(self, args_in, nargs_in, kwargs_in);
}

/// Simplified nb_func_vectorcall variant for functions w/o keyword arguments
PyObject *nb_func_vectorcall_simple(PyObject *self, PyObject *const *args_in,
                                    size_t nargsf, PyObject *kwargs_in) {
    const size_t count      = (size_t) Py_SIZE(self),
                 nargs_in   = (size_t) PyVectorcall_NARGS(nargsf);

    bool *args_convert = (bool *) alloca(nargs_in * sizeof(bool));
    func_record *fr = nb_func_get(self);
    PyObject *parent = nullptr;
    bool is_constructor = false;
    PyObject *result = NB_NEXT_OVERLOAD;

    if (kwargs_in)
        goto error;

    if (nargs_in > 0) {
        parent = args_in[0];

        if (fr->flags & (uint16_t) func_flags::is_constructor) {
            is_constructor =
                strcmp(Py_TYPE(Py_TYPE(parent))->tp_name, "nb_type") == 0;

            if (is_constructor && ((nb_inst *) parent)->destruct) {
                PyErr_SetString(PyExc_RuntimeError,
                                "nanobind::detail::nb_func_vectorcall(): the __init__ "
                                "method should only be called once!");
                return nullptr;
            }
        }
    }

    for (int pass = (count > 1) ? 0 : 1; pass < 2; ++pass) {
        memset(args_convert, pass, nargs_in * sizeof(bool));

        for (size_t k = 0; k < count; ++k) {
            const func_record *f = fr + k;

            if (nargs_in != f->nargs)
                continue;

            try {
                // Found a suitable overload, let's try calling it
                result = f->impl((void *) f->capture, (PyObject **) args_in,
                                 args_convert, (rv_policy) (f->flags & 0b111),
                                 parent);
            } catch (next_overload &) {
                result = NB_NEXT_OVERLOAD;
            } catch (python_error &e) {
                e.restore();
                return nullptr;
            } catch (...) {
                return nb_func_error_except();
            }

            if (!result)
                return nb_func_error_noconvert(f);

            if (result != NB_NEXT_OVERLOAD) {
                if (is_constructor)
                    ((nb_inst *) parent)->destruct = true;
                return result;
            }
        }
    }

error:
    return nb_func_error_overload(self, args_in, nargs_in, kwargs_in);
}

PyObject *nb_meth_descr_get(PyObject *self, PyObject *inst, PyObject *) {
    if (inst) {
        /* Return a classic bound method. This should be avoidable
           in most cases via the 'CALL_METHOD' opcode and vector calls. PyTest
           rewrites the bytecode in a way that breaks this optimization :-/ */
        return PyMethod_New(self, inst);
    } else {
        Py_INCREF(self);
        return self;
    }
}


/// Finalize function signatures + docstrings once a module has finished loading
void nb_func_finalize() noexcept {
    internals &internals = internals_get();

    Py_ssize_t i = 0;
    PyObject *key;
    Py_hash_t hash;
    while (_PySet_NextEntry(internals.funcs, &i, &key, &hash)) {
        void *o = key_to_ptr(key);
        func_record *f = nb_func_get(o);
        size_t count = (size_t) Py_SIZE(o);

        for (size_t i = 0; i < count; ++i) {
            buf.clear();

            const bool is_method      = f->flags & (uint16_t) func_flags::is_method,
                       has_args       = f->flags & (uint16_t) func_flags::has_args,
                       has_var_args   = f->flags & (uint16_t) func_flags::has_var_args,
                       has_var_kwargs = f->flags & (uint16_t) func_flags::has_var_kwargs;

            const std::type_info **descr_type = f->descr_types;

            size_t arg_index = 0;
            buf.put_dstr(f->name);

            for (const char *pc = f->descr; *pc != '\0'; ++pc) {
                char c = *pc;

                switch (c) {
                    case '{':
                        // Argument name
                        if (has_var_kwargs && arg_index == f->nargs - 1) {
                            buf.put("**");
                            if (has_args && f->args[arg_index].name)
                                buf.put_dstr(f->args[arg_index].name);
                            else
                                buf.put("kwargs");
                            pc += 6;
                            break;
                        }

                        if (has_var_args && arg_index == f->nargs - 1 - has_var_kwargs) {
                            buf.put("*");
                            if (has_args && f->args[arg_index].name)
                                buf.put_dstr(f->args[arg_index].name);
                            else
                                buf.put("args");
                            pc += 4;
                            break;
                        }

                        if (has_args && f->args[arg_index].name) {
                            buf.put_dstr(f->args[arg_index].name);
                        } else if (is_method && arg_index == 0) {
                            buf.put("self");
                        } else {
                            buf.put("arg");
                            buf.put_uint32(arg_index - is_method);
                        }

                        if (!(is_method && arg_index == 0))
                            buf.put(": ");
                        break;

                    case '}':
                        // Default argument
                        if (has_args && f->args[arg_index].value) {
                            PyObject *str = PyObject_Str(f->args[arg_index].value);
                            if (str) {
                                Py_ssize_t size = 0;
                                const char *cstr =
                                    PyUnicode_AsUTF8AndSize(str, &size);
                                if (cstr) {
                                    buf.put(" = ");
                                    buf.put(cstr, (size_t) size);
                                }
                                Py_DECREF(str);
                            } else {
                                PyErr_Clear();
                            }
                        }
                        arg_index++;
                        break;

                    case '%':
                        if (!*descr_type)
                            fail("nb::detail::nb_func_finalize(): missing type!");

                        if (!(is_method && arg_index == 0)) {
                            auto it = internals.type_c2p.find(std::type_index(**descr_type));

                            if (it != internals.type_c2p.end()) {
                                handle th((PyObject *) it->second->type_py);
                                buf.put_dstr(((str) th.attr("__module__")).c_str());
                                buf.put('.');
                                buf.put_dstr(((str) th.attr("__qualname__")).c_str());
                            } else {
                                char *name = type_name(*descr_type);
                                buf.put_dstr(name);
                                free(name);
                            }
                        }

                        descr_type++;
                        break;

                    default:
                        buf.put(c);
                        break;
                }
            }

            if (arg_index != f->nargs || *descr_type != nullptr)
                fail("nanobind::detail::nb_func_finalize(): arguments inconsistent.");

            free(f->signature);
            f->signature = buf.copy();

            f++;
        }
    }
}

PyObject *nb_func_get_doc(PyObject *self, void *) {
    func_record *f = nb_func_get(self);
    size_t count = (size_t) Py_SIZE(self);

    buf.clear();

    if (count > 1) {
        buf.put_dstr((f->flags & (uint16_t) func_flags::has_name)
                         ? f->name : "<anonymous>");
        buf.put("(*args, **kwargs) -> Any\n"
                "Overloaded function.\n\n");
    }

    for (size_t i = 0; i < count; ++i) {
        if (i > 0)
            buf.put('\n');

        if (count > 1) {
            buf.put_uint32(i + 1);
            buf.put(". ");
        }

        buf.put_dstr(f->signature ? f->signature : "[[ missing signature ]]");

        if ((f->flags & (uint16_t) func_flags::has_doc) && f->doc[0] != '\0') {
            buf.put("\n\n");
            buf.put_dstr(f->doc);
            buf.put('\n');
        }

        f++;
    }

    return PyUnicode_FromString(buf.get());
}

PyObject *nb_func_get_name(PyObject *self, void *) {
    func_record *f = nb_func_get(self);
    if (f->flags & (uint16_t) func_flags::has_name) {
        return PyUnicode_FromString(f->name);
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
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
char *type_name(const std::type_info *t) {
    const char *name_in = t->name();

#if defined(__GNUG__)
    int status = 0;
    char *name = abi::__cxa_demangle(name_in, nullptr, nullptr, &status);
#else
    char *name = strdup(name_in);
    strexc(name, "class ");
    strexc(name, "struct ");
    strexc(name, "enum ");
#endif
    strexc(name, "nanobind::");
    return name;
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
