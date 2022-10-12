#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(test_exception_ext, m) {
    m.def("raise_generic", [] { throw std::exception(); });
    m.def("raise_bad_alloc", [] { throw std::bad_alloc(); });
    m.def("raise_runtime_error", [] { throw std::runtime_error("a runtime error"); });
    m.def("raise_domain_error", [] { throw std::domain_error("a domain error"); });
    m.def("raise_invalid_argument", [] { throw std::invalid_argument("an invalid argument error"); });
    m.def("raise_length_error", [] { throw std::length_error("a length error"); });
    m.def("raise_out_of_range", [] { throw std::out_of_range("an out of range error"); });
    m.def("raise_range_error", [] { throw std::range_error("a range error"); });
    m.def("raise_overflow_error", [] { throw std::overflow_error("an overflow error"); });
    m.def("raise_index_error", [] { throw nb::index_error("an index error"); });
    m.def("raise_key_error", [] { throw nb::key_error("a key error"); });
    m.def("raise_value_error", [] { throw nb::value_error("a value error"); });
    m.def("raise_type_error", [] { throw nb::type_error("a type error"); });
    m.def("raise_import_error", [] { throw nb::import_error("an import error"); });
    m.def("raise_attribute_error", [] { throw nb::attribute_error("an attribute error"); });
    m.def("raise_stop_iteration", [] { throw nb::stop_iteration("a stop iteration error"); });
}
