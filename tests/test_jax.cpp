#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

namespace nb = nanobind;

int destruct_count = 0;

NB_MODULE(test_jax_ext, m) {
    m.def("destruct_count", []() { return destruct_count; });
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
}
