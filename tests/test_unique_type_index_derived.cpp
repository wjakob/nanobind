#include "nanobind/nanobind.h"

#include "test_unique_type_index.h"

namespace nb = nanobind;
using namespace nb::literals;

struct MyFoo: public Foo {
  MyFoo() = default;
};

NB_MODULE(test_unique_type_index_derived_ext, m) {
  nanobind::module_::import_("test_unique_type_index_base_ext");
  nb::class_<MyFoo, Foo>(m, "MyFoo").def(nanobind::init<>{});
}
