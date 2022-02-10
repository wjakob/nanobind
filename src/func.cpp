#include "smallvec.h"
#include "buffer.h"
#include <cstring>
#include <tsl/robin_set.h>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

// Forward/external declarations
extern Buffer buf;
static PyObject *func_dispatch(PyObject *, PyObject *, PyObject *);

/// Nanobind function metadata (signature, overloads, etc.)
struct func_record : func_data<0> {
    arg_data *args = nullptr;

    /// Method definition (if first element of overload chain)
    PyMethodDef *def = nullptr;

    /// Pointer to next overload
    func_record *next = nullptr;
};

/// List of all functions for docstring generation
static tsl::robin_pg_set<func_record *> all_funcs;

void func_free(void *p) noexcept {
    func_record *f = (func_record *) p;
    if (f->free_capture)
        f->free_capture(f->capture);
    for (size_t i = 0; f->nargs_provided; ++i)
        Py_XDECREF(f->args[i].value);
    delete[] f->args;
    delete[] f->descr;
    delete[] f->descr_types;
    delete f->def;
    delete f;
    all_funcs.erase(f);
}

PyObject *func_init(void *in_) noexcept {
    func_data<0> *in = std::launder((func_data<0> *) in_);
    func_record *f = new func_record();

    /// Copy temporary data from the caller's stack
    std::memcpy(f, in, sizeof(func_data<0>));

    for (size_t i = 0; ; ++i) {
        if (!f->descr[i]) {
            char *descr = new char[i + 1];
            std::memcpy(descr, f->descr, (i + 1) * sizeof(char));
            f->descr = descr;
            break;
        }
    }

    for (size_t i = 0; ; ++i) {
        if (!f->descr_types[i]) {
            const std::type_info **descr_types =
                new const std::type_info *[i + 1];
            std::memcpy(descr_types, f->descr_types,
                        (i + 1) * sizeof(const std::type_info *));
            f->descr_types = descr_types;
            break;
        }
    }

    if (f->nargs_provided) {
        const bool is_method = f->flags & (uint32_t) func_flags::is_method;
        arg_data *args_in = std::launder((arg_data *) in->args);

        f->args = new arg_data[f->nargs_provided + is_method];

        if (is_method) // add implicit 'self' argument annotation
            f->args[0] = arg_data{ "self", nullptr, false, false };

        for (size_t i = 0; i < f->nargs_provided; ++i) {
            f->args[i + is_method] = args_in[i];
            Py_XINCREF(f->args[i].value);
        }
    }

    f->def = new PyMethodDef();
    std::memset(f->def, 0, sizeof(PyMethodDef));
    f->def->ml_name = f->name;
    f->def->ml_meth = reinterpret_cast<PyCFunction>(func_dispatch);
    f->def->ml_flags = METH_VARARGS | METH_KEYWORDS;

    capsule f_capsule(f, func_free);

    PyObject *scope_name = nullptr;
    if (f->scope) {
        scope_name = getattr(f->scope, "__module__", nullptr);
        if (!scope_name)
            scope_name = getattr(f->scope, "__name__", nullptr);
    }

    PyObject *r = PyCFunction_NewEx(f->def, f_capsule.ptr(), scope_name);
    if (!r)
        fail("nanobind::detail::func_init(\"%s\"): alloc. failed.", f->name);

    all_funcs.insert(f);

    return r;
}

static PyObject *func_dispatch(PyObject *self, PyObject *args_in, PyObject *kwargs_in) {
    func_record *fr = (func_record *) PyCapsule_GetPointer(self, nullptr);

    const bool has_overloads = fr->next != nullptr;
    const size_t nargs_in = (size_t) PyTuple_GET_SIZE(args_in);
    PyObject *parent = nargs_in > 0 ? PyTuple_GET_ITEM(args_in, 0) : nullptr;

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

    for (int pass = has_overloads ? 0 : 1; pass < 2; ++pass) {
        const func_record *it = fr;

        while (it) {
            // Clear scratch space
            args.clear();
            args_convert.clear();
            kwargs_consumed.clear();

            // Advance the function iterator
            const func_record *f = it;
            it = it->next;

            const bool has_args   = f->flags & (uint32_t) func_flags::has_args,
                       has_kwargs = f->flags & (uint32_t) func_flags::has_kwargs;

            /// Number of positional arguments
            size_t nargs_pos = f->nargs - has_args - has_kwargs;

            if (nargs_in > nargs_pos && !has_args)
                continue; // Too many positional arguments given for this overload

            if (nargs_in < nargs_pos && f->nargs_provided == 0)
                continue; // Not enough positional arguments, insufficient
                          // keyword/default arguments to fill in the blanks

            // 1. Copy positional arguments, potentially substitute kwargs/defaults
            for (size_t i = 0; i < nargs_pos; ++i) {
                PyObject *arg = nullptr;
                bool arg_convert = pass == 1;

                if (i < nargs_in)
                    arg = PyTuple_GET_ITEM(args_in, i);

                if (f->nargs_provided) {
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

                    if (arg == Py_None && !ad.none)
                        break;

                    arg_convert &= !ad.convert;
                }

                if (!arg)
                    break;

                args.push_back(arg);
                args_convert.push_back(arg_convert);
            }

            // Skip this overload if positional arguments were unavailable
            if (args.size() != nargs_pos)
                continue;

            // Deal with remaining positional arguments
            if (has_args) {
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
            if (has_kwargs) {
                if (kwargs_consumed.empty()) {
                    extra_kwargs = borrow(kwargs_in);
                } else {
                    extra_kwargs = steal(PyDict_Copy(kwargs_in));
                    for (size_t i = 0; i < kwargs_consumed.size(); ++i) {
                        if (PyDict_DelItemString(extra_kwargs.ptr(),
                                                 kwargs_consumed[i]) == -1)
                            fail("nanobind::detail::func_dispatch(): could not "
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
            // Found a suitable overload, let's try calling it
            PyObject *result = f->impl((void *) f->capture, args.data(),
                                       args_convert.data(), parent);

            // Try another overload if there was a argument conversion issue
            if (result != NB_NEXT_OVERLOAD)
                return result;
        }
    }

    printf("Could not resolve overload..\n");

    Py_INCREF(Py_None);
    return Py_None;
}

void func_make_docstr() {
    for (func_record *f: all_funcs) {
        buf.clear();
        size_t arg_index = 0;
        const bool is_method  = f->flags & (uint32_t) func_flags::is_method;
        const std::type_info **descr_type = f->descr_types;

        for (const char *pc = f->descr; *pc != '\0'; ++pc) {
            char c = *pc;

            switch (c) {
                case '{':
                    // Argument name
                    if (f->nargs_provided && f->args[arg_index].name) {
                        buf.put_dstr(f->args[arg_index].name);
                    } else if (is_method && arg_index == 0) {
                        buf.put("self");
                    } else {
                        buf.put("arg");
                        buf.put_uint32(arg_index - is_method);
                    }
                    buf.put(": ");
                    break;

                case '}':
                    // Default argument
                    if (f->nargs_provided && f->args[arg_index].value) {
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
                        fail("nanobind::detail::func_make_docstr(): missing type!");
                    descr_type++;
                    break;

                default:
                    buf.put(c);
                    break;
            }
        }

        if (arg_index != f->nargs || *descr_type != nullptr)
            fail("nanobild::detail::func_make_docstr(): internal error.");

        free((char *) f->def->ml_doc);
        f->def->ml_doc = buf.copy();
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
