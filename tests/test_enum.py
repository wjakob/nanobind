import test_enum_ext as t
import pytest
import platform

def test01_unsigned_enum():
    assert repr(t.Enum.A) == 'test_enum_ext.Enum.A'
    assert repr(t.Enum.B) == 'test_enum_ext.Enum.B'
    assert repr(t.Enum.C) == 'test_enum_ext.Enum.C'
    assert t.Enum.A.__name__ == 'A'
    assert t.Enum.B.__name__ == 'B'
    assert t.Enum.C.__name__ == 'C'
    assert t.Enum.A.name == 'A'
    assert t.Enum.B.name == 'B'
    assert t.Enum.C.name == 'C'

    # The following fails on PyPy: https://github.com/pypy/pypy/issues/4898
    if platform.python_implementation() != 'PyPy':
        assert t.Enum.__doc__.__doc__ == 'enum-level docstring'

    assert t.Enum.A.__doc__ == 'Value A'
    assert t.Enum.B.__doc__ == 'Value B'
    assert t.Enum.C.__doc__ == 'Value C'
    assert int(t.Enum.A) == 0
    assert int(t.Enum.B) == 1
    assert int(t.Enum.C) == 0xffffffff
    assert t.Enum.A.value == 0
    assert t.Enum.B.value == 1
    assert t.Enum.C.value == 0xffffffff
    assert t.Enum(0) is t.Enum.A
    assert t.Enum(1) is t.Enum.B
    assert t.Enum(0xffffffff) is t.Enum.C
    assert t.Enum(t.Enum.A) is t.Enum.A
    assert t.Enum(t.Enum.B) is t.Enum.B
    assert t.Enum(t.Enum.C) == t.Enum.C
    assert t.from_enum(t.Enum.A) == 0
    assert t.from_enum(t.Enum.B) == 1
    assert t.from_enum(t.Enum.C) == 0xffffffff
    assert t.to_enum(0).__name__ == 'A'
    assert t.to_enum(0) == t.Enum.A
    assert t.to_enum(1) == t.Enum.B
    assert t.to_enum(0xffffffff) == t.Enum.C
    assert hash(t.Enum.A) == 0
    assert hash(t.Enum.B) == 1
    assert hash(t.Enum.C) == -2 # -1 is an invalid hash value.

    with pytest.raises(RuntimeError) as excinfo:
        t.to_enum(5).__name__
        assert t.test_bad_tuple()
    assert 'nb_enum: could not find entry!' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        t.Enum(0x123)
    assert 'test_enum_ext.Enum(): could not convert the input into an enumeration value!' in str(excinfo.value)


def test02_signed_enum():
    assert repr(t.SEnum.A) == 'test_enum_ext.SEnum.A'
    assert repr(t.SEnum.B) == 'test_enum_ext.SEnum.B'
    assert repr(t.SEnum.C) == 'test_enum_ext.SEnum.C'
    assert int(t.SEnum.A) == 0
    assert int(t.SEnum.B) == 1
    assert int(t.SEnum.C) == -1
    assert t.SEnum.A.value == 0
    assert t.SEnum.B.value == 1
    assert t.SEnum.C.value == -1
    assert t.SEnum(0) is t.SEnum.A
    assert t.SEnum(1) is t.SEnum.B
    assert t.SEnum(-1) is t.SEnum.C
    assert t.from_enum(t.SEnum.A) == 0
    assert t.from_enum(t.SEnum.B) == 1
    assert t.from_enum(t.SEnum.C) == -1
    assert hash(t.SEnum.A) == 0
    assert hash(t.SEnum.B) == 1
    assert hash(t.SEnum.C) == -2 # -1 is an invalid hash value.


def test03_enum_arithmetic():
    assert t.SEnum.B + 2 == 3
    assert t.SEnum.B + 2.5 == 3.5
    assert 2 + t.SEnum.B == 3
    assert 2.5 + t.SEnum.B == 3.5
    assert t.SEnum.B >> t.SEnum.B == 0
    assert t.SEnum.B << t.SEnum.B == 2
    assert -t.SEnum.B == -1 and -t.SEnum.C == 1
    assert t.SEnum.B & t.SEnum.B == 1
    assert t.SEnum.B & ~t.SEnum.B == 0

    with pytest.raises(TypeError, match="unsupported operand type"):
        t.Enum.B + 2
    with pytest.raises(TypeError, match="unsupported operand type"):
        t.SEnum.B - "1"
    with pytest.raises(TypeError, match="unsupported operand type"):
        t.SEnum.B >> 1.0


def test04_enum_export():
    assert t.Item1 is t.ClassicEnum.Item1 and int(t.Item1) == 0
    assert t.Item2 is t.ClassicEnum.Item2 and int(t.Item2) == 1

# test for issue #39
def test05_enum_property():
    w = t.EnumProperty()
    assert w.read_enum == t.Enum.A
    assert str(w.read_enum) == 'test_enum_ext.Enum.A'

def test06_enum_with_custom_slots():
    # Custom | operator returns an enum
    assert t.Color.Red | t.Color.Green | t.Color.Blue is t.Color.White
    assert t.Color.Black | t.Color.Black is t.Color.Black
    # Other operators (via is_arithmetic) return ints
    yellow = t.Color.Red + t.Color.Green
    assert type(yellow) is int
    assert yellow == t.Color.Yellow and yellow is not t.Color.Yellow


def test07_enum_entries_dict_is_protected():
    with pytest.raises(AttributeError, match="internal nanobind attribute"):
        setattr(t.Color, "@entries", {})
    with pytest.raises(AttributeError, match="internal nanobind attribute"):
        delattr(t.Color, "@entries")
    assert getattr(t.Color, "@entries")[3] == ("Yellow", None, t.Color.Yellow)


def test08_enum_comparisons():
    assert int(t.Enum.B) == int(t.SEnum.B) == 1
    for enum in (t.Enum, t.SEnum):
        value = getattr(enum, "B")
        assert value != str(int(value))
        assert value != int(value) + 0.4
        assert value != float(int(value))
        assert value < int(value) + 0.4
        for i in (0, 0.5, 1, 1.5, 2):
            assert (value == i) == (int(value) == i)
            assert (value != i) == (int(value) != i)
            assert (value < i) == (int(value) < i)
            assert (value <= i) == (int(value) <= i)
            assert (value >= i) == (int(value) >= i)
            assert (value > i) == (int(value) > i)

            assert (i == value) == (i == int(value))
            assert (i != value) == (i != int(value))
            assert (i < value) == (i < int(value))
            assert (i <= value) == (i <= int(value))
            assert (i >= value) == (i >= int(value))
            assert (i > value) == (i > int(value))

        for unrelated in (None, "hello", "1"):
            assert value != unrelated and unrelated != value
            assert not (value == unrelated) and not (unrelated == value)
            with pytest.raises(TypeError):
                value < unrelated
            with pytest.raises(TypeError):
                unrelated < value

    # different enum types never compare equal ...
    assert t.Enum.B != t.SEnum.B and t.SEnum.B != t.Enum.B
    assert not (t.Enum.B == t.SEnum.B) and not (t.SEnum.B == t.Enum.B)
    assert t.Enum.B != t.SEnum.C and t.SEnum.C != t.Enum.B

    # ... but can be ordered by their underlying values
    assert t.Enum.A < t.SEnum.B
    assert t.SEnum.B > t.Enum.A
    assert t.Enum.A <= t.SEnum.A and t.Enum.A >= t.SEnum.A
    assert t.Enum.A != t.SEnum.A
