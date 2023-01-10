import pytest

import test_bind_map_ext as t


def test_map_string_double():
    mm = t.MapStringDouble()
    mm["a"] = 1
    mm["b"] = 2.5

    assert list(mm) == ["a", "b"]
    assert "b" in mm
    assert "c" not in mm
    assert 123 not in mm

    # Check that keys, values, items are views, not merely iterable
    keys = mm.keys()
    values = mm.values()
    items = mm.items()
    assert list(keys) == ["a", "b"]
    assert len(keys) == 2
    assert "a" in keys
    assert "c" not in keys
    assert 123 not in keys
    assert list(items) == [("a", 1), ("b", 2.5)]
    assert len(items) == 2
    assert ("b", 2.5) in items
    assert "hello" not in items
    assert ("b", 2.5, None) not in items
    assert list(values) == [1, 2.5]
    assert len(values) == 2
    assert 1 in values
    assert 2 not in values
    # Check that views update when the map is updated
    mm["c"] = -1
    assert list(keys) == ["a", "b", "c"]
    assert list(values) == [1, 2.5, -1]
    assert list(items) == [("a", 1), ("b", 2.5), ("c", -1)]

    um = t.UnorderedMapStringDouble()
    um["ua"] = 1.1
    um["ub"] = 2.6

    assert sorted(list(um)) == ["ua", "ub"]
    assert list(um.keys()) == list(um)
    assert sorted(list(um.items())) == [("ua", 1.1), ("ub", 2.6)]
    assert list(zip(um.keys(), um.values())) == list(um.items())
    assert "UnorderedMapStringDouble" in str(um)

    assert type(keys).__qualname__ == 'MapStringDouble.KeyView'
    assert type(values).__qualname__ == 'MapStringDouble.ValueView'
    assert type(items).__qualname__ == 'MapStringDouble.ItemView'


def test_map_string_double_const():
    mc = t.MapStringDoubleConst()
    mc["a"] = 10
    mc["b"] = 20.5

    umc = t.UnorderedMapStringDoubleConst()
    umc["a"] = 11
    umc["b"] = 21.5

    str(umc)


def test_maps_with_noncopyable_values():
    # std::map
    mnc = t.get_mnc(5)
    for i in range(1, 6):
        assert mnc[i].value == 10 * i

    vsum = 0
    for k, v in mnc.items():
        assert v.value == 10 * k
        vsum += v.value

    assert vsum == 150

    # std::unordered_map
    mnc = t.get_umnc(5)
    for i in range(1, 6):
        assert mnc[i].value == 10 * i

    vsum = 0
    for k, v in mnc.items():
        assert v.value == 10 * k
        vsum += v.value

    assert vsum == 150

    # nested std::map<std::vector>
    nvnc = t.get_nvnc(5)
    for i in range(1, 6):
        for j in range(0, 5):
            assert nvnc[i][j].value == j + 1

    # Note: maps do not have .values()
    for _, v in nvnc.items():
        for i, j in enumerate(v, start=1):
            assert j.value == i

    # nested std::map<std::map>
    nmnc = t.get_nmnc(5)
    for i in range(1, 6):
        for j in range(10, 60, 10):
            assert nmnc[i][j].value == 10 * j

    vsum = 0
    for _, v_o in nmnc.items():
        for k_i, v_i in v_o.items():
            assert v_i.value == 10 * k_i
            vsum += v_i.value

    assert vsum == 7500

    # nested std::unordered_map<std::unordered_map>
    numnc = t.get_numnc(5)
    for i in range(1, 6):
        for j in range(10, 60, 10):
            assert numnc[i][j].value == 10 * j

    vsum = 0
    for _, v_o in numnc.items():
        for k_i, v_i in v_o.items():
            assert v_i.value == 10 * k_i
            vsum += v_i.value

    assert vsum == 7500


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
