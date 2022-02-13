import test_classes_ext as t
import pytest
import gc

def test01_signature():
    assert t.Struct.__init__.__doc__ == (
        "__init__(*args, **kwargs) -> Any\n"
        "Overloaded function.\n"
        "\n"
        "1. __init__(self) -> None\n"
        "2. __init__(self, arg0: int) -> None")

    assert t.Struct.value.__doc__ == "value(self) -> int"
    assert t.Struct.create_move.__doc__ == "create_move() -> test_classes_ext.Struct"


def test02_instantiate():
    t.reset()

    s1 = t.Struct()
    assert s1.value() == 5
    s2 = t.Struct(10)
    assert s2.value() == 10
    del s1
    del s2
    gc.collect()

    assert t.stats() == {
        'default_constructed': 1,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }


def test03_factory():
    t.reset()
    s = t.Struct()
    assert s.self() is s
    assert s.none() is None
    del s

    assert t.Struct.create_take().value() == 8

    gc.collect()

    assert t.stats() == {
        'default_constructed': 1,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 0,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }
    t.reset()

    assert t.Struct.create_move().value() == 8

    gc.collect()

    assert t.stats() == {
        'default_constructed': 0,
        'value_constructed': 1,
        'copy_constructed': 0,
        'move_constructed': 1,
        'copy_assigned': 0,
        'move_assigned': 0,
        'destructed': 2
    }
