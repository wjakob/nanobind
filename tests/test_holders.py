import test_holders_ext as t
import pytest
import gc

def test01_create():
    gc.collect()
    t.clear()
    e = t.Example(123)
    assert e.value == 123
    assert t.query_shared_1(e) == 123
    assert t.query_shared_2(e) == 123
    del e
    gc.collect()
    assert t.stats() == (1, 1)


def test02_sharedptr_from_python():
    gc.collect()
    t.clear()
    e = t.Example(234)
    w = t.ExampleWrapper(e)
    assert w.test is e
    del e
    gc.collect()
    assert t.stats() == (1, 0)
    del w
    gc.collect()
    assert t.stats() == (1, 1)

    w = t.ExampleWrapper(t.Example(234))
    assert t.stats() == (2, 1)
    w.test = t.Example(0)
    gc.collect()
    assert t.stats() == (3, 2)
    del w
    gc.collect()
    assert t.stats() == (3, 3)


def test03_sharedptr_from_cpp():
    gc.collect()
    t.clear()
    e = t.Example.make(5)
    assert t.passthrough(e) is e
    assert t.query_shared_1(e) == 5
    assert t.query_shared_2(e) == 5

    w = t.ExampleWrapper(e)
    assert e is not w.value
    assert w.value == 5
    w.value = 6
    assert e.value == 6
    del w, e

    e = t.Example.make_shared(6)
    assert t.query_shared_1(e) == 6
    assert t.query_shared_2(e) == 6
    assert t.passthrough(e) is e

    w = t.ExampleWrapper(e)
    assert e is not w.value
    assert w.value == 6
    del w, e
    assert t.stats() == (2, 2)
