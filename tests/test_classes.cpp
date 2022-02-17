#include <nanobind/nanobind.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <memory>

namespace nb = nanobind;
using namespace nb::literals;

static int default_constructed = 0, value_constructed = 0, copy_constructed = 0,
           move_constructed = 0, copy_assigned = 0, move_assigned = 0,
           destructed = 0;

struct Struct;
std::unique_ptr<Struct> struct_tmp;

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
    void set_value(int value) { i = value; }

    static Struct* create_take() { return new Struct(10); }
    static Struct  create_move() { return Struct(11); }
    static Struct* create_copy() { return struct_tmp.get(); }
    static Struct* create_reference() { return struct_tmp.get(); }
    Struct &self() { return *this; }
};

struct PairStruct {
    Struct s1;
    Struct s2;
};

struct Big {
    char data[1024];
    Big() { memset(data, 0xff, 1024); }
};
struct alignas(1024) BigAligned {
    char data[1024];
    BigAligned() {
        if (((uintptr_t) data) % 1024)
            throw std::runtime_error("data is not aligned!");
        memset(data, 0xff, 1024);
    }
};

NB_MODULE(test_classes_ext, m) {
    struct_tmp = std::unique_ptr<Struct>(new Struct(12));

    nb::class_<Struct>(m, "Struct")
        .def(nb::init<>())
        .def(nb::init<int>())
        .def("value", &Struct::value)
        .def("set_value", &Struct::set_value, "value"_a)
        .def("self", &Struct::self)
        .def("none", [](Struct &) -> const Struct * { return nullptr; })
        .def_static("create_move", &Struct::create_move)
        .def_static("create_reference", &Struct::create_reference,
                    nb::rv_policy::reference)
        .def_static("create_copy", &Struct::create_copy,
                    nb::rv_policy::copy)
        .def_static("create_take", &Struct::create_take);

    nb::class_<PairStruct>(m, "PairStruct")
        .def(nb::init<>())
        .def_readwrite("s1", &PairStruct::s1)
        .def_readwrite("s2", &PairStruct::s2);

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

    nb::class_<BigAligned>(m, "BigAligned")
        .def(nb::init<>());

    struct Animal {
        virtual ~Animal() = default;
        virtual std::string name() const { return "Animal"; }
        virtual std::string what() const = 0;
        virtual void void_ret() { }
    };

    struct PyAnimal : Animal {
        NB_TRAMPOLINE(Animal, 3);

        PyAnimal() {
            default_constructed++;
        }

        ~PyAnimal() {
            destructed++;
        }

        std::string name() const override {
            NB_OVERRIDE(std::string, Animal, name);
        }

        std::string what() const override {
            NB_OVERRIDE_PURE(std::string, Animal, what);
        }

        void void_ret() override {
            NB_OVERRIDE(void, Animal, void_ret);
        }
    };

    struct Dog : Animal {
        Dog(const std::string &s) : s(s) { }
        std::string name() const override { return "Dog"; }
        std::string what() const override { return s; }
        std::string s;
    };

    struct Cat : Animal {
        Cat(const std::string &s) : s(s) { }
        std::string name() const override { return "Cat"; }
        std::string what() const override { return s; }
        std::string s;
    };

    auto animal = nb::class_<Animal, PyAnimal>(m, "Animal")
        .def(nb::init<>())
        .def("what", &Animal::what)
        .def("name", &Animal::name);

    nb::class_<Dog, Animal>(m, "Dog")
        .def(nb::init<const std::string &>());

    nb::class_<Cat>(m, "Cat", animal)
        .def(nb::init<const std::string &>());

    m.def("go", [](Animal *a) {
        return a->name() + " says " + a->what();
    });

    m.def("void_ret", [](Animal *a) { a->void_ret(); });

    m.def("call_function", [](nb::handle h) {
        return h(1, 2, "hello", true, 4);
    });

    m.def("call_method", [](nb::handle h) {
        return h.attr("f")(1, 2, "hello", true, 4);
    });
}
