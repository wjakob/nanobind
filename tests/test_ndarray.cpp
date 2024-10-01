#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <algorithm>
#include <complex>
#include <vector>

namespace nb = nanobind;

using namespace nb::literals;

int destruct_count = 0;
static float f_global[] { 1, 2, 3, 4, 5, 6, 7, 8 };
static int i_global[] { 1, 2, 3, 4, 5, 6, 7, 8 };

#if defined(__aarch64__)
namespace nanobind::detail {
    template <> struct dtype_traits<__fp16> {
        static constexpr dlpack::dtype value {
            (uint8_t) dlpack::dtype_code::Float, // type code
            16, // size in bits
            1   // lanes (simd)
        };
        static constexpr auto name = const_name("float16");
    };
}
#endif

template<bool expect_ro, bool is_shaped, typename... Ts>
bool check_ro(const nb::ndarray<Ts...>& a) {  // Pytest passes five doubles
    static_assert(std::remove_reference_t<decltype(a)>::ReadOnly == expect_ro);
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(a.data())>>
                  == expect_ro);
    auto vd = a.template view<double, nb::ndim<1>>();
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(vd.data())>>
                  == expect_ro);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(vd(0))>>
                  == expect_ro);
    auto vcd = a.template view<const double, nb::ndim<1>>();
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(vcd.data())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(vcd(0))>>);

    bool pass = vd.data() == a.data() && vcd.data() == a.data();
    if constexpr (!expect_ro) {
        vd(1) = 1.414214;
        pass &= vcd(1) == 1.414214;
    }
    if constexpr (is_shaped) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(a(0))>>
                      == expect_ro);
        auto v = a.view();
        static_assert(std::is_const_v<std::remove_pointer_t<decltype(v.data())>>
                      == expect_ro);
        static_assert(std::is_const_v<std::remove_reference_t<decltype(v(0))>>
                      == expect_ro);
        pass &= v.data() == a.data();
        if constexpr (!expect_ro) {
              a(2) = 2.718282;
              v(4) = 16.0;
        }
    }
    pass &= vcd(3) == 3.14159;
    return pass;
}

