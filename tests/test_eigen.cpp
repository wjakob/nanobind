#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/sparse.h>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_ext, m) {
    m.def(
        "addV3i_1",
        [](const Eigen::Vector3i &a,
           const Eigen::Vector3i &b) -> Eigen::Vector3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addV3i_2",
        [](const Eigen::RowVector3i &a,
           const Eigen::RowVector3i &b) -> Eigen::RowVector3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addV3i_3",
        [](const Eigen::Ref<const Eigen::Vector3i> &a,
           const Eigen::Ref<const Eigen::Vector3i> &b) -> Eigen::Vector3i {
            return a + b;
        },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addV3i_4",
        [](const Eigen::Array3i &a,
           const Eigen::Array3i &b) -> Eigen::Array3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addV3i_5",
        [](const Eigen::Array3i &a,
           const Eigen::Array3i &b) { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def("addVXi",
          [](const Eigen::VectorXi &a,
             const Eigen::VectorXi &b) -> Eigen::VectorXi { return a + b; });

    using Matrix4uC = Eigen::Matrix<uint32_t, 4, 4, Eigen::ColMajor>;
    using Matrix4uR = Eigen::Matrix<uint32_t, 4, 4, Eigen::RowMajor>;
    using MatrixXuC = Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;
    using MatrixXuR = Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    m.def("addM4u_1",
          [](const Matrix4uC &a,
             const Matrix4uC &b) -> Matrix4uC { return a + b; });
    m.def("addMXu_1",
          [](const MatrixXuC &a,
             const MatrixXuC &b) -> MatrixXuC { return a + b; });
    m.def("addMXu_1_nc",
          [](const MatrixXuC &a,
             const MatrixXuC &b) -> MatrixXuC { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());


    m.def("addM4u_2",
          [](const Matrix4uR &a,
             const Matrix4uR &b) -> Matrix4uR { return a + b; });
    m.def("addMXu_2",
          [](const MatrixXuR &a,
             const MatrixXuR &b) -> MatrixXuR { return a + b; });
    m.def("addMXu_2_nc",
          [](const MatrixXuR &a,
             const MatrixXuR &b) -> MatrixXuR { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addM4u_3",
          [](const Matrix4uC &a,
             const Matrix4uR &b) -> Matrix4uC { return a + b; });
    m.def("addMXu_3",
          [](const MatrixXuC &a,
             const MatrixXuR &b) -> MatrixXuC { return a + b; });

    m.def("addM4u_4",
          [](const Matrix4uR &a,
             const Matrix4uC &b) -> Matrix4uR { return a + b; });
    m.def("addMXu_4",
          [](const MatrixXuR &a,
             const MatrixXuC &b) -> MatrixXuR { return a + b; });

    m.def("addMXu_5",
          [](const nb::DRef<const MatrixXuC> &a,
             const nb::DRef<const MatrixXuC> &b) -> MatrixXuC { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());

    m.def("mutate_MXu", [](nb::DRef<MatrixXuC> a) { a *= 2; }, nb::arg().noconvert());

    m.def("updateV3i", [](Eigen::Ref<Eigen::Vector3i> a) { a[2] = 123; });
    m.def("updateVXi", [](Eigen::Ref<Eigen::VectorXi> a) { a[2] = 123; });

    using SparseMatrixR = Eigen::SparseMatrix<float, Eigen::RowMajor>;
    using SparseMatrixC = Eigen::SparseMatrix<float>;
    Eigen::MatrixXf mat(5, 6);
    mat <<
	 0, 3,  0, 0,  0, 11,
	22, 0,  0, 0, 17, 11,
	 7, 5,  0, 1,  0, 11,
	 0, 0,  0, 0,  0, 11,
	 0, 0, 14, 0,  8, 11;
    m.def("sparse_r", [mat]() -> SparseMatrixR {
        return Eigen::SparseView<Eigen::MatrixXf>(mat);
    });
    m.def("sparse_c", [mat]() -> SparseMatrixC {
        return Eigen::SparseView<Eigen::MatrixXf>(mat);
    });
    m.def("sparse_copy_r", [](const SparseMatrixR &m) -> SparseMatrixR { return m; });
    m.def("sparse_copy_c", [](const SparseMatrixC &m) -> SparseMatrixC { return m; });
    m.def("sparse_r_uncompressed", []() -> SparseMatrixR {
        SparseMatrixR m(2,2);
        m.coeffRef(0,0) = 1.0f;
        return m;
    });

    struct Buffer {
        uint32_t x[30] { };

        using Map = Eigen::Map<Eigen::Array<uint32_t, 10, 3>>;
        using DMap = Eigen::Map<Eigen::Array<uint32_t, Eigen::Dynamic, Eigen::Dynamic>>;

        Map map() { return Map(x); }
        DMap dmap() { return DMap(x, 10, 3); }
    };

    nb::class_<Buffer>(m, "Buffer")
        .def(nb::init<>())
        .def("map", &Buffer::map, nb::rv_policy::reference_internal)
        .def("dmap", &Buffer::dmap, nb::rv_policy::reference_internal);
}
