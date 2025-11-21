#pragma once

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

struct nb_func;
struct func_data;

PyObject *nb_introspect_annotations(nb_func *func, const func_data *f) noexcept;
PyObject *nb_introspect_text_signature(nb_func *func, const func_data *f) noexcept;
PyObject *nb_introspect_signature(nb_func *func, const func_data *f) noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
