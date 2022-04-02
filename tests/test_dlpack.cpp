#include <nanobind/nanobind.h>
#include <nanobind/dlpack.h>

namespace nb = nanobind;

NB_MODULE(test_dlpack_ext, m) {
    m.def("get_shape", [](const nb::tensor<> &t) {
        nb::list l;
        for (size_t i = 0; i < t.ndim(); ++i)
            l.append(t.shape(i));
        return l;
    });

    m.def("check_float", [](const nb::tensor<> &t) {
        return t.dtype() == nb::dtype<float>();
    });

    m.def("pass_float32", [](const nb::tensor<float> &) { });
    m.def("pass_uint32", [](const nb::tensor<uint32_t> &) { });
    m.def("pass_float32_shaped",
          [](const nb::tensor<float, nb::shape<3, nb::any, 4>> &) {});

    m.def("pass_float32_shaped_ordered",
          [](const nb::tensor<float, nb::order<'C'>,
                              nb::shape<nb::any, nb::any, 4>> &) {});

    m.def("check_order", [](nb::tensor<nb::order<'C'>>) -> char { return 'C'; });
    m.def("check_order", [](nb::tensor<nb::order<'F'>>) -> char { return 'F'; });
    m.def("check_order", [](nb::tensor<>) -> char { return '?'; });
}
