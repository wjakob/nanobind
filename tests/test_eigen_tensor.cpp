#include <nanobind/stl/complex.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/tensor.h>
#include <nanobind/trampoline.h>
#include <iostream>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_tensor_ext, m) {
    using Tensor3d = Eigen::Tensor<double, 3, 0>;
    using RowTensor3d = Eigen::Tensor<double, 3, 1>;
    using Dims = Tensor3d::Dimensions;
    using DimPair = Tensor3d::DimensionPair;
    static_assert(nb::detail::make_caster<RowTensor3d>::IsRowMajor); 

    m.def("add3dTensor", [](const Tensor3d &a, const Tensor3d &b) -> Tensor3d {
        auto c = (a + b).eval();
        return c;
    }, "a"_a, "b"_a.noconvert());

    m.def("mul3dTensor", [](const double &a, const Tensor3d &b) -> Tensor3d {
        auto c = (a * b).eval();
        return c;
    }, "a"_a, "b"_a.noconvert());


}
