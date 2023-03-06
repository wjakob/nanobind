#if defined(__GNUC__)
// warning: '..' declared with greater visibility than the type of its field '..'
#  pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;

static int created = 0;
static int deleted = 0;

struct Example {
    int value;
    Example(int value) : value(value) { created++; }
    ~Example() { deleted++; }

    static Example *make(int value) { return new Example(value); }
    static std::shared_ptr<Example> make_shared(int value) {
        return std::make_shared<Example>(value);
    }
};

struct SharedWrapper { std::shared_ptr<Example> value; };
struct UniqueWrapper { std::unique_ptr<Example> value; };
struct UniqueWrapper2 { std::unique_ptr<Example, nb::deleter<Example>> value; };

enum class PetKind { Cat, Dog };
struct Pet { const PetKind kind; };
struct Dog : Pet { Dog() : Pet{PetKind::Dog} { } };
struct Cat : Pet { Cat() : Pet{PetKind::Cat} { } };

namespace nanobind::detail {
    template <> struct type_hook<Pet> {
        static const std::type_info *get(Pet *p) {
            if (p) {
                switch (p->kind) {
                    case PetKind::Dog: return &typeid(Dog);
                    case PetKind::Cat: return &typeid(Cat);
                }
            }
            return &typeid(Pet);
        }
    };
} // namespace nanobind::detail

NB_MODULE(test_holders_ext, m) {
    nb::class_<Example>(m, "Example")
        .def(nb::init<int>())
        .def_rw("value", &Example::value)
        .def_static("make", &Example::make)
        .def_static("make_shared", &Example::make_shared);

    // ------- shared_ptr -------

    nb::class_<SharedWrapper>(m, "SharedWrapper")
        .def(nb::init<std::shared_ptr<Example>>())
        .def_rw("ptr", &SharedWrapper::value)
        .def_prop_rw("value",
            [](SharedWrapper &t) { return t.value->value; },
            [](SharedWrapper &t, int value) { t.value->value = value; });

    m.def("query_shared_1", [](Example *shared) { return shared->value; });
    m.def("query_shared_2",
          [](std::shared_ptr<Example> &shared) { return shared->value; });
    m.def("passthrough",
          [](std::shared_ptr<Example> shared) { return shared; });

    // ------- unique_ptr -------

    m.def("unique_from_cpp",
          []() { return std::make_unique<Example>(1); });
    m.def("unique_from_cpp_2", []() {
        return std::unique_ptr<Example, nb::deleter<Example>>(new Example(2));
    });

    nb::class_<UniqueWrapper>(m, "UniqueWrapper")
        .def(nb::init<std::unique_ptr<Example>>())
        .def("get", [](UniqueWrapper *uw) { return std::move(uw->value); });

    nb::class_<UniqueWrapper2>(m, "UniqueWrapper2")
        .def(nb::init<std::unique_ptr<Example, nb::deleter<Example>>>())
        .def("get", [](UniqueWrapper2 *uw) { return std::move(uw->value); });

    m.def("passthrough_unique",
          [](std::unique_ptr<Example> unique) { return unique; });
    m.def("passthrough_unique_2",
          [](std::unique_ptr<Example, nb::deleter<Example>> unique) { return unique; });

    m.def("stats", []{ return std::make_pair(created, deleted); });
    m.def("reset", []{ created = deleted = 0; });

    struct Base { ~Base() = default; };
    struct PolymorphicBase { virtual ~PolymorphicBase() = default; };
    struct Subclass : Base { };
    struct PolymorphicSubclass : PolymorphicBase { };
    struct AnotherSubclass : Base { };
    struct AnotherPolymorphicSubclass : PolymorphicBase { };

    nb::class_<Base> (m, "Base");
    nb::class_<Subclass> (m, "Subclass");
    nb::class_<PolymorphicBase> (m, "PolymorphicBase");
    nb::class_<PolymorphicSubclass> (m, "PolymorphicSubclass");

    m.def("u_polymorphic_factory", []() { return std::unique_ptr<PolymorphicBase>(new PolymorphicSubclass()); });
    m.def("u_polymorphic_factory_2", []() { return std::unique_ptr<PolymorphicBase>(new AnotherPolymorphicSubclass()); });
    m.def("u_factory", []() { return std::unique_ptr<Base>(new Subclass()); });
    m.def("u_factory_2", []() { return std::unique_ptr<Base>(new AnotherSubclass()); });

    m.def("s_polymorphic_factory", []() { return std::shared_ptr<PolymorphicBase>(new PolymorphicSubclass()); });
    m.def("s_polymorphic_factory_2", []() { return std::shared_ptr<PolymorphicBase>(new AnotherPolymorphicSubclass()); });
    m.def("s_factory", []() { return std::shared_ptr<Base>(new Subclass()); });
    m.def("s_factory_2", []() { return std::shared_ptr<Base>(new AnotherSubclass()); });

    nb::class_<Pet>(m, "Pet");
    nb::class_<Dog>(m, "Dog");
    nb::class_<Cat>(m, "Cat");

    nb::enum_<PetKind>(m, "PetKind")
        .value("Cat", PetKind::Cat)
        .value("Dog", PetKind::Dog);

    m.def("make_pet", [](PetKind kind) -> Pet* {
        switch (kind) {
            case PetKind::Dog:
                return new Dog();
            case PetKind::Cat:
                return new Cat();
            default:
                throw std::runtime_error("Internal error");
        }
    });

    m.def("make_pet_u", [](PetKind kind) -> std::unique_ptr<Pet> {
        switch (kind) {
            case PetKind::Dog:
                return std::make_unique<Dog>();
            case PetKind::Cat:
                return std::make_unique<Cat>();
            default:
                throw std::runtime_error("Internal error");
        }
    });

    m.def("make_pet_s", [](PetKind kind) -> std::shared_ptr<Pet> {
        switch (kind) {
            case PetKind::Dog:
                return std::make_shared<Dog>();
            case PetKind::Cat:
                return std::make_shared<Cat>();
            default:
                throw std::runtime_error("Internal error");
        }
    });
}
