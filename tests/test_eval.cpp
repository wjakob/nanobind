/*
    tests/test_eval.cpp -- Usage of eval() and eval_file()

    Adapted from pybind11's test_eval.cpp with

    Copyright (c) 2016 Klemens D. Morgenstern

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>

// #include "pybind11_tests.h"

// #include <utility>

namespace nb = nanobind;

NB_MODULE(test_eval_ext, m) {
    auto global = nb::dict(nb::module_::import_("__main__").attr("__dict__"));

    m.def("test_eval_statements", [global]() {
        auto local = nb::dict();
        local["call_test"] = nb::cpp_function([&]() -> int { return 42; });

        // Regular string literal
        nb::exec("message = 'Hello World!'\n"
                 "x = call_test()",
                 global,
                 local);

        // Multi-line raw string literal
        nb::exec(R"(
            if x == 42:
                print(message)
            else:
                raise RuntimeError
            )",
                 global,
                 local);
        auto x = nb::cast<int>(local["x"]);
        return x == 42;
    });

    m.def("test_eval", [global]() {
        auto local = nb::dict();
        local["x"] = nb::int_(42);
        auto x = nb::eval("x", global, local);
        return nb::cast<int>(x) == 42;
    });

    m.def("test_eval_single_statement", []() {
        auto local = nb::dict();
        local["call_test"] = nb::cpp_function([&]() -> int { return 42; });

        auto result = nb::eval<nb::eval_single_statement>("x = call_test()", nb::dict(), local);
        auto x = nb::cast<int>(local["x"]);
        return result.is_none() && x == 42;
    });

    m.def("test_eval_failure", []() {
        try {
            nb::eval("nonsense code ...");
        } catch (nb::python_error &) {
            return true;
        }
        return false;
    });

    // test_eval_empty_globals
    m.def("eval_empty_globals", [](nb::dict global) {
        // if (global.is_none()) {
        //     global = nb::dict();
        // }
        auto int_class = nb::eval("isinstance(42, int)", global);
        return global;
    });

    // test_eval_closure
    m.def("test_eval_closure", []() {
        nb::dict global;
        global["closure_value"] = 42;
        nb::dict local;
        local["closure_value"] = 0;
        nb::exec(R"(
            local_value = closure_value

            def func_global():
                return closure_value

            def func_local():
                return local_value
            )",
                 global,
                 local);
        return std::make_pair(global, local);
    });
}
