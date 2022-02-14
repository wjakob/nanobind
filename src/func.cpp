#include "smallvec.h"
#include "buffer.h"
#include "internals.h"

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Forward/external declarations
extern Buffer buf;
static char *type_name(const std::type_info *t);

/// Fetch the nanobind function record from a 'nbfunc' instance
static func_record *nbfunc_get(void *o) {
    return (func_record *) (((char *) o) + sizeof(PyVarObject));
}

/// Free a function overload chain
void nbfunc_dealloc(PyObject *self) noexcept {
    Py_ssize_t size = Py_SIZE(self);

    if (size) {
        func_record *f = nbfunc_get(self);

        // Delete from registered function list
        auto &funcs = get_internals().funcs;
        auto it = funcs.find(self);
        if (it == funcs.end()) {
            const char *name = (f->flags & (uint16_t) func_flags::has_name)
                                   ? f->name : "<anonymous>";
            fail("nanobind::detail::nbfunc_dealloc(\"%s\"): function not found!",
                 name);
        }
        funcs.erase(it);

        for (Py_ssize_t i = 0; i < size; ++i) {
            if (f->flags & (uint16_t) func_flags::has_free)
                f->free(f->capture);

            if (f->flags & (uint16_t) func_flags::has_args) {
                for (size_t i = 0; i< f->nargs; ++i)
                    Py_XDECREF(f->args[i].value);
            }

            free(f->args);
            free((char *) f->descr);
            free(f->descr_types);
            free(f->signature);
            ++f;
        }
    }

    Py_TYPE(self)->tp_free(self);
}

/**
 * \brief Wrap a C++ function into a Python function object
 *
 * This is an implementation detail of nanobind::cpp_function.
 */
PyObject *nbfunc_new(const void *in_) noexcept {
    func_data<0> *f = (func_data<0> *) in_;

    const bool has_scope  = f->flags & (uint16_t) func_flags::has_scope,
               has_name   = f->flags & (uint16_t) func_flags::has_name,
               has_args   = f->flags & (uint16_t) func_flags::has_args,
               is_method  = f->flags & (uint16_t) func_flags::is_method,
               return_ref = f->flags & (uint16_t) func_flags::return_ref;

    PyObject *name = nullptr;
    PyObject *func_prev = nullptr;
    internals &internals = get_internals();

    // Check for previous overloads
    if (has_scope && has_name) {
        name = PyUnicode_FromString(f->name);
        if (!name)
            fail("nb::detail::nbfunc_new(\"%s\"): invalid name.", f->name);

        func_prev = PyObject_GetAttr(f->scope, name);
        if (func_prev) {
            if (Py_IS_TYPE(func_prev, internals.nbfunc)) {
                func_record *fp = nbfunc_get(func_prev);

                uint16_t mask = (uint16_t) func_flags::is_method |
                                (uint16_t) func_flags::is_static;

                if ((fp->flags & mask) != (f->flags & mask))
                    fail("nb::detail::nbfunc_new(\"%s\"): mismatched static/"
                         "instance method flags in function overloads!", f->name);

                /* Never append a method to an overload chain of a parent class;
                   instead, hide the parent's overloads in this case */
                if (fp->scope != f->scope) {
                    Py_DECREF(func_prev);
                    func_prev = nullptr;
                }
            } else if (f->name[0] != '_') {
                fail("nb::detail::nbfunc_new(\"%s\"): cannot overload "
                     "existing non-function object of the same name!", f->name);
            }
        } else {
            PyErr_Clear();
        }
    }

    // Create a new function and destroy the old one
    Py_ssize_t to_copy = func_prev ? Py_SIZE(func_prev) : 0;
    PyObject *func = PyType_GenericAlloc(internals.nbfunc, to_copy + 1);
    if (!func)
        fail("nb::detail::nbfunc_new(\"%s\"): alloc. failed (1).",
             has_name ? f->name : "<anonymous>");

    if (func_prev) {
        func_record *cur  = nbfunc_get(func),
                    *prev = nbfunc_get(func_prev);

        memcpy(cur, prev, sizeof(func_record) * to_copy);
        memset(prev, 0, sizeof(func_record) * to_copy);

        auto it = internals.funcs.find(func_prev);
        if (it == internals.funcs.end())
            fail("nanobind::detail::nbfunc_new(\"%s\"): previous function not "
                 "found!", has_name ? f->name : "<anonymous>");
        internals.funcs.erase(it);

        ((PyVarObject *) func_prev)->ob_size = 0;
    }

    internals.funcs.insert(func);

    func_record *fc = nbfunc_get(func) + to_copy;
    memcpy(fc, f, sizeof(func_data<0>));
    if (!has_name)
        fc->name = "<anonymous>";

    char *descr;
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
            fc->args[0] = arg_data{ "self", nullptr, false, false };

        for (size_t i = is_method; i < fc->nargs; ++i) {
            fc->args[i] = args_in[i - is_method];
            Py_XINCREF(fc->args[i].value);
        }
    }

    if (has_scope && name) {
        int rv = PyObject_SetAttr(f->scope, name, func);
        if (rv)
            fail("nb::detail::nbfunc_new(\"%s\"): setattr. failed.", f->name);
    }

    Py_XDECREF(name);
    Py_XDECREF(func_prev);

    if (return_ref) {
        return func;
    } else {
        Py_DECREF(func);
        return nullptr;
    }
}

