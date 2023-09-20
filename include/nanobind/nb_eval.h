/*
    nanobind/nb_eval.h: Support for evaluating Python expressions and statements
                        from strings and files

    Adapted by Nico Schlömer from pybind11's eval.h with

    Copyright (c) 2016 Klemens Morgenstern <klemens.morgenstern@ed-chemnitz.de> and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>

#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)

enum eval_mode {
    /// Evaluate a string containing an isolated expression
    eval_expr,

    /// Evaluate a string containing a single statement. Returns \c none
    eval_single_statement,

    /// Evaluate a string containing a sequence of statement. Returns \c none
    eval_statements
};

template <eval_mode mode = eval_expr>
object eval(const str &expr, dict global = globals(), object local = object()) {
    if (!local) {
        local = global;
    }

    int start = 0;
    switch (mode) {
        case eval_expr:
            start = Py_eval_input;
            break;
        case eval_single_statement:
            start = Py_single_input;
            break;
        case eval_statements:
            start = Py_file_input;
            break;
        default:
            detail::fail("invalid evaluation mode");
    }

    // This used to be PyRun_String, but that function isn't in the stable ABI.
    PyObject *codeobj = Py_CompileString(expr.c_str(), "<string>", start);
    if (!codeobj)
    {
        throw python_error();
    }
    PyObject *result = PyEval_EvalCode(codeobj, global.ptr(), local.ptr());
    if (!result) {
        throw python_error();
    }

    return steal<object>(result);
}

template <eval_mode mode = eval_expr, size_t N>
object eval(const char (&s)[N], dict global = globals(), object local = object()) {
    /* Support raw string literals by removing common leading whitespace */
    auto expr = (s[0] == '\n') ? str(module_::import_("textwrap").attr("dedent")(s)) : str(s);
    return eval<mode>(expr, std::move(global), std::move(local));
}

inline void exec(const str &expr, dict global = globals(), object local = object()) {
    eval<eval_statements>(expr, std::move(global), std::move(local));
}

template <size_t N>
void exec(const char (&s)[N], dict global = globals(), object local = object()) {
    eval<eval_statements>(s, std::move(global), std::move(local));
}

NAMESPACE_END(NB_NAMESPACE)
