#include "smallvec.h"
#include <cstring>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

struct arg_record {
    const char *name;
    bool convert;
    bool none;
    object def;
};

/// Internal data structure which holds metadata about a bound function (signature, overloads, etc.)
struct function_record {
    /// Storage for the wrapped function pointer and captured data, if any
    void *capture[3] = { };

    /// Pointer to custom destructor for 'data' (if needed)
    void (*free_capture) (void *) = nullptr;

    /// Function implementation
    PyObject *(*impl)(void *, PyObject **, bool *, PyObject *);

    // Function name
    const char *name = nullptr;

    // User-specified documentation string
    const char *docstr = nullptr;

    /// Additional flags characterizing this function
    uint16_t flags = 0;

    /// Total number of arguments including *args and *kwargs
    uint16_t nargs = 0;

    /// Total number of positional arguments
    uint16_t nargs_pos = 0;

    /// Information about call arguments
    smallvec<arg_record> args;

    // Function scope (e.g. class, module)
    PyObject *scope = nullptr;

    // Predecessor method in an overload chain
    PyObject *pred = nullptr;

    /// Method definition (if first element of overload chain)
    PyMethodDef *def = nullptr;

    /// Pointer to next overload
    function_record *next = nullptr;
};

void *func_alloc() noexcept {
    /* If 'new' fails below, we are in deep trouble and std::terminate() will be
       invoked. This is preferable to having an func_alloc() potentially throw. */
    function_record *result = new function_record();
    return result;
}

void func_free(void *rec_) noexcept {
    function_record *rec = (function_record *) rec_;
    if (rec->free_capture)
        rec->free_capture(rec);
    delete rec->def;
    delete rec;
}

void func_set_flag(void *rec_, uint32_t flag) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->flags |= flag;
}

void func_set_scope(void *rec_, PyObject *scope) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->scope = scope;
}

void func_set_pred(void *rec_, PyObject *pred) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->pred = pred;
}

void func_set_name(void *rec_, const char *name) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->name = name;
}

void func_set_docstr(void *rec_, const char *docstr) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->docstr = docstr;
}

void func_add_arg(void *rec_, const char *name, bool convert, bool none,
                  PyObject *def) noexcept {
    function_record *rec = (function_record *) rec_;
    if (rec->args.empty() && (rec->flags & (uint32_t) func_flags::is_method))
        rec->args.push_back(arg_record{ "self", false, false, object() });
    rec->args.push_back(
        arg_record{ name, convert, none, reinterpret_borrow<object>(def) });
}

