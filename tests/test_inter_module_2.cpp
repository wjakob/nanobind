#include <nanobind/nanobind.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/shared_ptr.h>
#include "inter_module.h"
#include "delattr_and_ensure_destroyed.h"

namespace nb = nanobind;

struct Other {};

NB_MODULE(test_inter_module_2_ext, m) {
    m.def("create_bindings", [hm = nb::handle(m)]() {
        nb::class_<Shared>(hm, "Shared");
        nb::enum_<SharedEnum>(hm, "SharedEnum")
            .value("One", SharedEnum::One)
            .value("Two", SharedEnum::Two);
    });
    m.attr("create_bindings")();

    m.def("remove_bindings", [hm = nb::handle(m)]() {
#if !defined(NB_FREE_THREADED) // nanobind types are currently immortal in FT
        delattr_and_ensure_destroyed(hm, "Shared");
        delattr_and_ensure_destroyed(hm, "SharedEnum");
#else
        (void) hm;
#endif
    });

    m.def("check_shared", &check_shared);
    m.def("check_shared_sp", &check_shared_sp);
    m.def("check_shared_up", &check_shared_up);
    m.def("check_enum", &check_enum);

    nb::register_exception_translator(
        [](const std::exception_ptr &p, void *) {
            try {
                std::rethrow_exception(p);
            } catch (const Shared &s) {
                // Instead of just calling PyErr_SetString, exercise the
                // path where one translator throws an exception to be handled
                // by another.
                throw std::range_error(
                        nb::str("Shared({})").format(s.value).c_str());
            }
        });
    m.def("throw_shared", &throw_shared);

    m.def("export_for_interop", &nb::export_for_interop);
    m.def("import_for_interop", &nb::import_for_interop<>);
    m.def("import_for_interop_explicit", &nb::import_for_interop<Shared>);
    m.def("import_for_interop_wrong_type", &nb::import_for_interop<Other>);
}
