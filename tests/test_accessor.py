import sys
import types

import pytest

import test_accessor_ext as t


def test_01_str_attr_inplace_mutation():
    """
    Tests that a C++ expression like obj.attr("foo") += ... 
    can actually modify the object in-place.
    """
    a = t.test_str_attr_accessor_inplace_mutation()
    assert a.value == 1


def test_02_str_item_inplace_mutation():
    """
    Similar to test 01, but tests obj["foo"] (keyed attribute access)
    on the C++ side.
    """
    d = t.test_str_item_accessor_inplace_mutation()
    assert d.keys() == {"a"}
    assert d["a"] == 1


def test_03_num_item_list_inplace_mutation():
    """
    Similar to test 01, but tests l[n] (index access)
    on the C++ side, where l is an ``nb::list``.
    """
    l = t.test_num_item_list_accessor_inplace_mutation()
    assert len(l) == 1
    assert l[0] == 1


def test_04_obj_item_inplace_mutation():
    """
    Similar to test 01, but tests obj[h] (handle access)
    on the C++ side.
    """
    d = t.test_obj_item_accessor_inplace_mutation()
    assert len(d) == 1
    assert d.keys() == {0}
    assert d[0] == 1  # dict lookup


def test_05_obj_item_accessor_borrows_key():
    """
    An accessor keyed by an lvalue (obj[k] on the C++ side, where k is a named
    variable) borrows the key instead of taking a reference to it.
    """
    assert t.test_obj_item_accessor_borrows_key()


def test_06_obj_item_accessor_owns_key():
    """
    An accessor keyed by a temporary must keep that key alive for its own
    lifetime.
    """
    assert t.test_obj_item_accessor_owns_key()


def test_07_obj_attr_accessor_owns_key():
    """
    Same as the previous test, for attribute access.
    """
    assert t.test_obj_attr_accessor_owns_key()


def test_08_nested_accessor_key():
    """
    An accessor keyed by another accessor must capture that key, since the
    inner accessor releases its reference at the end of the full expression.
    """
    assert t.test_nested_accessor_key()


def test_09_accessor_conversion_refcount():
    """
    Converting a temporary accessor to nb::object hands out the accessor's own
    reference, while converting a named accessor takes a new one.
    """
    assert t.test_accessor_conversion_refcount()


def test_10_dynamic_keys_uncached():
    """
    Attribute access with runtime-generated keys goes through the pointer
    path of the string cache. It must behave correctly and must not grow the
    interpreter's interned-string table, whose entries are immortal on
    free-threaded builds.
    """
    o = types.SimpleNamespace()
    t.setattr_dynamic(o, "dyn_first", 1)
    assert t.getattr_dynamic(o, "dyn_first") == 1

    # CPython itself interns names stored by setattr, so the growth check
    # must use a read-only operation
    if hasattr(sys, "getunicodeinternedsize"):
        base = sys.getunicodeinternedsize()
        for i in range(5000):
            assert not t.hasattr_dynamic(o, "dyn_key_%d" % i)
        assert sys.getunicodeinternedsize() - base < 100


def test_11_failing_accessor_conversion():
    """
    An accessor returned by a bound function performs its lookup while the
    return value is converted. A failure must raise instead of terminating.
    """
    assert t.test_return_item_accessor({"k": 3}) == 3
    assert t.test_return_dict_accessor({"k": 3}) == 3

    with pytest.raises(KeyError, match="k"):
        t.test_return_item_accessor({})

    with pytest.raises(KeyError, match="k"):
        t.test_return_dict_accessor({})


def test_12_getattr_object_key_default():
    """
    nb::getattr() with an object key returns the default for a missing
    attribute and swallows errors raised by a custom __getattr__.
    """
    o = types.SimpleNamespace()
    o.present = 1
    assert t.getattr_obj_def(o, "present", None) == 1
    assert t.getattr_obj_def(o, "absent", 5) == 5
    assert t.getattr_obj_def(o, "absent", None) is None

    class Raising:
        def __getattr__(self, name):
            raise ValueError("nope")

    assert t.getattr_obj_def(Raising(), "absent", 7) == 7
