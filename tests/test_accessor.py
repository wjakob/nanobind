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
