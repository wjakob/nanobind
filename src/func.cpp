#include "smallvec.h"
#include <cstring>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

/// Internal data structure which holds metadata about a bound function (signature, overloads, etc.)
struct function_record {
    /// Storage for the wrapped function pointer and captured data, if any
    void *capture[3] = { };

    /// Pointer to custom destructor for 'data' (if needed)
    void (*free_capture) (void *) = nullptr;

    /// Additional flags characterizing this function
    uint32_t flags = 0;

    // User-specified documentation string
    const char *docstr = nullptr;

    // Function name
    const char *name = nullptr;

    // Function scope (e.g. class, module)
    PyObject *scope = nullptr;

    // Predecessor method in an overload chain
    PyObject *pred = nullptr;

    /// Information about call arguments
    smallvec<const char *> args;

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

static PyObject *func_dispatch(PyObject *self, PyObject *args, PyObject *kwargs) {
    fprintf(stderr, "Got to dispatch.\n");
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *func_init(void *rec_, void (*free_capture)(void *),
                    PyObject *(*impl)(void *)) noexcept {
    function_record *rec = (function_record *) rec_;
    rec->free_capture = free_capture;

    rec->def = new PyMethodDef();
    std::memset(rec->def, 0, sizeof(PyMethodDef));

    rec->def->ml_name = rec->name;
    rec->def->ml_meth = reinterpret_cast<PyCFunction>(func_dispatch);
    rec->def->ml_flags = METH_VARARGS | METH_KEYWORDS;

    capsule rec_capsule(rec, func_free);

    handle scope_module;
    if (rec->scope) {
        scope_module = getattr(rec->scope, "__module__", handle());
        if (!scope_module.check())
            scope_module = getattr(rec->scope, "__name__", handle());
    }

    PyObject *f = PyCFunction_NewEx(rec->def, rec_capsule.ptr(), scope_module.ptr());
    if (!f)
        raise("nanobind::detail::func_init(): Could not allocate function object");

    return f;
}

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
