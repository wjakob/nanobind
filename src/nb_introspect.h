#pragma once

#include <nanobind/nanobind.h>
#include <string>
#include <vector>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

struct nb_func;
struct func_data;

struct pep_metadata_param {
    std::string name;
    std::string annotation;
};

struct pep_metadata {
    std::vector<pep_metadata_param> parameters;
    std::vector<std::string> sanitized_tokens;
    std::string return_type;
};

bool nb_collect_pep_metadata(const func_data *f, pep_metadata &out) noexcept;
PyObject *nb_introspect_annotations(nb_func *func, const func_data *f) noexcept;
PyObject *nb_introspect_text_signature(nb_func *func, const func_data *f) noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
