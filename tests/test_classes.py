import test_classes_ext as t
import pytest
import gc

def test01_signature():
    assert t.Struct.__init__.__doc__ == (
        "__init__(*args, **kwargs) -> Any\n"
        "Overloaded function.\n"
        "\n"
        "1. __init__(self) -> None\n"
        "2. __init__(self, arg0: int) -> None")

    assert t.Struct.value.__doc__ == "value(self) -> int"
    assert t.Struct.create_move.__doc__ == "create_move() -> test_classes_ext.Struct"
    assert t.Struct.set_value.__doc__ == "set_value(self, value: int) -> None"
    assert t.Struct.__doc__ == 'Some documentation'
    assert t.Struct.static_test.__doc__ == (
        "static_test(*args, **kwargs) -> Any\n"
        "Overloaded function.\n"
        "\n"
        "1. static_test(arg0: int) -> int\n"
        "2. static_test(arg0: float) -> int")


def test02_static_overload():
    assert t.Struct.static_test(1) == 1
    assert t.Struct.static_test(1.0) == 2


def test03_instantiate():
    gc.collect()
    t.reset()

    s1 = t.Struct()
    assert s1.value() == 5
    s2 = t.Struct(10)
    assert s2.value() == 10
    del s1
    del s2
    gc.collect()

    assert t.stats() == {
        'default_constructed': 1,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }

def test04_double_init():
    s = t.Struct()
    with pytest.raises(RuntimeError) as excinfo:
        s.__init__()
    assert 'the __init__ method should not be called on an initialized object!' in str(excinfo.value)


def test05_rv_policy():
    gc.collect()
    t.reset()
    s = t.Struct()
    assert s.self() is s
    assert s.none() is None
    del s
    gc.collect()
    assert t.stats() == {
        'default_constructed': 1,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 1
    }

    # ------

    t.reset()
    assert t.Struct.create_take().value() == 10
    gc.collect()
    assert t.stats() == {
        'default_constructed': 0,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 1
    }

    # ------

    t.reset()
    assert t.Struct.create_move().value() == 11
    gc.collect()
    assert t.stats() == {
        'default_constructed': 0,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 1,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }

    # ------

    t.reset()
    assert t.Struct.create_reference().value() == 12
    gc.collect()
    assert t.stats() == {
        'default_constructed': 0,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 0
    }

    # ------

    t.reset()
    assert t.Struct.create_copy().value() == 12
    gc.collect()
    assert t.stats() == {
        'default_constructed': 0,
        'value_constructed': 0,
        'copy_constructed': 1,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 1
    }

def test06_reference_internal():
    gc.collect()
    t.reset()
    s = t.PairStruct()
    s1 = s.s1
    s2 = s.s2
    del s
    assert t.stats() == {
        'default_constructed': 2,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 0
    }
    assert s2.value() == 5
    del s2
    gc.collect()

    assert t.stats() == {
        'default_constructed': 2,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 0
    }

    assert s1.value() == 5
    del s1
    gc.collect()

    assert t.stats() == {
        'default_constructed': 2,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }


def test07_big():
    x = [t.Big() for i in range(1024)]
    x = [t.BigAligned() for i in range(1024)]


def test08_inheritance():
    dog = t.Dog('woof')
    cat = t.Cat('meow')
    assert isinstance(dog, t.Animal) and isinstance(dog, t.Dog)
    assert isinstance(cat, t.Animal) and isinstance(cat, t.Cat)
    assert t.go(dog) == 'Dog says woof'
    assert t.go(cat) == 'Cat says meow'


def test09_method_vectorcall():
    out = []

    def f(a, b, c, d, e):
        out.append((a, b, c, d, e))

    class my_class:
        def f(self, a, b, c, d, e):
            self.out = ((a, b, c, d, e))

    t.call_function(f)

    i = my_class()
    t.call_method(i)
    assert out == [(1, 2, "hello", True, 4)]
    assert i.out == (1, 2, "hello", True, 4)


def test10_trampoline():
    gc.collect()
    t.reset()
    for i in range(10):
        class Dachshund(t.Animal):
            def __init__(self):
                super().__init__()
            def name(self):
                return "Dachshund"
            def what(self):
                return "yap"

        d = Dachshund()
        for i in range(10):
            assert t.go(d) == 'Dachshund says yap'

    a = 0
    class GenericAnimal(t.Animal):
        def what(self):
            return "goo"

        def void_ret(self):
            nonlocal a
            a += 1

    ga = GenericAnimal()
    assert t.go(ga) == 'Animal says goo'
    assert t.void_ret(ga) is None
    assert a == 1

    del ga
    del d
    gc.collect()

    assert t.stats() == {
        'default_constructed': 11,
        'value_constructed': 0,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 11
    }


def test11_trampoline_failures():
    class Incomplete(t.Animal):
        def __init__(self):
            super().__init__()

        def void_ret(self):
            raise TypeError("propagating an exception")

    d = Incomplete()
    with pytest.raises(RuntimeError) as excinfo:
        t.go(d)
    assert ('nanobind::detail::get_trampoline(\'Incomplete::what()\'): tried '
            'to call a pure virtual function!' in str(excinfo.value))

    with pytest.raises(TypeError) as excinfo:
        t.void_ret(d)
    assert 'propagating an exception' in str(excinfo.value)

    class Incomplete2(t.Animal):
        def __init__(self):
            pass # Missing call to super().__init__()
        def name(self):
            return "a"
        def what(self):
            return "b"

    with pytest.warns(RuntimeWarning, match='nanobind: attempted to access an uninitialized instance of type \'Incomplete2\'!'):
        with pytest.raises(TypeError) as excinfo:
            t.go(Incomplete2())
    assert 'incompatible function arguments' in str(excinfo.value)


def test12_large_pointers():
    for i in range(1, 10):
        c = t.i2p(i)
        assert isinstance(c, t.Cat)
        assert t.p2i(c) == i

    large = 0xffffffffffffffff
    for i in range(large - 10, large):
        c = t.i2p(i)
        assert isinstance(c, t.Cat)
        assert t.p2i(c) == i


def test13_implicitly_convertible():
    assert t.get_d.__doc__ == "get_d(arg0: test_classes_ext.D) -> int"
    a = t.A(1)
    b = t.B(2)
    b2 = t.B2(3)
    c = t.C(4)
    d = 5
    with pytest.raises(TypeError) as excinfo:
        t.get_d(c)
    assert str(excinfo.value) == (
        "get_d(): incompatible function arguments. The following argument types are supported:\n"
        "    1. get_d(arg0: test_classes_ext.D) -> int\n"
        "\n"
        "Invoked with types: C")
    assert t.get_d(a) == 11
    assert t.get_d(b) == 102
    assert t.get_d(b2) == 103
    assert t.get_d(d) == 10005


def test14_operators():
    a = t.Int(1)
    b = t.Int(2)
    assert repr(a + b) == "3"
    a += b
    assert repr(a) == "3"
    assert repr(b) == "2"

    assert a.__add__("test") is NotImplemented
