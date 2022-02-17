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


def test02_instantiate():
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


def test03_double_init():
    s = t.Struct()
    with pytest.raises(RuntimeError) as excinfo:
        s.__init__()
    assert 'the __init__ method should not be called on an initialized object!' in str(excinfo.value)


def test04_rv_policy():
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

def test05_reference_internal():
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


def test06_big():
    x = [t.Big() for i in range(1024)]
    x = [t.BigAligned() for i in range(1024)]


def test07_inheritance():
    dog = t.Dog('woof')
    cat = t.Cat('meow')
    assert isinstance(dog, t.Animal) and isinstance(dog, t.Dog)
    assert isinstance(cat, t.Animal) and isinstance(cat, t.Cat)
    assert t.go(dog) == 'Dog says woof'
    assert t.go(cat) == 'Cat says meow'


def test08_method_vectorcall():
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


def test09_trampoline():
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


def test10_trampoline_failures():
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

    with pytest.raises(TypeError) as excinfo:
        t.go(Incomplete2())
    assert 'incompatible function arguments' in str(excinfo.value)
