#include <nanobind/eigen/dense.h>
#include <nanobind/eigen/sparse.h>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(test_eigen_ext, m) {
    m.def(
        "addV3i",
        [](const Eigen::Vector3i &a,
           const Eigen::Vector3i &b) -> Eigen::Vector3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addR3i",
        [](const Eigen::RowVector3i &a,
           const Eigen::RowVector3i &b) -> Eigen::RowVector3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addRefV3i",
        [](const Eigen::Ref<const Eigen::Vector3i> &a,
           const Eigen::Ref<const Eigen::Vector3i> &b) -> Eigen::Vector3i {
            return a + b;
        },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addA3i",
        [](const Eigen::Array3i &a,
           const Eigen::Array3i &b) -> Eigen::Array3i { return a + b; },
        "a"_a, "b"_a.noconvert());

    m.def(
        "addA3i_retExpr",
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

    m.def("addM4uCC",
          [](const Matrix4uC &a,
             const Matrix4uC &b) -> Matrix4uC { return a + b; });
    m.def("addMXuCC",
          [](const MatrixXuC &a,
             const MatrixXuC &b) -> MatrixXuC { return a + b; });
    m.def("addMXuCC_nc",
          [](const MatrixXuC &a,
             const MatrixXuC &b) -> MatrixXuC { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());


    m.def("addM4uRR",
          [](const Matrix4uR &a,
             const Matrix4uR &b) -> Matrix4uR { return a + b; });
    m.def("addMXuRR",
          [](const MatrixXuR &a,
             const MatrixXuR &b) -> MatrixXuR { return a + b; });
    m.def("addMXuRR_nc",
          [](const MatrixXuR &a,
             const MatrixXuR &b) -> MatrixXuR { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addM4uCR",
          [](const Matrix4uC &a,
             const Matrix4uR &b) -> Matrix4uC { return a + b; });
    m.def("addMXuCR",
          [](const MatrixXuC &a,
             const MatrixXuR &b) -> MatrixXuC { return a + b; });

    m.def("addM4uRC",
          [](const Matrix4uR &a,
             const Matrix4uC &b) -> Matrix4uR { return a + b; });
    m.def("addMXuRC",
          [](const MatrixXuR &a,
             const MatrixXuC &b) -> MatrixXuR { return a + b; });

    m.def("addMapMXuCC_nc",
      [](const Eigen::Map<const MatrixXuC>& a,
         const Eigen::Map<const MatrixXuC>& b) -> MatrixXuC { return a + b; },
      "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addMapMXuRR_nc",
      [](const Eigen::Map<const MatrixXuR>& a,
         const Eigen::Map<const MatrixXuR>& b) -> MatrixXuC { return a + b; },
      "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addRefMXuCC_nc",
      [](const Eigen::Ref<const MatrixXuC>& a,
         const Eigen::Ref<const MatrixXuC>& b) -> MatrixXuC { return a + b; },
      "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addRefMXuRR_nc",
      [](const Eigen::Ref<const MatrixXuR>& a,
         const Eigen::Ref<const MatrixXuR>& b) -> MatrixXuC { return a + b; },
      "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addDRefMXuCC_nc",
          [](const nb::DRef<const MatrixXuC> &a,
             const nb::DRef<const MatrixXuC> &b) -> MatrixXuC { return a + b; },
          "a"_a.noconvert(), "b"_a.noconvert());

    m.def("addDRefMXuRR_nc",
      [](const nb::DRef<const MatrixXuR>& a,
         const nb::DRef<const MatrixXuR>& b) -> MatrixXuC { return a + b; },
      "a"_a.noconvert(), "b"_a.noconvert());

    m.def("mutate_DRefMXuC", [](nb::DRef<MatrixXuC> a) { a *= 2; }, nb::arg().noconvert());

    m.def("updateRefV3i", [](Eigen::Ref<Eigen::Vector3i> a) { a[2] = 123; });
    m.def("updateRefV3i_nc", [](Eigen::Ref<Eigen::Vector3i> a) { a[2] = 123; }, nb::arg().noconvert());
    m.def("updateRefVXi", [](Eigen::Ref<Eigen::VectorXi> a) { a[2] = 123; });
    m.def("updateRefVXi_nc", [](Eigen::Ref<Eigen::VectorXi> a) { a[2] = 123; }, nb::arg().noconvert());

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
        assert(!m.isCompressed());
        return m.markAsRValue();
    });

    /// issue #166
    using Matrix1d = Eigen::Matrix<double,1,1>;
    try {
        m.def(
            "default_arg", [](Matrix1d a, Matrix1d b) { return a + b; },
            "a"_a = Matrix1d::Zero(), "b"_a = Matrix1d::Zero());
    } catch (...) {
        // Ignore (NumPy not installed, etc.)
    }

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


    struct ClassWithEigenMember {
        Eigen::MatrixXd member = Eigen::Matrix2d::Ones();
    };

    nb::class_<ClassWithEigenMember>(m, "ClassWithEigenMember")
        .def(nb::init<>())
        .def_rw("member", &ClassWithEigenMember::member);

    m.def("castToMapVectorXi", [](nb::object obj) -> Eigen::Map<Eigen::VectorXi>
      {
        return nb::cast<Eigen::Map<Eigen::VectorXi>>(obj);
      });
    m.def("castToRefVectorXi", [](nb::object obj) -> Eigen::VectorXi
      {
        return nb::cast<Eigen::Ref<Eigen::VectorXi>>(obj);
      });
}
