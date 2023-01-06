import pytest

import test_bind_map_ext as t

def test_map_delitem():
    mm = t.MapStringDouble()
    mm["a"] = 1
    mm["b"] = 2.5

    assert list(mm) == ["a", "b"]
    assert list(mm.items()) == [("a", 1), ("b", 2.5)]
    del mm["a"]
    assert list(mm) == ["b"]
    assert list(mm.items()) == [("b", 2.5)]

    um = t.UnorderedMapStringDouble()
    um["ua"] = 1.1
    um["ub"] = 2.6

    assert sorted(list(um)) == ["ua", "ub"]
    assert sorted(list(um.items())) == [("ua", 1.1), ("ub", 2.6)]
    del um["ua"]
    assert sorted(list(um)) == ["ub"]
    assert sorted(list(um.items())) == [("ub", 2.6)]

def test_map_view_types():
    map_string_double = t.MapStringDouble()
    unordered_map_string_double = t.UnorderedMapStringDouble()
    map_string_double_const = t.MapStringDoubleConst()
    unordered_map_string_double_const = t.UnorderedMapStringDoubleConst()

    assert map_string_double.keys().__class__.__name__ == "KeysView[str]"
    assert map_string_double.values().__class__.__name__ == "ValuesView[float]"
    assert map_string_double.items().__class__.__name__ == "ItemsView[str, float]"

    keys_type = type(map_string_double.keys())
    assert type(unordered_map_string_double.keys()) is keys_type
    assert type(map_string_double_const.keys()) is keys_type
    assert type(unordered_map_string_double_const.keys()) is keys_type

    values_type = type(map_string_double.values())
    assert type(unordered_map_string_double.values()) is values_type
    assert type(map_string_double_const.values()) is values_type
    assert type(unordered_map_string_double_const.values()) is values_type

    items_type = type(map_string_double.items())
    assert type(unordered_map_string_double.items()) is items_type
    assert type(map_string_double_const.items()) is items_type
    assert type(unordered_map_string_double_const.items()) is items_type