NB_MODULE(test_ndarray_ext, m) {
    m.def("get_is_valid", [](const nb::ndarray<nb::ro> &t) {
        return t.is_valid();
    }, "array"_a.noconvert().none());

    m.def("get_shape", [](const nb::ndarray<nb::ro> &t) {
        nb::list l;
        for (size_t i = 0; i < t.ndim(); ++i)
            l.append(t.shape(i));
        return l;
    }, "array"_a.noconvert());

    m.def("get_size", [](const nb::ndarray<> &t) {
        return t.size();
    }, "array"_a.noconvert().none());

    m.def("get_itemsize", [](const nb::ndarray<> &t) {
        return t.itemsize();
    }, "array"_a.noconvert().none());

    m.def("get_nbytes", [](const nb::ndarray<> &t) {
        return t.nbytes();
    }, "array"_a.noconvert().none());

    m.def("get_stride", [](const nb::ndarray<> &t, size_t i) {
        return t.stride(i);
    }, "array"_a.noconvert(), "i"_a);

    m.def("check_shape_ptr", [](const nb::ndarray<> &t) {
        std::vector<int64_t> shape(t.ndim());
        std::copy(t.shape_ptr(), t.shape_ptr() + t.ndim(), shape.begin());
        for (size_t i = 0; i < t.ndim(); ++i)
            if (shape[i] != (int64_t) t.shape(i))
                return false;
        return true;
    });

    m.def("check_stride_ptr", [](const nb::ndarray<> &t) {
        std::vector<int64_t> stride(t.ndim());
        std::copy(t.stride_ptr(), t.stride_ptr() + t.ndim(), stride.begin());
        for (size_t i = 0; i < t.ndim(); ++i)
            if (stride[i] != (int64_t) t.stride(i))
                return false;
        return true;
    });

    m.def("check_float", [](const nb::ndarray<> &t) {
        return t.dtype() == nb::dtype<float>();
    });
    m.def("check_bool", [](const nb::ndarray<> &t) {
        return t.dtype() == nb::dtype<bool>();
    });

    m.def("pass_float32", [](const nb::ndarray<float> &) { }, "array"_a.noconvert());
    m.def("pass_float32_const", [](const nb::ndarray<const float> &) { }, "array"_a.noconvert());
    m.def("pass_complex64", [](const nb::ndarray<std::complex<float>> &) { }, "array"_a.noconvert());
    m.def("pass_complex64_const", [](nb::ndarray<const std::complex<float>>) { }, "array"_a.noconvert());
    m.def("pass_uint32", [](const nb::ndarray<uint32_t> &) { }, "array"_a.noconvert());
    m.def("pass_bool", [](const nb::ndarray<bool> &) { }, "array"_a.noconvert());
    m.def("pass_float32_shaped",
          [](const nb::ndarray<float, nb::shape<3, -1, 4>> &) {}, "array"_a.noconvert());

    m.def("pass_float32_shaped_ordered",
          [](const nb::ndarray<float, nb::c_contig,
                               nb::shape<-1, -1, 4>> &) {}, "array"_a.noconvert());

    m.def("check_rw_by_value",
          [](nb::ndarray<> a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/false>(a);
          });
    m.def("check_ro_by_value_ro",
          [](nb::ndarray<nb::ro> a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/false>(a);
          });
    m.def("check_rw_by_value_float64",
          [](nb::ndarray<double, nb::ndim<1>> a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/true>(a);
          });
    m.def("check_ro_by_value_const_float64",
          [](nb::ndarray<const double, nb::ndim<1>> a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/true>(a);
          });

    m.def("check_rw_by_const_ref",
          [](const nb::ndarray<>& a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/false>(a);
          });
    m.def("check_ro_by_const_ref_ro",
          [](const nb::ndarray<nb::ro>& a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/false>(a);
          });
    m.def("check_rw_by_const_ref_float64",
          [](nb::ndarray<double, nb::ndim<1>> a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/true>(a);
          });
    m.def("check_ro_by_const_ref_const_float64",
          [](const nb::ndarray<const double, nb::ndim<1>>& a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/true>(a);
          });

    m.def("check_rw_by_rvalue_ref",
          [](nb::ndarray<>&& a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/false>(a);
          });
    m.def("check_ro_by_rvalue_ref_ro",
          [](nb::ndarray<nb::ro>&& a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/false>(a);
          });
    m.def("check_rw_by_rvalue_ref_float64",
          [](nb::ndarray<double, nb::ndim<1>>&& a) {
              return check_ro</*expect_ro=*/false, /*is_shaped=*/true>(a);
          });
    m.def("check_ro_by_rvalue_ref_const_float64",
          [](nb::ndarray<const double, nb::ndim<1>>&& a) {
              return check_ro</*expect_ro=*/true, /*is_shaped=*/true>(a);
          });

    m.def("check_order", [](nb::ndarray<nb::c_contig>) -> char { return 'C'; });
    m.def("check_order", [](nb::ndarray<nb::f_contig>) -> char { return 'F'; });
    m.def("check_order", [](nb::ndarray<>) -> char { return '?'; });

    m.def("make_contig", [](nb::ndarray<nb::c_contig> a) { return a; });

    m.def("check_device", [](nb::ndarray<nb::device::cpu>) -> const char * { return "cpu"; });
    m.def("check_device", [](nb::ndarray<nb::device::cuda>) -> const char * { return "cuda"; });

    m.def("initialize",
          [](nb::ndarray<float, nb::shape<10>, nb::device::cpu> &t) {
              for (size_t i = 0; i < 10; ++i)
                t(i) = (float) i;
          });

    m.def("initialize",
          [](nb::ndarray<float, nb::shape<10, -1>, nb::device::cpu> &t) {
              int k = 0;
              for (size_t i = 0; i < 10; ++i)
                  for (size_t j = 0; j < t.shape(1); ++j)
                      t(i, j) = (float) k++;
          });

    m.def(
        "noimplicit",
        [](nb::ndarray<float, nb::c_contig, nb::shape<2, 2>>) { return 0; },
        "array"_a.noconvert());

    m.def(
        "implicit",
        [](nb::ndarray<float, nb::c_contig, nb::shape<2, 2>>) { return 0; },
        "array"_a);

    m.def("inspect_ndarray", [](const nb::ndarray<>& ndarray) {
        printf("Tensor data pointer : %p\n", ndarray.data());
        printf("Tensor dimension : %zu\n", ndarray.ndim());
        for (size_t i = 0; i < ndarray.ndim(); ++i) {
            printf("Tensor dimension [%zu] : %zu\n", i, ndarray.shape(i));
            printf("Tensor stride    [%zu] : %zu\n", i, (size_t) ndarray.stride(i));
        }
        printf("Tensor is on CPU? %i\n", ndarray.device_type() == nb::device::cpu::value);
        printf("Device ID = %u\n", ndarray.device_id());
        printf("Tensor dtype check: int16=%i, uint32=%i, float32=%i complex64=%i\n",
            ndarray.dtype() == nb::dtype<int16_t>(),
            ndarray.dtype() == nb::dtype<uint32_t>(),
            ndarray.dtype() == nb::dtype<float>(),
            ndarray.dtype() == nb::dtype<std::complex<float>>()
        );
    });

    m.def("process", [](nb::ndarray<uint8_t, nb::shape<-1, -1, 3>,
                                   nb::c_contig, nb::device::cpu> ndarray) {
        // Double brightness of the MxNx3 RGB image
        for (size_t y = 0; y < ndarray.shape(0); ++y)
            for (size_t x = 0; x < ndarray.shape(1); ++x)
                for (size_t ch = 0; ch < 3; ++ch)
                    ndarray(y, x, ch) = (uint8_t) std::min(255, ndarray(y, x, ch) * 2);

    });

    m.def("destruct_count", []() { return destruct_count; });
    m.def("return_dlpack", []() {
        float *f = new float[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(f, [](void *data) noexcept {
            destruct_count++;
            delete[] (float *) data;
        });

        return nb::ndarray<float, nb::shape<2, 4>>(f, 2, shape, deleter);
    });

    m.def("passthrough", [](nb::ndarray<> a) { return a; }, nb::rv_policy::none);
    m.def("passthrough_copy", [](nb::ndarray<> a) { return a; }, nb::rv_policy::copy);

    m.def("passthrough_arg_none", [](nb::ndarray<> a) { return a; },
          nb::arg().none(), nb::rv_policy::none);

    m.def("ret_numpy", []() {
        float *f = new float[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(f, [](void *data) noexcept {
            destruct_count++;
            delete[] (float *) data;
        });

        return nb::ndarray<nb::numpy, float, nb::shape<2, 4>>(f, 2, shape,
                                                              deleter);
    });

    m.def("ret_numpy_const_ref", []() {
        size_t shape[2] = { 2, 4 };
        return nb::ndarray<nb::numpy, const float, nb::shape<2, 4>, nb::c_contig>(f_global, 2, shape, nb::handle());
    }, nb::rv_policy::reference);

    m.def("ret_numpy_const_ref_f", []() {
        size_t shape[2] = { 2, 4 };
        return nb::ndarray<nb::numpy, const float, nb::shape<2, 4>, nb::f_contig>(f_global, 2, shape, nb::handle());
    }, nb::rv_policy::reference);


    m.def("ret_numpy_const", []() {
        return nb::ndarray<nb::numpy, const float, nb::shape<2, 4>>(f_global, { 2, 4 }, nb::handle());
    });

    m.def("ret_pytorch", []() {
        float *f = new float[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(f, [](void *data) noexcept {
           destruct_count++;
           delete[] (float *) data;
        });

        return nb::ndarray<nb::pytorch, float, nb::shape<2, 4>>(f, 2, shape,
                                                                deleter);
    });

    m.def("ret_jax", []() {
        float *f = new float[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(f, [](void *data) noexcept {
           destruct_count++;
           delete[] (float *) data;
        });

        return nb::ndarray<nb::jax, float, nb::shape<2, 4>>(f, 2, shape,
                                                            deleter);
    });

    m.def("ret_tensorflow", []() {
        struct alignas(256) Buf {
            float f[8];
        };
        Buf *buf = new Buf({ 1, 2, 3, 4, 5, 6, 7, 8 });
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(buf, [](void *data) noexcept {
           destruct_count++;
           delete (Buf *) data;
        });

        return nb::ndarray<nb::tensorflow, float, nb::shape<2, 4>>(buf->f, 2, shape,
                                                                   deleter);
    });

    m.def("ret_array_scalar", []() {
            float* f = new float[1] { 1 };
            size_t shape[1] = {};

            nb::capsule deleter(f, [](void* data) noexcept {
                destruct_count++;
                delete[] (float *) data;
            });

            return nb::ndarray<nb::numpy, float>(f, 0, shape, deleter);
    });

    m.def("noop_3d_c_contig",
          [](nb::ndarray<float, nb::ndim<3>, nb::c_contig>) { return; });

    m.def("noop_2d_f_contig",
          [](nb::ndarray<float, nb::ndim<2>, nb::f_contig>) { return; });

    m.def("accept_rw", [](nb::ndarray<float, nb::shape<2>> a) { return a(0); });
    m.def("accept_ro", [](nb::ndarray<const float, nb::shape<2>> a) { return a(0); });

    m.def("check", [](nb::handle h) { return nb::ndarray_check(h); });


    struct Cls {
        auto f1() { return nb::ndarray<nb::numpy, float>(data, { 10 }, nb::handle()); }
        auto f2() { return nb::ndarray<nb::numpy, float>(data, { 10 }, nb::cast(this, nb::rv_policy::none)); }
        auto f3(nb::handle owner) { return nb::ndarray<nb::numpy, float>(data, { 10 }, owner); }

        ~Cls() {
           destruct_count++;
        }

        float data [10] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    };

    nb::class_<Cls>(m, "Cls")
        .def(nb::init<>())
        .def("f1", &Cls::f1)
        .def("f2", &Cls::f2)
        .def("f1_ri", &Cls::f1, nb::rv_policy::reference_internal)
        .def("f2_ri", &Cls::f2, nb::rv_policy::reference_internal)
        .def("f3_ri", &Cls::f3, nb::rv_policy::reference_internal);

    m.def("fill_view_1", [](nb::ndarray<> x) {
        if (x.ndim() == 2 && x.dtype() == nb::dtype<float>()) {
            auto v = x.view<float, nb::ndim<2>>();
            for (size_t i = 0; i < v.shape(0); i++)
                for (size_t j = 0; j < v.shape(1); j++)
                    v(i, j) *= 2;
        }
    }, "x"_a.noconvert());

    m.def("fill_view_2", [](nb::ndarray<float, nb::ndim<2>, nb::device::cpu> x) {
        auto v = x.view();
        for (size_t i = 0; i < v.shape(0); ++i)
            for (size_t j = 0; j < v.shape(1); ++j)
                v(i, j) = (float) (i * 10 + j);
    }, "x"_a.noconvert());

    m.def("fill_view_3", [](nb::ndarray<float, nb::shape<3, 4>, nb::c_contig, nb::device::cpu> x) {
        auto v = x.view();
        for (size_t i = 0; i < v.shape(0); ++i)
            for (size_t j = 0; j < v.shape(1); ++j)
                v(i, j) = (float) (i * 10 + j);
    }, "x"_a.noconvert());

    m.def("fill_view_4", [](nb::ndarray<float, nb::shape<3, 4>, nb::f_contig, nb::device::cpu> x) {
        auto v = x.view();
        for (size_t i = 0; i < v.shape(0); ++i)
            for (size_t j = 0; j < v.shape(1); ++j)
                v(i, j) = (float) (i * 10 + j);
    }, "x"_a.noconvert());

    m.def("fill_view_5", [](nb::ndarray<std::complex<float>, nb::shape<2, 2>, nb::c_contig, nb::device::cpu> x) {
        auto v = x.view();
        for (size_t i = 0; i < v.shape(0); ++i)
            for (size_t j = 0; j < v.shape(1); ++j)
                v(i, j) *= std::complex<float>(-1.0f, 2.0f);
    }, "x"_a.noconvert());

    m.def("fill_view_6", [](nb::ndarray<std::complex<float>, nb::shape<2, 2>, nb::c_contig, nb::device::cpu> x) {
        auto v = x.view<nb::shape<4>>();
        for (size_t i = 0; i < v.shape(0); ++i)
            v(i) = -v(i);
    }, "x"_a.noconvert());

#if defined(__aarch64__)
    m.def("ret_numpy_half", []() {
        __fp16 *f = new __fp16[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(f, [](void *data) noexcept {
            destruct_count++;
            delete[] (__fp16*) data;
        });
        return nb::ndarray<nb::numpy, __fp16, nb::shape<2, 4>>(f, 2, shape,
                                                               deleter);
    });
#endif

    m.def("cast", [](bool b) -> nb::ndarray<nb::numpy> {
        using Ret = nb::ndarray<nb::numpy>;
        if (b)
            return Ret(nb::ndarray<nb::numpy, float, nb::shape<>>(f_global, 0, nullptr, nb::handle()));
        else
            return Ret(nb::ndarray<nb::numpy, int, nb::shape<>>(i_global, 0, nullptr, nb::handle()));
    });

    // issue #365
    m.def("set_item",
          [](nb::ndarray<double, nb::ndim<1>, nb::c_contig> data, uint32_t) {
              data(0) = 123;
          });

    m.def("set_item",
          [](nb::ndarray<std::complex<double>, nb::ndim<1>, nb::c_contig> data, uint32_t) {
              data(0) = 123;
          });

    // issue #709
    m.def("test_implicit_conversion",
          [](nb::ndarray<nb::ro, nb::c_contig, nb::device::cpu> arg) {
              return arg;
          },
          nb::arg());

    m.def("ret_infer_c",
          []() { return nb::ndarray<float, nb::shape<2, 4>, nb::numpy, nb::c_contig>(f_global); });
    m.def("ret_infer_f",
          []() { return nb::ndarray<float, nb::shape<2, 4>, nb::numpy, nb::f_contig>(f_global); });

    using Array = nb::ndarray<float, nb::numpy, nb::shape<4, 4>, nb::f_contig>;

    struct Matrix4f {
        float m[4][4];
        Array data() { return Array(m); }
        auto data_ref() { return Array(m).cast(nb::rv_policy::reference_internal, nb::find(this)); }
        auto data_copy() { return Array(m).cast(nb::rv_policy::copy); }
    };

    nb::class_<Matrix4f>(m, "Matrix4f")
        .def(nb::init<>())
        .def("data", &Matrix4f::data, nb::rv_policy::reference_internal)
        .def("data_ref", &Matrix4f::data_ref)
        .def("data_copy", &Matrix4f::data_copy);

    using Vector3f = nb::ndarray<float, nb::numpy, nb::shape<3>>;

    m.def("ret_from_stack_1", []() {
        float f[] { 1, 2, 3 };
        return nb::cast(Vector3f(f));
    });

    m.def("ret_from_stack_2", []() {
        float f[] { 1, 2, 3 };
        return Vector3f(f).cast();
    });
}
