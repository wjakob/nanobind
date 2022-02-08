NAMESPACE_BEGIN(nanobind)

struct scope {
    handle value;
    scope(handle value) : value(value) { }
};

struct pred {
    handle value;
    pred(handle value) : value(value) { }
};

struct name {
    const char *value;
    name(const char *value) : value(value) { }
};

NAMESPACE_BEGIN(detail)

inline void func_apply(void *func_rec, const pred &pred) {
    func_set_pred(func_rec, pred.value.ptr());
}

inline void func_apply(void *func_rec, const scope &scope) {
    func_set_scope(func_rec, scope.value.ptr());
}

inline void func_apply(void *func_rec, const name &name) {
    func_set_name(func_rec, name.value);
}

inline void func_apply(void *func_rec, const char *docstr) {
    func_set_docstr(func_rec, docstr);
}

NAMESPACE_END(detail)

NAMESPACE_END(nanobind)
