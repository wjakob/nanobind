#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/unordered_map.h>

namespace nb = nanobind;

NB_MAKE_OPAQUE(std::map<std::string, double>);
NB_MAKE_OPAQUE(std::unordered_map<std::string, double>);
NB_MAKE_OPAQUE(std::map<std::string, double const>);
NB_MAKE_OPAQUE(std::unordered_map<std::string, double const>);

NB_MODULE(test_bind_map_ext, m) {
    // test_map_string_double
    nb::bind_map<std::map<std::string, double>>(m, "MapStringDouble");
    nb::bind_map<std::unordered_map<std::string, double>>(m, "UnorderedMapStringDouble");
    // test_map_string_double_const
    nb::bind_map<std::map<std::string, double const>>(m, "MapStringDoubleConst");
    nb::bind_map<std::unordered_map<std::string, double const>>(m,
                                                                "UnorderedMapStringDoubleConst");
}
