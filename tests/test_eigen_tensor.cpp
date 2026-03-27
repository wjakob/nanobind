#include <nanobind/stl/complex.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/tensor.h>
#include <nanobind/trampoline.h>
#include <iostream>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_tensor_ext, m) {
    using DoubleTensor = Eigen::Tensor<double, 3, 0>;
    using Dims = DoubleTensor::Dimensions;
    using DimPair = DoubleTensor::DimensionPair;
    constexpr bool isEig = nb::detail::is_eigen_v<DoubleTensor>;
    constexpr bool isTens = nb::detail::is_eigen_tensor_v<DoubleTensor>;

    DoubleTensor t;
    m.def("add3dTensor", [](const DoubleTensor &a, const DoubleTensor &b) -> DoubleTensor {
        return a + b;
    }, "a"_a, "b"_a.noconvert());

    m.def("mul3dTensor", [](const double &a, const DoubleTensor &b) -> DoubleTensor {
        return a * b;
    }, "a"_a, "b"_a.noconvert());


}
