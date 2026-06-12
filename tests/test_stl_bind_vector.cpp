#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/shared_ptr.h>

namespace nb = nanobind;

// Counts how many instances are alive, to detect leaks of partially
// constructed vectors. The copy constructor can be made to throw after a
// given number of copies to exercise error paths.
struct Cnt {
    static inline int alive = 0;
    static inline int throw_after = -1;
    Cnt() { alive++; }
    Cnt(const Cnt &) {
        if (throw_after == 0)
            throw std::runtime_error("Cnt copy");
        if (throw_after > 0)
            throw_after--;
        alive++;
    }
    Cnt(Cnt &&) { alive++; }
    Cnt &operator=(const Cnt &) = default;
    Cnt &operator=(Cnt &&) = default;
    ~Cnt() { alive--; }
};

NB_MODULE(test_stl_bind_vector_ext, m) {
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

    // test_vector_leak: count live Cnt instances to detect that a partially
    // constructed container is cleaned up when an operation (e.g. a slice
    // __getitem__ whose element copy throws, or an iterable constructor that
    // throws) fails partway through.
    nb::class_<Cnt>(m, "Cnt").def(nb::init<>());
    nb::bind_vector<std::vector<Cnt>>(m, "VectorCnt");
    m.def("cnt_alive", [] { return Cnt::alive; });
    m.def("cnt_throw_after", [](int n) { Cnt::throw_after = n; });

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

    // By default, the bindings produce a __getitem__ that makes a copy, which
    // won't take this non-copyable type: (uncomment to verify build error)
    //nb::bind_vector<std::vector<E_nc>>(m, "VectorENC");

    // But we can request reference semantics instead (extreme care required,
    // read the documentation):
    nb::bind_vector<std::vector<E_nc>,
                    nb::rv_policy::reference_internal>(m, "VectorENC");
    m.def("get_vnc", [](int n) {
        std::vector<E_nc> result;
        for (int i = 1; i <= n; i++)
            result.emplace_back(i);
        return result;
    });
}
