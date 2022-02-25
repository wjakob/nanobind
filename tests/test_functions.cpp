#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(test_functions_ext, m) {
    // Function without inputs/outputs
    m.def("test_01", []() { });

    // Simple binary function (via function pointer)
    auto test_02 = [](int j, int k) -> int { return j - k; };
    m.def("test_02", (int (*)(int, int)) test_02, "j"_a = 8, "k"_a = 1);

    // Simple binary function with capture object
    int i = 42;
    m.def("test_03", [i](int j, int k) -> int { return i + j - k; });

    // Large capture object requiring separate storage
    uint64_t k = 10, l = 11, m_ = 12, n = 13, o = 14;
    m.def("test_04", [k, l, m_, n, o]() -> int { return (int) (k + l + m_ + n + o); });

    // Overload chain with two docstrings
    m.def("test_05", [](int) -> int { return 1; }, "doc_1");
    m.def("test_05", [](float) -> int { return 2; }, "doc_2");

    /// Function raising an exception
    m.def("test_06", []() { throw std::runtime_error("oops!"); });

    /// Function taking some positional/keyword args and nb::[kw]args
    m.def("test_07", [](int, int, nb::args args, nb::kwargs kwargs) {
        return std::make_pair(args.size(), kwargs.size());
    });

    /// As above, but with nb::arg annotations
    m.def("test_07", [](int, int, nb::args args, nb::kwargs kwargs) {
        return std::make_pair(args.size(), kwargs.size());
    }, "a"_a, "b"_a, "myargs"_a, "mykwargs"_a);

    /// Test successful/unsuccessful tuple conversion
    m.def("test_tuple", []() { return nb::make_tuple("Hello", 123); });
    m.def("test_bad_tuple", []() { struct Foo{}; return nb::make_tuple("Hello", Foo()); });

    /// Perform a Python function call from C++
    m.def("test_call_1", [](nb::object o) { return o(1); });
    m.def("test_call_2", [](nb::object o) { return o(1, 2); });

    /// Test expansion of args/kwargs-style arguments
    m.def("test_call_extra", [](nb::object o, nb::args args, nb::kwargs kwargs) {
        return o(1, 2, *args, **kwargs, "extra"_a = 5);
    });

    /// Test list manipulation
    m.def("test_list", [](nb::list l) {
        int result = 0;
        for (size_t i = 0; i < l.size(); ++i)
            result += nb::cast<int>(l[i]);
        l[2] = 123;
        l.append(result);
    });

    /// Test tuple manipulation
    m.def("test_tuple", [](nb::tuple l) {
        int result = 0;
        for (size_t i = 0; i < l.size(); ++i)
            result += nb::cast<int>(l[i]);
        return result;
    });

}
