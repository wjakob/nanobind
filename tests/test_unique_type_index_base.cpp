#include "nanobind/nanobind.h"

#include "test_unique_type_index.h"

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(test_unique_type_index_base_ext, m) {
  nb::class_<Foo>(m, "Foo").def(nanobind::init<>{});
}
