#include <nanobind/nanobind.h>
#include <nanobind/xtensor.h>

namespace nb = nanobind;

NB_MODULE(test_xtensor_ext, m) {
    m.def("test_add", [](const xt::xarray<double>& a, const xt::xarray<double>& b) {
        return a + b;
    });

    m.def("test_funcs", [](const xt::xarray<double>& a, const xt::xarray<double>& b) {
        return xt::sin(a) + xt::cos(b);
    });

    m.def("test_scalar", [](const xt::xarray<double>& a, double s, double t) -> xt::xarray<double> {
        return a * s + t;
    });
}
