#include "smallvec.h"
#include <cstring>

#if !defined(likely)
#  if !defined(_MSC_VER)
#    define likely(x)   __builtin_expect(!!(x), 1)
#    define unlikely(x) __builtin_expect(!!(x), 0)
#  else
#    define unlikely(x) x
#    define likely(x) x
#  endif
#endif

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

struct arg_record {
    const char *name;
    bool noconvert;
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

void func_add_arg(void *rec_, const char *name, bool noconvert, bool none,
                  PyObject *def) noexcept {
    function_record *rec = (function_record *) rec_;
    if (rec->args.empty() && (rec->flags & (uint32_t) func_flags::is_method))
        rec->args.push_back(arg_record{ "self", true, false, object() });
    rec->args.push_back(
        arg_record{ name, noconvert, none, reinterpret_borrow<object>(def) });
}

static PyObject *func_dispatch(PyObject *self, PyObject *args_in, PyObject *kwargs_in) {
    function_record *rec = (function_record *) PyCapsule_GetPointer(self, nullptr);

    const bool has_overloads = rec->next != nullptr;
    const size_t nargs_in = (size_t) PyTuple_GET_SIZE(args_in);
    PyObject *parent = nargs_in > 0 ? PyTuple_GET_ITEM(args_in, 0) : nullptr;

    smallvec<PyObject *> args;
    smallvec<bool> args_convert;

    // Two-pass function resolution, allow implicit conversions only in the 2nd pass
    for (int pass = has_overloads ? 0 : 1; pass < 2; ++pass) {
        const function_record *func = rec;
        args.clear();
        args_convert.clear();

        const bool has_args = func->flags & (uint16_t) func_flags::has_args,
                   has_kwargs = func->flags & (uint16_t) func_flags::has_kwargs;

        // Number of function arguments that must be loaded, ignoring *args and **kwargs
        size_t nargs_normal = func->nargs - has_args - has_kwargs;

        /// Number of positional arguments
        size_t nargs_pos = func->nargs_pos;

        if (!has_args && nargs_in > nargs_pos)
            continue; // Too many positional arguments for this overload

        if (nargs_in < nargs_pos && func->args.size() < nargs_pos)
            continue; // Not enough positional arguments given, and not enough defaults to fill in the blanks

        size_t nargs_to_copy = nargs_pos < nargs_in ? nargs_pos : nargs_in,
               nargs_copied = 0;

        // 1. Copy any positional arguments given.
        bool bad_arg = false;
        for (; nargs_copied < nargs_to_copy; ++nargs_copied) {
            const arg_record *arg_rec = nargs_copied < rec->args.size()
                                            ? &rec->args[nargs_copied]
                                            : nullptr;

            if (unlikely(has_kwargs && arg_rec && arg_rec->name)) {
                PyObject *hit = PyDict_GetItemString(kwargs_in, arg_rec->name);
                if (hit) {
                    Py_DECREF(hit);
                    bad_arg = true;
                    break;
                }
            }

            PyObject *arg = PyTuple_GET_ITEM(args_in, nargs_copied);
            if (unlikely(arg_rec && !arg_rec->none && arg == Py_None)) {
                bad_arg = true;
                break;
            }

            args.push_back(arg);
            args_convert.push_back(
                (arg_rec && arg_rec->noconvert) ? false : (pass == 1));
        }

        if (bad_arg)
            continue;

        PyObject *result =
            func->impl((void *) func, args.data(), args_convert.data(), parent);
        if (result != NB_NEXT_OVERLOAD)
            return result;

        func++;
    }

    printf("Could not resolve overload..\n");

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *func_init(void *rec_, size_t nargs, size_t args_pos,
                    size_t kwargs_pos, void (*free_capture)(void *),
                    PyObject *(*impl)(void *, PyObject **, bool *,
                                      PyObject *) ) noexcept {
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
