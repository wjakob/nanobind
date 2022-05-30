import test_stl_ext as t
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


# ------------------------------------------------------------------
# The following aren't strictly STL tests, but they are helpful in
# ensuring that move constructors/copy constructors of bound C++ types
# are properly triggered, which the STL type casters depend on.
# ------------------------------------------------------------------

def test01_movable_return(clean):
    assert t.return_movable().value == 5
    assert_stats(
        default_constructed=1,
        move_constructed=1,
        destructed=2)


def test02_movable_return_ptr(clean):
    assert t.return_movable_ptr().value == 5
    assert_stats(
        default_constructed=1,
        destructed=1)


def test03_movable_in_value(clean):
    s = t.Movable()
    t.movable_in_value(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)


def test04_movable_in_lvalue_ref(clean):
    s = t.Movable()
    t.movable_in_lvalue_ref(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test05_movable_in_ptr(clean):
    s = t.Movable()
    t.movable_in_ptr(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test06_movable_in_rvalue_ref(clean):
    s = t.Movable()
    t.movable_in_rvalue_ref(s)
    assert s.value == 0
    del s
    assert_stats(
        default_constructed=1,
        move_constructed=1,
        destructed=2)


def test07_copyable_return(clean):
    assert t.return_copyable().value == 5
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)


def test08_copyable_return_ptr(clean):
    assert t.return_copyable_ptr().value == 5
    assert_stats(
        default_constructed=1,
        destructed=1)


def test09_copyable_in_value(clean):
    s = t.Copyable()
    t.copyable_in_value(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)


def test10_copyable_in_lvalue_ref(clean):
    s = t.Copyable()
    t.copyable_in_lvalue_ref(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test11_copyable_in_ptr(clean):
    s = t.Copyable()
    t.copyable_in_ptr(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test12_copyable_in_rvalue_ref(clean):
    s = t.Copyable()
    t.copyable_in_rvalue_ref(s)
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)

# ------------------------------------------------------------------

def test13_tuple_movable_return(clean):
    assert t.tuple_return_movable()[0].value == 5
    assert_stats(
        default_constructed=1,
        move_constructed=2,
        destructed=3)


def test14_tuple_movable_return_ptr(clean):
    assert t.return_movable_ptr().value == 5
    assert_stats(
        default_constructed=1,
        destructed=1)


def test15_tuple_movable_in_value(clean):
    s = t.Movable()
    t.tuple_movable_in_value((s,))
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)


def test16_tuple_movable_in_lvalue_ref(clean):
    s = t.Movable()
    t.tuple_movable_in_lvalue_ref((s,))
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test17_tuple_movable_in_lvalue_ref_2(clean):
    s = t.Movable()
    t.tuple_movable_in_lvalue_ref_2((s,))
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        destructed=2)


def test18_tuple_movable_in_ptr(clean):
    s = t.Movable()
    t.tuple_movable_in_ptr((s,))
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        destructed=1)


def test19_tuple_movable_in_rvalue_ref(clean):
    s = t.Movable()
    t.tuple_movable_in_rvalue_ref((s,))
    assert s.value == 0
    del s
    assert_stats(
        default_constructed=1,
        move_constructed=1,
        destructed=2)


def test20_tuple_movable_in_rvalue_ref_2(clean):
    s = t.Movable()
    t.tuple_movable_in_rvalue_ref_2((s,))
    assert s.value == 5
    del s
    assert_stats(
        default_constructed=1,
        copy_constructed=1,
        move_constructed=1,
        destructed=3)

# ------------------------------------------------------------------

def test21_tuple_pair_basic():
    assert t.empty_tuple(()) == ()
    assert t.swap_tuple((1, 2.5)) == (2.5, 1)
    assert t.swap_pair((1, 2.5)) == (2.5, 1)

# ------------------------------------------------------------------

def test22_vec_return_movable(clean):
    for i, x in enumerate(t.vec_return_movable()):
        assert x.value == i
    del x
    assert_stats(
        value_constructed=10,
        move_constructed=10,
        destructed=20
    )


def test23_vec_return_copyable(clean):
    for i, x in enumerate(t.vec_return_copyable()):
        assert x.value == i
    del x
    assert_stats(
        value_constructed=10,
        copy_constructed=20,
        destructed=30
    )


def test24_vec_movable_in_value(clean):
    t.vec_moveable_in_value([t.Movable(i) for i in range(10)])
    assert_stats(
        value_constructed=10,
        copy_constructed=10,
        move_constructed=10,
        destructed=30
    )


def test25_vec_movable_in_value(clean):
    t.vec_copyable_in_value([t.Copyable(i) for i in range(10)])
    assert_stats(
        value_constructed=10,
        copy_constructed=20,
        destructed=30
    )


def test26_vec_movable_in_lvalue_ref(clean):
    t.vec_moveable_in_lvalue_ref([t.Movable(i) for i in range(10)])
    assert_stats(
        value_constructed=10,
        move_constructed=10,
        destructed=20
    )


def test27_vec_movable_in_ptr_2(clean):
    t.vec_moveable_in_ptr_2([t.Movable(i) for i in range(10)])
    assert_stats(
        value_constructed=10,
        destructed=10
    )


def test28_vec_movable_in_rvalue_ref(clean):
    t.vec_moveable_in_rvalue_ref([t.Movable(i) for i in range(10)])
    assert_stats(
        value_constructed=10,
        move_constructed=10,
        destructed=20
    )

def test29_opaque_vector():
    f = t.float_vec()
    assert f.size() == 0
    assert isinstance(f, t.float_vec)
    f.push_back(1)
    assert f.size() == 1


def test30_std_function():
    assert t.return_empty_function() is None
    assert t.return_function()(3) == 8
    assert t.call_function(lambda x: 5 + x, 3) == 8

    with pytest.raises(TypeError) as excinfo:
        assert t.call_function(5, 3) == 8
    assert 'incompatible function arguments' in str(excinfo.value)

    with pytest.raises(TypeError) as excinfo:
        assert t.call_function(lambda x, y: x+y, 3) == 8
    assert 'missing 1 required positional argument' in str(excinfo.value)


def test31_vec_type_check():
    with pytest.raises(TypeError) as excinfo:
        t.vec_moveable_in_value(0)

def test32_list():
    assert t.identity_list([]) == []
    assert t.identity_list([1, 2, 3]) == [1, 2, 3]
    assert t.identity_list(()) == []
    assert t.identity_list((1, 2, 3)) == [1, 2, 3]
