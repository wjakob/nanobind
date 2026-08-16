import sys
import types

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


def test_05_obj_item_accessor_owns_key():
    """
    An accessor created from a handle key (obj[h] on the C++ side) must keep
    a reference to that key alive for its own lifetime.
    """
    assert t.test_obj_item_accessor_owns_key()


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
