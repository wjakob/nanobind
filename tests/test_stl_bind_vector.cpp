#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/shared_ptr.h>

namespace nb = nanobind;

NB_MODULE(test_bind_vector_ext, m) {
    nb::bind_vector<std::vector<unsigned int>>(m, "VectorInt");
    nb::bind_vector<std::vector<bool>>(m, "VectorBool");

    // Ensure that a repeated binding call is ignored
    nb::bind_vector<std::vector<bool>>(m, "VectorBool");

    struct El {
        explicit El(int v) : a(v) {}
        int a;
    };

    // test_vector_custom
    nb::class_<El>(m, "El").def(nb::init<int>())
        .def_rw("a", &El::a);
    nb::bind_vector<std::vector<El>>(m, "VectorEl");
    nb::bind_vector<std::vector<std::vector<El>>>(m, "VectorVectorEl");

    // test_vector_shared_ptr
    nb::bind_vector<std::vector<std::shared_ptr<El>>>(m, "VectorElShared");
}
