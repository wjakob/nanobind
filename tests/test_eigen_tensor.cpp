#include <nanobind/stl/complex.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/tensor.h>
#include <nanobind/trampoline.h>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_tensor_ext, m) {
    using Tensor3d = Eigen::Tensor<double, 3, 0>;
    static_assert(nb::detail::is_eigen_tensor_v<Tensor3d>);
    using RowTensor3d = Eigen::Tensor<double, 3, Eigen::RowMajor>;
    static_assert(nb::detail::make_caster<RowTensor3d>::IsRowMajor);

    using MapTensor3d = Eigen::TensorMap<Tensor3d>;
    static_assert(nb::detail::is_eigen_tensor_v<MapTensor3d>);
    static_assert(nb::detail::is_eigen_tensor_map_v<MapTensor3d>);

    using RefTensor3d = Eigen::TensorRef<Tensor3d>;
    static_assert(nb::detail::is_eigen_tensor_v<RefTensor3d>);
    static_assert(!nb::detail::is_eigen_tensor_map_v<RefTensor3d>);
    static_assert(nb::detail::is_eigen_tensor_ref_v<RefTensor3d>);

    // -- Plain tensor types

    m.def("add3dTensor", [](const Tensor3d &a, const Tensor3d &b) -> Tensor3d {
        return a + b;
    }, "a"_a, "b"_a);
    m.def("add3dTensor_nc", [](const Tensor3d &a, const Tensor3d &b) -> Tensor3d {
        return a + b;
    }, "a"_a.noconvert(), "b"_a.noconvert());
    m.def("square3dTensorR", [](const RowTensor3d &a) -> RowTensor3d {
        return a.square();
    }, "a"_a.noconvert());

    m.def("mul3dTensor", [](double a, const Tensor3d &b) -> Tensor3d {
        auto c = (a * b).eval();
        return c;
    }, "a"_a, "b"_a);

    // -- Maps - noconvert() is implicit

    m.def("mul3dTensorMap", [](double a, Eigen::TensorMap<const Tensor3d> b) -> Tensor3d {
        return (a * b).eval();
    }, "a"_a, "b"_a);

    m.def("mul3dTensorMapInPlace", [](double a, Eigen::TensorMap<Tensor3d> b) -> void {
        b = a * b;
    }, "a"_a, "b"_a);

}