static PyObject *func_dispatch(PyObject *self, PyObject *args_in, PyObject *kwargs_in) {
    function_record *rec = (function_record *) PyCapsule_GetPointer(self, nullptr);

    const bool has_overloads = rec->next != nullptr;
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

        6. Call the function call dispatcher (function_record::impl)

        If one of these fail, move on to the next overload and keep trying
        until we get a result other than NB_NEXT_OVERLOAD.
    */

    for (int pass = has_overloads ? 0 : 1; pass < 2; ++pass) {
        const function_record *func_it = rec;

        while (func_it) {
            // Clear scratch space
            args.clear();
            args_convert.clear();
            kwargs_consumed.clear();

            // Advance the function iterator
            const function_record *func = func_it;
            func_it = func_it->next;

            const bool has_args = func->flags & (uint16_t) func_flags::has_args,
                       has_kwargs = func->flags & (uint16_t) func_flags::has_kwargs;

            /// Number of positional arguments
            size_t nargs_pos = func->nargs_pos;

            if (nargs_in > nargs_pos && !has_args)
                continue; // Too many positional arguments given for this overload

            if (nargs_in < nargs_pos && func->args.size() < nargs_pos)
                continue; // Not enough positional arguments, insufficient
                          // keyword/default arguments to fill in the blanks

            // 1. Copy positional arguments, potentially substitute kwargs/defaults
            for (size_t i = 0; i < nargs_pos; ++i) {
                PyObject *arg = nullptr;
                bool arg_convert = pass == 1;

                if (i < nargs_in)
                    arg = PyTuple_GET_ITEM(args_in, i);

                if (!rec->args.empty()) {
                    const arg_record &arg_rec = rec->args[i];

                    if (kwargs_in && arg_rec.name) {
                        PyObject *hit = PyDict_GetItemString(kwargs_in, arg_rec.name);

                        if (hit) {
                            Py_DECREF(hit); // kwargs still holds a reference

                            if (arg) {
                                // conflict between keyword and positional arg.
                                break;
                            } else {
                                arg = hit;
                                kwargs_consumed.push_back(arg_rec.name);
                            }
                        }
                    }

                    if (!arg)
                        arg = arg_rec.def.ptr();

                    if (arg == Py_None && !arg_rec.none)
                        break;

                    arg_convert &= !arg_rec.convert;
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
                extra_args = reinterpret_steal<object>(PyTuple_New(
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
                    extra_kwargs = reinterpret_borrow<object>(kwargs_in);
                } else {
                    extra_kwargs = reinterpret_steal<object>(PyDict_Copy(kwargs_in));
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
            PyObject *result =
                func->impl((void *) func, args.data(), args_convert.data(), parent);

            // Try another overload if there was a argument conversion issue
            if (result != NB_NEXT_OVERLOAD)
                return result;
        }
    }


    printf("Could not resolve overload..\n");

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *func_init(void *rec_, size_t nargs, size_t args_pos,
                    size_t kwargs_pos, void (*free_capture)(void *),
                    PyObject *(*impl)(void *, PyObject **, bool *, PyObject *),
                    const char *descr, const std::type_info **descr_types) {
    const bool has_args = args_pos != nargs,
          has_kwargs = kwargs_pos != nargs;

    function_record *rec = (function_record *) rec_;
    rec->free_capture = free_capture;

    if (!rec->args.empty() && rec->args.size() != nargs) {
        func_free(rec_);
        raise("nanobind::detail::func_init(\"%s\"): function declaration does "
              "not have the expected number of keyword arguments!", rec->name);
    }

    if (has_kwargs && kwargs_pos + 1 != nargs) {
        func_free(rec_);
        raise("nanobind::detail::func_init(\"%s\"): nanobind::kwargs must be "
              "the last element of the function signature!", rec->name);
    }

    if (has_args && has_kwargs && args_pos + 1 != kwargs_pos) {
        func_free(rec_);
        raise("nanobind::detail::func_init(\"%s\"): if nanobind::args and "
              "nanobind::kwargs are used, they must be the last elements of "
              "the function signature (in that order).", rec->name);
    }

    rec->nargs = (uint16_t) nargs;
    rec->nargs_pos = (uint16_t) (has_args ? args_pos : nargs - (has_kwargs ? 1 : 0));
    rec->flags |= (has_args ? (uint32_t) func_flags::has_args : 0) |
                  (has_kwargs ? (uint32_t) func_flags::has_kwargs : 0);
    rec->impl = impl;

    rec->def = new PyMethodDef();
    std::memset(rec->def, 0, sizeof(PyMethodDef));

    rec->def->ml_name = rec->name;
    rec->def->ml_meth = reinterpret_cast<PyCFunction>(func_dispatch);
    rec->def->ml_flags = METH_VARARGS | METH_KEYWORDS;

    capsule rec_capsule(rec, func_free);

    handle scope_module;
    if (rec->scope) {
        scope_module = getattr(rec->scope, "__module__", handle());
        if (!scope_module)
            scope_module = getattr(rec->scope, "__name__", handle());
    }

    PyObject *f = PyCFunction_NewEx(rec->def, rec_capsule.ptr(), scope_module.ptr());
    if (!f)
        raise("nanobind::detail::func_init(): Could not allocate function object");

    return f;
}

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
