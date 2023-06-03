#include <vector>

#include <nanobind/stl/bind_vector.h>

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

    struct E_nc {
        explicit E_nc(int i) : value{i} {}
        E_nc(const E_nc &) = delete;
        E_nc &operator=(const E_nc &) = delete;
        E_nc(E_nc &&) = default;
        E_nc &operator=(E_nc &&) = default;

        int value;
    };

    // test_noncopyable_containers
    nb::class_<E_nc>(m, "ENC")
        .def(nb::init<int>())
        .def_rw("value", &E_nc::value);

    nb::bind_vector<std::vector<E_nc>>(m, "VectorENC");
    m.def("get_vnc", [](int n) {
        std::vector<E_nc> result;
        for (int i = 1; i <= n; i++)
            result.emplace_back(i);
        return result;
    });
}
