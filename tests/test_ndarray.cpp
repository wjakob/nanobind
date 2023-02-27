#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <algorithm>

namespace nb = nanobind;

using namespace nb::literals;

int destruct_count = 0;

NB_MODULE(test_ndarray_ext, m) {
    m.def("get_shape", [](const nb::ndarray<> &t) {
        nb::list l;
        for (size_t i = 0; i < t.ndim(); ++i)
            l.append(t.shape(i));
        return l;
    }, "array"_a.noconvert());

    m.def("check_float", [](const nb::ndarray<> &t) {
        return t.dtype() == nb::dtype<float>();
    });

    m.def("pass_float32", [](const nb::ndarray<float> &) { }, "array"_a.noconvert());
    m.def("pass_uint32", [](const nb::ndarray<uint32_t> &) { }, "array"_a.noconvert());
    m.def("pass_float32_shaped",
          [](const nb::ndarray<float, nb::shape<3, nb::any, 4>> &) {}, "array"_a.noconvert());

    m.def("pass_float32_shaped_ordered",
          [](const nb::ndarray<float, nb::c_contig,
                               nb::shape<nb::any, nb::any, 4>> &) {}, "array"_a.noconvert());

    m.def("check_order", [](nb::ndarray<nb::c_contig>) -> char { return 'C'; });
    m.def("check_order", [](nb::ndarray<nb::f_contig>) -> char { return 'F'; });
    m.def("check_order", [](nb::ndarray<>) -> char { return '?'; });

    m.def("check_device", [](nb::ndarray<nb::device::cpu>) -> const char * { return "cpu"; });
    m.def("check_device", [](nb::ndarray<nb::device::cuda>) -> const char * { return "cuda"; });

    m.def("initialize",
          [](nb::ndarray<float, nb::shape<10>, nb::device::cpu> &t) {
              for (size_t i = 0; i < 10; ++i)
                t(i) = (float) i;
          });

    m.def("initialize",
          [](nb::ndarray<float, nb::shape<10, nb::any>, nb::device::cpu> &t) {
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

    m.def("inspect_ndarray", [](nb::ndarray<> ndarray) {
        printf("Tensor data pointer : %p\n", ndarray.data());
        printf("Tensor dimension : %zu\n", ndarray.ndim());
        for (size_t i = 0; i < ndarray.ndim(); ++i) {
            printf("Tensor dimension [%zu] : %zu\n", i, ndarray.shape(i));
            printf("Tensor stride    [%zu] : %zu\n", i, (size_t) ndarray.stride(i));
        }
        printf("Tensor is on CPU? %i\n", ndarray.device_type() == nb::device::cpu::value);
        printf("Device ID = %u\n", ndarray.device_id());
        printf("Tensor dtype check: int16=%i, uint32=%i, float32=%i\n",
            ndarray.dtype() == nb::dtype<int16_t>(),
            ndarray.dtype() == nb::dtype<uint32_t>(),
            ndarray.dtype() == nb::dtype<float>()
        );
    });

    m.def("process", [](nb::ndarray<uint8_t, nb::shape<nb::any, nb::any, 3>,
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
    m.def("passthrough", [](nb::ndarray<> a) { return a; });

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

    m.def("ret_array_scalar", []() {
            float* f = new float[1] { 1 };
            size_t shape[1] = {};

            nb::capsule deleter(f, [](void* data) noexcept {
                destruct_count++;
                delete[] (float *) data;
            });

            return nb::ndarray<nb::numpy, float>(f, 0, shape, deleter);
    });
}
