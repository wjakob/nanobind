import test_holders_ext as t
import pytest
import gc

@pytest.fixture
def clean():
    gc.collect()
    t.reset()

# ------------------------------------------------------------------

def test01_create(clean):
    e = t.Example(123)
    assert e.value == 123
    assert t.query_shared_1(e) == 123
    assert t.query_shared_2(e) == 123
    del e
    gc.collect()
    assert t.stats() == (1, 1)


def test02_sharedptr_from_python(clean):
    e = t.Example(234)
    w = t.SharedWrapper(e)
    assert w.ptr is e
    del e
    gc.collect()
    assert t.stats() == (1, 0)
    del w
    gc.collect()
    assert t.stats() == (1, 1)

    w = t.SharedWrapper(t.Example(234))
    assert t.stats() == (2, 1)
    w.ptr = t.Example(0)
    gc.collect()
    assert t.stats() == (3, 2)
    del w
    gc.collect()
    assert t.stats() == (3, 3)


def test03_sharedptr_from_cpp(clean):
    e = t.Example.make(5)
    assert t.passthrough(e) is e
    assert t.query_shared_1(e) == 5
    assert t.query_shared_2(e) == 5

    w = t.SharedWrapper(e)
    assert e is not w.value
    assert w.value == 5
    w.value = 6
    assert e.value == 6
    del w, e

    e = t.Example.make_shared(6)
    assert t.query_shared_1(e) == 6
    assert t.query_shared_2(e) == 6
    assert t.passthrough(e) is e

    w = t.SharedWrapper(e)
    assert e is not w.value
    assert w.value == 6
    del w, e
    assert t.stats() == (2, 2)

# ------------------------------------------------------------------

def test04_uniqueptr_from_cpp(clean):
    a = t.unique_from_cpp()
    b = t.unique_from_cpp_2()
    assert a.value == 1
    assert b.value == 2
    del a, b
    gc.collect()
    assert t.stats() == (2, 2)


def test05_uniqueptr_from_cpp(clean):
    # Test ownership exchange when the object has been created on the C++ side
    a = t.unique_from_cpp()
    b = t.unique_from_cpp_2()
    wa = t.UniqueWrapper(a)
    wb = t.UniqueWrapper(b)
    with pytest.warns(RuntimeWarning, match='nanobind: attempted to access an uninitialized instance of type \'test_holders_ext.Example\'!'):
        with pytest.raises(TypeError) as excinfo:
            assert a.value == 1
        assert 'incompatible function arguments' in str(excinfo.value)
    with pytest.warns(RuntimeWarning, match='nanobind: attempted to access an uninitialized instance of type \'test_holders_ext.Example\'!'):
        with pytest.raises(TypeError) as excinfo:
            assert b.value == 2
        assert 'incompatible function arguments' in str(excinfo.value)
    del a, b
    del wa, wb
    gc.collect()
    assert t.stats() == (2, 2)

    t.reset()
    a = t.unique_from_cpp()
    b = t.unique_from_cpp_2()
    wa = t.UniqueWrapper(a)
    wb = t.UniqueWrapper(b)

    a2 = wa.get()
    b2 = wb.get()
    assert a2.value == 1 and b2.value == 2
    assert a2 is a and b2 is b
    assert a.value == 1 and b.value == 2
    assert t.stats() == (2, 0)
    del a, b, a2, b2
    gc.collect()
    assert t.stats() == (2, 2)


def test06_uniqueptr_from_py(clean):
    # Test ownership exchange when the object has been created on the Python side
    a = t.Example(1)
    with pytest.warns(RuntimeWarning, match=r'nanobind::detail::nb_relinquish_ownership()'):
        with pytest.raises(TypeError) as excinfo:
            wa = t.UniqueWrapper(a)
    wa = t.UniqueWrapper2(a)
    with pytest.warns(RuntimeWarning, match='nanobind: attempted to access an uninitialized instance of type \'test_holders_ext.Example\'!'):
        with pytest.raises(TypeError) as excinfo:
            assert a.value == 1
        assert 'incompatible function arguments' in str(excinfo.value)
    a2 = wa.get()
    assert a2.value == 1 and a is a2
    del a, a2
    gc.collect()
    assert t.stats() == (1, 1)

def test07_uniqueptr_passthrough(clean):
    assert t.passthrough_unique(t.unique_from_cpp()).value == 1
    assert t.passthrough_unique(t.unique_from_cpp_2()).value == 2
    assert t.passthrough_unique_2(t.unique_from_cpp()).value == 1
    assert t.passthrough_unique_2(t.unique_from_cpp_2()).value == 2
    gc.collect()
    assert t.stats() == (4, 4)
    t.reset()

    with pytest.warns(RuntimeWarning, match=r'nanobind::detail::nb_relinquish_ownership()'):
        with pytest.raises(TypeError):
            assert t.passthrough_unique(t.Example(1)).value == 1
    assert t.passthrough_unique_2(t.Example(1)).value == 1
    assert t.stats() == (2, 2)
