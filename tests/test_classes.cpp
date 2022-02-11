#include <nanobind/nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

static int default_constructed = 0, value_constructed = 0, copy_constructed = 0,
           move_constructed = 0, copy_assigned = 0, move_assigned = 0,
           destructed = 0;

struct Struct {
    int i = 5;

    Struct() { default_constructed++; }
    Struct(int i) : i(i) { value_constructed++; }
    Struct(const Struct &s) : i(s.i) { copy_constructed++; }
    Struct(Struct &&s) : i(s.i) { s.i = 0; move_constructed++; }
    Struct &operator=(const Struct &s) { i = s.i; copy_assigned++; return *this; }
    Struct &operator=(Struct &&s) { std::swap(i, s.i); move_assigned++; return *this; }
    ~Struct() { destructed++; }

    int value() const { return i; }
};

struct alignas(1024) Big { char data[1024]; };

NB_MODULE(test_classes_ext, m) {
    nb::class_<Struct>(m, "Struct")
        .def(nb::init<>())
        .def(nb::init<int>())
        .def("value", &Struct::value);

    m.def("stats", []{
        nb::dict d;
        d["default_constructed"] = default_constructed;
        d["value_constructed"] = value_constructed;
        d["copy_constructed"] = copy_constructed;
        d["move_constructed"] = move_constructed;
        d["copy_assigned"] = copy_assigned;
        d["move_assigned"] = move_assigned;
        d["destructed"] = destructed;
        return d;
    });

    m.def("reset", []() {
        default_constructed = 0;
        value_constructed = 0;
        copy_constructed = 0;
        move_constructed = 0;
        copy_assigned = 0;
        move_assigned = 0;
        destructed = 0;
    });

    nb::class_<Big>(m, "Big")
        .def(nb::init<>());
}
