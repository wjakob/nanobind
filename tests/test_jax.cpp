#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

namespace nb = nanobind;

int destruct_count = 0;

NB_MODULE(test_jax_ext, m) {
    m.def("destruct_count", []() { return destruct_count; });
    m.def("reference_jax", [](nb::ndarray<nb::numpy, const float, nb::ndim<1>,
                                         nb::c_contig, nb::device::cpu> a) {
        return nb::ndarray<nb::jax, const float>(a.data(), { a.shape(0) }, a.cast());
    }, nb::rv_policy::reference);

    m.def("ret_jax", []() {
        struct alignas(64) Buf {
            float f[8];
        };
        Buf *buf = new Buf({ 1, 2, 3, 4, 5, 6, 7, 8 });
        size_t shape[2] = { 2, 4 };

        nb::capsule deleter(buf, [](void *p) noexcept {
           destruct_count++;
           delete (Buf *) p;
        });

        return nb::ndarray<nb::jax, float, nb::shape<2, 4>>(buf->f, 2, shape,
                                                            deleter);
    });
}
