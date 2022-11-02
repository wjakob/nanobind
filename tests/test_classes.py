import test_classes_ext as t
import pytest
import gc

@pytest.fixture
def clean():
    gc.collect()
    t.reset()

def assert_stats(**kwargs):
    gc.collect()
    for k, v in t.stats().items():
        fail = False
        if k in kwargs:
            if v != kwargs[k]:
                fail = True
        elif v != 0:
            fail = True
        if fail:
            raise Exception(f'Mismatch for key {k}: {t.stats()}')

def test01_signature():
    assert t.Struct.__init__.__doc__ == (
        "__init__(self) -> None\n"
        "__init__(self, arg: int, /) -> None"
    )

    assert t.Struct.value.__doc__ == "value(self) -> int"
    assert t.Struct.create_move.__doc__ == "create_move() -> test_classes_ext.Struct"
    assert t.Struct.set_value.__doc__ == "set_value(self, value: int) -> None"
    assert t.Struct.__doc__ == 'Some documentation'
    assert t.Struct.static_test.__doc__ == (
        "static_test(arg: int, /) -> int\n"
        "static_test(arg: float, /) -> int")


def test02_static_overload():
    assert t.Struct.static_test(1) == 1
    assert t.Struct.static_test(1.0) == 2


def test03_instantiate(clean):
    s1 : t.Struct = t.Struct()
    assert s1.value() == 5
    s2 = t.Struct(10)
    assert s2.value() == 10
    del s1
    del s2
    assert_stats(
        default_constructed=1,
        value_constructed=1,
        destructed=2
    )

def test04_double_init():
    s = t.Struct()
    with pytest.raises(RuntimeError) as excinfo:
        s.__init__()
    assert 'the __init__ method should not be called on an initialized object!' in str(excinfo.value)


def test05_rv_policy(clean):
    s = t.Struct()
    assert s.self() is s
    assert s.none() is None
    del s
    assert_stats(
        default_constructed=1,
        destructed=1
    )

    # ------

    t.reset()
    assert t.Struct.create_take().value() == 10
    assert_stats(
        value_constructed=1,
        destructed=1
    )

    # ------

    t.reset()
    assert t.Struct.create_move().value() == 11
    assert_stats(
        value_constructed=1,
        move_constructed=1,
        destructed=2
    )

    # ------

    t.reset()
    assert t.Struct.create_reference().value() == 12
    assert_stats()

    # ------

    t.reset()
    assert t.Struct.create_copy().value() == 12
    assert_stats(
        copy_constructed=1,
        destructed=1
    )

def test06_reference_internal(clean):
    s = t.PairStruct()
    s1 = s.s1
    s2 = s.s2
    del s
    assert_stats(default_constructed=2)
    assert s2.value() == 5
    del s2

    assert_stats(default_constructed=2)

    assert s1.value() == 5
    del s1
    assert_stats(
        default_constructed=2,
        destructed=2
    )


def test07_big():
    x = [t.Big() for i in range(1024)]
    x2 = [t.BigAligned() for i in range(1024)]


def test08_inheritance():
    dog = t.Dog('woof')
    cat = t.Cat('meow')
    assert dog.name() == 'Dog'
    assert cat.name() == 'Cat'
    assert dog.what() == 'woof'
    assert cat.what() == 'meow'
    assert isinstance(dog, t.Animal) and isinstance(dog, t.Dog)
    assert isinstance(cat, t.Animal) and isinstance(cat, t.Cat)
    assert t.go(dog) == 'Dog says woof'
    assert t.go(cat) == 'Cat says meow'


def test09_method_vectorcall():
    out = []

    def f(a, b, c, d, e):
        out.append((a, b, c, d, e))

    class MyClass:
        def f(self, a, b, c, d, e):
            self.out = ((a, b, c, d, e))

    t.call_function(f)

    i = MyClass()
    t.call_method(i)
    assert out == [(1, 2, "hello", True, 4)]
    assert i.out == (1, 2, "hello", True, 4)


def test10_trampoline(clean):
    for _ in range(10):
        class Dachshund(t.Animal):
            def __init__(self):
                super().__init__()
            def name(self):
                return "Dachshund"
            def what(self):
                return "yap"

        d = Dachshund()
        for _ in range(10):
            assert t.go(d) == 'Dachshund says yap'

    a = 0
    class GenericAnimal(t.Animal):
        def what(self):
            return "goo"

        def void_ret(self):
            nonlocal a
            a += 1

        def name(self):
            return 'Generic' + super().name()

    ga = GenericAnimal()
    assert t.go(ga) == 'GenericAnimal says goo'
    assert t.void_ret(ga) is None
    assert a == 1

    del ga
    del d

    assert_stats(
        default_constructed=11,
        destructed=11
    )


