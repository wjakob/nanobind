#include <nanobind/stl/complex.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/tensor.h>
#include <nanobind/trampoline.h>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_tensor_ext, m) {
    using Tensor3d = Eigen::Tensor<double, 3, 0>;
    using Tensor3i = Eigen::Tensor<int32_t, 3, 0>;
    static_assert(nb::detail::is_eigen_tensor_v<Tensor3d>);
    using RowTensor3d = Eigen::Tensor<double, 3, Eigen::RowMajor>;
    static_assert(nb::detail::make_caster<RowTensor3d>::IsRowMajor);

    using Tensor0d = Eigen::Tensor<double, 0, 0>;

    using MapTensor3d = Eigen::TensorMap<Tensor3d>; // Unaligned by default
    using MapTensor3dConst = Eigen::TensorMap<const Tensor3d>;
    static_assert(nb::detail::is_eigen_tensor_v<MapTensor3d>);
    static_assert(nb::detail::is_eigen_tensor_map_v<MapTensor3d>);

    using RefTensor3d = Eigen::TensorRef<Tensor3d>;
    static_assert(nb::detail::is_eigen_tensor_v<RefTensor3d>);
    static_assert(!nb::detail::is_eigen_tensor_map_v<RefTensor3d>);
    static_assert(nb::detail::is_eigen_tensor_ref_v<RefTensor3d>);

    // -- Plain tensor types

    m.def("add3dTensor", [](const Tensor3d &a, const Tensor3d &b) {
        return a + b;
    }, "a"_a, "b"_a);
    m.def("add3dTensor_nc", [](const Tensor3d &a, const Tensor3d &b) {
        return a + b;
    }, "a"_a.noconvert(), "b"_a.noconvert());
    m.def("square3dTensorR", [](const RowTensor3d &a) {
        return a.square();
    }, "a"_a.noconvert());

    m.def("mul3dTensor", [](double a, const Tensor3d &b) -> Tensor3d {
        return a * b;
    }, "a"_a, "b"_a);

    // -- Refs

    m.def("update3dTensorRef", [](Eigen::TensorRef<Tensor3d> a) {
        a.coeffRef(0, 0, 0) = 42.0;
    }, "a"_a.noconvert());

    // -- Maps - noconvert() is implicit

    m.def("add3dTensorCnstMap", [](MapTensor3dConst a, MapTensor3dConst b) -> Tensor3d {
        return a + b;
    }, "a"_a, "b"_a);

    m.def("castTo3iTensorMap", [](nb::object obj)  {
        return nb::cast<Eigen::TensorMap<Tensor3i, Eigen::Unaligned>>(obj);
    });

    m.def("castTo3iTensorMapCnst", [](nb::object obj)  {
        return nb::cast<Eigen::TensorMap<const Tensor3i, Eigen::Unaligned>>(obj);
    });

    m.def("castTo3iTensorMapAligned", [](nb::object obj)  {
        return nb::cast<Eigen::TensorMap<Tensor3i, Eigen::Aligned>>(obj);
    });

    m.def("castTo0dTensorMap", [](nb::object obj) {
        return nb::cast<Eigen::TensorMap<Tensor0d>>(obj);
    });

    m.def("mul3dTensorMap", [](double a, Eigen::TensorMap<const Tensor3d> b) -> Tensor3d {
        return (a * b).eval();
    }, "a"_a, "b"_a);

    m.def("mul3dTensorMapInPlace", [](double a, Eigen::TensorMap<Tensor3d> b) -> void {
        b = a * b;
    }, "a"_a, "b"_a);

    struct Buffer {
        uint32_t x[18];
        using Tensor3u = Eigen::Tensor<uint32_t, 3>;
        using Map = Eigen::TensorMap<Tensor3u>;

        Map map() { return Map(x, std::array{2, 3, 3}); }
    };

    nb::class_<Buffer>(m, "Buffer")
        .def(nb::init<>())
        .def("map", &Buffer::map, nb::rv_policy::reference_internal);

    struct ClassWithEigenMember {
        Tensor3d member;
        ClassWithEigenMember() : member(2, 1, 2) {
            member.setConstant(1.0);
        }
        const Tensor3d &get_member_ref() { return member; }
        const Tensor3d get_member_copy() { return member; }
    };

    nb::class_<ClassWithEigenMember>(m, "ClassWithEigenMember")
        .def(nb::init<>())
        .def_prop_ro("member_ro_ref", &ClassWithEigenMember::get_member_ref)
        .def_prop_ro("member_ro_copy", &ClassWithEigenMember::get_member_copy)
        .def_rw("member", &ClassWithEigenMember::member);

}
