import test_intrusive_ext as t
import pytest
import gc

@pytest.fixture
def clean():
    gc.collect()
    gc.collect()
    t.reset()

def test01_construct(clean):
    o = t.Test()
    assert o.value() == 123
    assert t.get_value_1(o) == 123
    assert t.get_value_2(o) == 123
    assert t.get_value_3(o) == 123
    del o
    gc.collect()
    gc.collect()
    assert t.stats() == (1, 1)


def test02_factory(clean):
    o = t.Test.create_raw()
    assert o.value() == 123
    assert t.get_value_1(o) == 123
    assert t.get_value_2(o) == 123
    assert t.get_value_3(o) == 123
    del o
    gc.collect()
    gc.collect()
    assert t.stats() == (1, 1)


def test03_factory_ref(clean):
    o = t.Test.create_ref()
    assert o.value() == 123
    assert t.get_value_1(o) == 123
    assert t.get_value_2(o) == 123
    assert t.get_value_3(o) == 123
    del o
    gc.collect()
    gc.collect()
    assert t.stats() == (1, 1)

def test04_subclass(clean):
    class MyTest(t.Test):
        def __init__(self, x):
            super().__init__()
            self.x = x

        def value(self):
            return self.x

    o = MyTest(456)
    assert o.value() == 456
    assert t.get_value_1(o) == 456
    assert t.get_value_2(o) == 456
    assert t.get_value_3(o) == 456
    del o
    gc.collect()
    gc.collect()
    assert t.stats() == (1, 1)