def test11_trampoline_failures():
    class Incomplete(t.Animal):
        def __init__(self):
            super().__init__()

        def void_ret(self):
            raise TypeError("propagating an exception")

    d = Incomplete()
    with pytest.raises(RuntimeError) as excinfo:
        t.go(d)
    assert ('test_classes.Incomplete::what()\'): tried to call a pure virtual function!' in str(excinfo.value))

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

    with pytest.warns(RuntimeWarning, match='nanobind: attempted to access an uninitialized instance of type'):
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
    assert t.get_d.__doc__ == "get_d(arg: test_classes_ext.D, /) -> int"
    a = t.A(1)
    b = t.B(2)
    b2 = t.B2(3)
    c = t.C(4)
    d = 5
    with pytest.raises(TypeError) as excinfo:
        t.get_d(c)
    assert str(excinfo.value) == (
        "get_d(): incompatible function arguments. The following argument types are supported:\n"
        "    1. get_d(arg: test_classes_ext.D, /) -> int\n"
        "\n"
        "Invoked with types: test_classes_ext.C")
    assert t.get_d(a) == 11
    assert t.get_d(b) == 102
    assert t.get_d(b2) == 103
    assert t.get_d(d) == 10005


def test14_operators():
    a = t.Int(1)
    b = t.Int(2)
    assert repr(a + b) == "3"
    with pytest.raises(TypeError) as excinfo:
        assert repr(a - b) == "3"
    assert "unsupported operand type" in str(excinfo.value)
    assert repr(a - 2) == "-1"
    a += b
    assert repr(a) == "3"
    assert repr(b) == "2"

    assert a.__add__("test") is NotImplemented


def test15_keep_alive_nbtype(clean):
    t.reset()
    s = t.Struct()
    a = t.Dog('Rufus')
    assert t.keep_alive_arg(s, a) is a
    del s
    assert_stats(
        default_constructed=1
    )
    del a
    assert_stats(
        default_constructed=1,
        destructed=1
    )

    t.reset()
    s = t.Struct()
    a = t.Dog('Rufus')
    assert t.keep_alive_ret(a, s) is s
    del a
    assert_stats(
        default_constructed=1
    )
    del s
    assert_stats(
        default_constructed=1,
        destructed=1
    )


def test16_keep_alive_custom(clean):
    constructed = 0
    destructed = 0

    class Struct():
        def __init__(self):
            nonlocal constructed
            constructed += 1

        def __del__(self):
            nonlocal destructed
            destructed += 1

    class Struct2():
        def __init__(self):
            pass

    s = Struct()
    a = Struct2()
    assert t.keep_alive_arg(s, a) is a
    del s
    gc.collect()
    assert constructed == 1 and destructed == 0
    del a
    gc.collect()
    assert constructed == 1 and destructed == 1

    s = Struct()
    a = Struct2()
    assert t.keep_alive_ret(a, s) is s
    del a
    gc.collect()
    assert constructed == 2 and destructed == 1
    del s
    gc.collect()
    assert constructed == 2 and destructed == 2

def f():
    pass

class MyClass:
    def f(self):
        pass

    class NestedClass:
        def f(self):
            pass

def test17_name_qualname_module():
    # First, check what CPython does
    assert f.__module__ == 'test_classes'
    assert f.__name__ == 'f'
    assert f.__qualname__ == 'f'
    assert MyClass.__name__ == 'MyClass'
    assert MyClass.__qualname__ == 'MyClass'
    assert MyClass.__module__ == 'test_classes'
    assert MyClass.f.__name__ == 'f'
    assert MyClass.f.__qualname__ == 'MyClass.f'
    assert MyClass.f.__module__ == 'test_classes'
    assert MyClass.NestedClass.__name__ == 'NestedClass'
    assert MyClass.NestedClass.__qualname__ == 'MyClass.NestedClass'
    assert MyClass.NestedClass.__module__ == 'test_classes'
    assert MyClass.NestedClass.f.__name__ == 'f'
    assert MyClass.NestedClass.f.__qualname__ == 'MyClass.NestedClass.f'
    assert MyClass.NestedClass.f.__module__ == 'test_classes'

    # Now, check the extension module
    assert t.f.__module__ == 'test_classes_ext'
    assert t.f.__name__ == 'f'
    assert t.f.__qualname__ == 'f'
    assert t.MyClass.__name__ == 'MyClass'
    assert t.MyClass.__qualname__ == 'MyClass'
    assert t.MyClass.__module__ == 'test_classes_ext'
    assert t.MyClass.f.__name__ == 'f'
    assert t.MyClass.f.__qualname__ == 'MyClass.f'
    assert t.MyClass.f.__module__ == 'test_classes_ext'
    assert t.MyClass.NestedClass.__name__ == 'NestedClass'
    assert t.MyClass.NestedClass.__qualname__ == 'MyClass.NestedClass'
    assert t.MyClass.NestedClass.__module__ == 'test_classes_ext'
    assert t.MyClass.NestedClass.f.__name__ == 'f'
    assert t.MyClass.NestedClass.f.__qualname__ == 'MyClass.NestedClass.f'
    assert t.MyClass.NestedClass.f.__module__ == 'test_classes_ext'


