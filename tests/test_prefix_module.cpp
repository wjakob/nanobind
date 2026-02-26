// Compiled module to demonstrate a similar issue to prefix.py/prefixabc.py.
// For some reason, there is a difference in behavior between the two.

#include "nanobind/nanobind.h"
namespace nb = nanobind;

struct Type {};

NB_MODULE(test_prefix_module, m) {
  nb::class_<Type>(m.def_submodule("prefixabc"), "Type");
  m.def_submodule("prefix").def("func", [] { return Type{}; });
  m.def("func", [] { return Type{}; });
}