/// Dispatch loop that is used to invoke functions created by nbfunc_new
PyObject *nbfunc_call(PyObject *self, PyObject *args_in, PyObject *kwargs_in) {
    const size_t count = (size_t) Py_SIZE(self),
                 nargs_in = (size_t) PyTuple_GET_SIZE(args_in);

    func_record *fr = nbfunc_get(self);

    PyObject *parent = nargs_in > 0 ? PyTuple_GET_ITEM(args_in, 0) : nullptr;
    bool is_constructor = strcmp(fr->name, "__init__") == 0;

    internals &internals = get_internals();
    if (is_constructor) {
        is_constructor &= parent && Py_TYPE(Py_TYPE(parent)) == internals.nbtype;

        if (is_constructor && ((nb_inst *) parent)->destruct) {
            PyErr_SetString(PyExc_RuntimeError,
                            "nanobind::detail::nbfunc_dispatch(): the __init__ "
                            "method should only be called once!");
            return nullptr;
        }
    }

    smallvec<PyObject *> args;
    smallvec<const char *> kwargs_consumed;
    smallvec<bool> args_convert;
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
        for (size_t i = 0; i < count; ++i) {
            // Clear scratch space
            args.clear();
            args_convert.clear();
            kwargs_consumed.clear();

            // Advance the function iterator
            const func_record *f = fr + i;

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

            // 1. Copy positional arguments, potentially substitute kwargs/defaults
            for (size_t i = 0; i < nargs_pos; ++i) {
                PyObject *arg = nullptr;
                bool arg_convert  = pass == 1,
                     arg_none     = false;

                if (i < nargs_in)
                    arg = PyTuple_GET_ITEM(args_in, i);

                if (has_args) {
                    const arg_data &ad = f->args[i];

                    if (kwargs_in && ad.name) {
                        PyObject *hit = PyDict_GetItemString(kwargs_in, ad.name);

                        if (hit) {
                            Py_DECREF(hit); // kwargs still holds a reference

                            if (arg) {
                                // conflict between keyword and positional arg.
                                break;
                            } else {
                                arg = hit;
                                kwargs_consumed.push_back(ad.name);
                            }
                        }
                    }

                    if (!arg)
                        arg = ad.value;

                    arg_convert &= ad.convert;
                    arg_none = ad.none;
                }

                if (!arg || (arg == Py_None && !arg_none))
                    break;

                args.push_back(arg);
                args_convert.push_back(arg_convert);
            }

            // Skip this overload if positional arguments were unavailable
            if (args.size() != nargs_pos)
                continue;

            // Deal with remaining positional arguments
            if (has_var_args) {
                extra_args = steal(PyTuple_New(
                    nargs_in > nargs_pos ? (nargs_in - nargs_pos) : 0));

                for (size_t i = nargs_pos; i < nargs_in; ++i) {
                    PyObject *o = PyTuple_GET_ITEM(args_in, i);
                    Py_INCREF(o);
                    PyTuple_SET_ITEM(extra_args.ptr(), i - nargs_pos, o);
                }

                args.push_back(extra_args.ptr());
                args_convert.push_back(false);
            }

            // Deal with remaining keyword arguments
            if (has_var_kwargs) {
                if (kwargs_consumed.empty()) {
                    if (kwargs_in)
                        extra_kwargs = borrow(kwargs_in);
                    else
                        extra_kwargs = steal(PyDict_New());
                } else {
                    extra_kwargs = steal(PyDict_Copy(kwargs_in));
                    for (size_t i = 0; i < kwargs_consumed.size(); ++i) {
                        if (PyDict_DelItemString(extra_kwargs.ptr(),
                                                 kwargs_consumed[i]) == -1)
                            fail("nb::detail::nbfunc_call(): could not "
                                 "filter kwargs");
                    }
                }

                args.push_back(extra_kwargs.ptr());
                args_convert.push_back(false);
            } else if (kwargs_in && kwargs_consumed.size() !=
                                        (size_t) PyDict_GET_SIZE(kwargs_in)) {
                // Not all keyword arguments were consumed, try next overload
                continue;
            }

            try {
                // Found a suitable overload, let's try calling it
                result = f->impl((void *) f->capture, args.data(),
                                 args_convert.data(),
                                 (rv_policy) (f->flags & 0b111), parent);
            } catch (python_error &e) {
                e.restore();
                return nullptr;
#if defined(__GLIBCXX__)
            } catch (abi::__forced_unwind&) {
                throw;
#endif
            } catch (next_overload &) {
                result = NB_NEXT_OVERLOAD;
            } catch (...) {
                auto &translators = internals.exception_translators;

                std::exception_ptr exc = std::current_exception();
                for (size_t i = 0; i < translators.size(); ++i) {
                    try {
                        translators[i](exc);
                        return nullptr;
                    } catch (...) {
                        exc = std::current_exception();
                    }
                }

                PyErr_SetString(PyExc_SystemError,
                                "nanobind::detail::nbfunc_call(): exception "
                                "could not be translated!");

                return nullptr;
            }

            if (!result) {
                buf.clear();
                buf.put("Unable to convert function return value to a Python "
                        "type! The signature was\n    ");
                buf.put_dstr(f->signature ? f->signature
                                          : "[[ missing signature ]]");
                PyErr_SetString(PyExc_TypeError, buf.get());
                return nullptr;
            }

            if (result != NB_NEXT_OVERLOAD) {
                if (is_constructor)
                    ((nb_inst *) parent)->destruct = true;
                return result;
            }
        }
    }

    buf.clear();
    buf.put_dstr(fr->name);
    buf.put("(): incompatible function arguments. The following argument types "
            "are supported:\n");

    for (size_t i = 0; i < count; ++i) {
        buf.put("    ");
        buf.put_uint32(i + 1);
        buf.put(". ");
        buf.put_dstr(fr[i].signature ? fr[i].signature : "[[ missing signature ]]");
        buf.put("\n");
    }

    buf.put("\nInvoked with types: ");
    for (size_t i = 0; i < nargs_in; ++i) {
        PyTypeObject *t = Py_TYPE(PyTuple_GET_ITEM(args_in, i));
        buf.put_dstr(t->tp_name);
        if (i + 1 < nargs_in)
            buf.put(", ");
    }

    if (kwargs_in) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;

        if (nargs_in)
            buf.put(", ");
        buf.put("kwargs = { ");
        while (PyDict_Next(kwargs_in, &pos, &key, &value)) {
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

/// Finalize function signatures + docstrings once a module has finished loading
void nbfunc_finalize() noexcept {
    internals &internals = get_internals();

    for (PyObject *o: internals.funcs) {
        func_record *f = nbfunc_get(o);
        size_t count = (size_t) Py_SIZE(o);

        for (size_t i = 0; i < count; ++i) {
            buf.clear();

            const bool is_method      = f->flags & (uint16_t) func_flags::is_method,
                       has_args       = f->flags & (uint16_t) func_flags::has_args,
                       has_var_args   = f->flags & (uint16_t) func_flags::has_var_args,
                       has_var_kwargs = f->flags & (uint16_t) func_flags::has_var_kwargs,
                       has_doc        = f->flags & (uint16_t) func_flags::has_doc;

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
                            fail("nb::detail::nbfunc_finalize(): missing type!");

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
                fail("nanobind::detail::nbfunc_finalize(): arguments inconsistent.");

            free(f->signature);
            f->signature = buf.copy();

            f++;
        }
    }
}

PyObject *nbfunc_get_doc(PyObject *o, void *) {
    func_record *f = nbfunc_get(o);
    size_t count = (size_t) Py_SIZE(o);

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

PyObject *nbfunc_get_name(PyObject *o, void *) {
    func_record *f = nbfunc_get(o);
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
static char *type_name(const std::type_info *t) {
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