def test18_static_properties():
    assert t.StaticProperties.value == 23
    t.StaticProperties.value += 1
    assert t.StaticProperties.value == 24
    assert t.StaticProperties.get() == 24
    assert t.StaticProperties2.get() == 24
    t.StaticProperties2.value = 50
    assert t.StaticProperties2.get() == 50
    assert t.StaticProperties.get() == 50
    import pydoc
    assert "Static property docstring" in pydoc.render_doc(t.StaticProperties2)

def test19_supplement():
    c = t.ClassWithSupplement()
    assert t.check_supplement(c)
    assert not t.check_supplement(t.Struct())


def test20_type_callback():
    o = t.ClassWithLen()
    assert len(o) == 123


def test21_low_level(clean):
    s1, s2, s3 = t.test_lowlevel()
    assert s1.value() == 123 and s2.value() == 0 and s3.value() == 345
    del s1
    del s2
    del s3
    assert_stats(
        value_constructed=2,
        copy_constructed=1,
        move_constructed=1,
        destructed=4
    )


def test22_handle_t(clean):
    assert t.test_handle_t.__doc__ == 'test_handle_t(arg: test_classes_ext.Struct, /) -> object'
    s = t.test_handle_t(t.Struct(5))
    assert s.value() == 5
    del s

    with pytest.raises(TypeError) as excinfo:
        t.test_handle_t("test")
    assert "incompatible function argument" in str(excinfo.value)
    assert_stats(
        value_constructed=1,
        destructed=1
    )


def test23_type_object_t(clean):
    assert t.test_type_object_t.__doc__ == 'test_type_object_t(arg: type[test_classes_ext.Struct], /) -> object'
    assert t.test_type_object_t(t.Struct) is t.Struct

    with pytest.raises(TypeError):
        t.test_type_object_t(t.Struct())

    with pytest.raises(TypeError):
        t.test_type_object_t(int)


def test24_none_arg():
    with pytest.raises(TypeError):
        t.none_0(None)
    with pytest.raises(TypeError):
        t.none_1(None)
    with pytest.raises(TypeError):
        t.none_2(arg=None)
    assert t.none_3(None) is True
    assert t.none_4(arg=None) is True
    assert t.none_0.__doc__ == 'none_0(arg: test_classes_ext.Struct, /) -> bool'
    assert t.none_1.__doc__ == 'none_1(arg: test_classes_ext.Struct) -> bool'
    assert t.none_2.__doc__ == 'none_2(arg: test_classes_ext.Struct) -> bool'
    assert t.none_3.__doc__ == 'none_3(arg: Optional[test_classes_ext.Struct]) -> bool'
    assert t.none_4.__doc__ == 'none_4(arg: Optional[test_classes_ext.Struct]) -> bool'


def test25_is_final():
    with pytest.raises(TypeError) as excinfo:
        class MyType(t.FinalType):
            pass
    assert "The type 'test_classes_ext.FinalType' prohibits subclassing!" in str(excinfo.value)


def test26_dynamic_attr(clean):
    l = [None] * 100
    for i in range(100):
        l[i] = t.StructWithAttr(i)

    # Create a big reference cycle..
    for i in range(100):
        l[i].prev = l[i - 1]
        l[i].next = l[i + 1 if i < 99 else 0]
        l[i].t = t.StructWithAttr
        l[i].self = l[i]

    for i in range(100):
        assert l[i].value() == i
        assert l[i].self.value() == i
        assert l[i].prev.value() == (i-1 if i > 0 else 99)
        assert l[i].next.value() == (i+1 if i < 99 else 0)

    del l
    gc.collect()

    assert_stats(
        value_constructed=100,
        destructed=100
    )

def test27_copy_rvp():
    a = t.Struct.create_reference()
    b = t.Struct.create_copy()
    assert a is not b


def test28_pydoc():
    import pydoc
    assert "Some documentation" in pydoc.render_doc(t)


def test29_property_assignment_instance():
    s = t.PairStruct()
    s1 = t.Struct(123)
    s2 = t.Struct(456)
    s.s1 = s1
    s.s2 = s2
    assert s2 is not s.s2 and s1 is not s.s1
    assert s.s1.value() == 123
    assert s.s2.value() == 456
    assert s1.value() == 123
    assert s2.value() == 456

def test30_cycle():
    a = t.Wrapper()
    a.value = a
    del a
