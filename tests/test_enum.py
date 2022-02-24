import test_enum_ext as t
import pytest
import gc

def test01_unsigned_enum():
    assert repr(t.Enum1.A) == 'Enum1.A'
    assert repr(t.Enum1.B) == 'Enum1.B'
    assert repr(t.Enum1.C) == 'Enum1.C'
    assert t.Enum1.A.__name__ == 'A'
    assert t.Enum1.B.__name__ == 'B'
    assert t.Enum1.C.__name__ == 'C'
    assert t.Enum1.A.__doc__ == 'Value A'
    assert t.Enum1.B.__doc__ == 'Value B'
    assert t.Enum1.C.__doc__ == 'Value C'
    assert int(t.Enum1.A) == 0
    assert int(t.Enum1.B) == 1
    assert int(t.Enum1.C) == 0xffffffff
    assert t.Enum1(0) is t.Enum1.A
    assert t.Enum1(1) is t.Enum1.B
    assert t.Enum1(0xffffffff) is t.Enum1.C
    assert t.Enum1(t.Enum1.A) is t.Enum1.A
    assert t.Enum1(t.Enum1.B) is t.Enum1.B
    assert t.Enum1(t.Enum1.C) == t.Enum1.C


def test02_signed_enum():
    assert repr(t.SEnum1.A) == 'SEnum1.A'
    assert repr(t.SEnum1.B) == 'SEnum1.B'
    assert repr(t.SEnum1.C) == 'SEnum1.C'
    assert int(t.SEnum1.A) == 0
    assert int(t.SEnum1.B) == 1
    assert int(t.SEnum1.C) == -1
    assert t.SEnum1(0) is t.SEnum1.A
    assert t.SEnum1(1) is t.SEnum1.B
    assert t.SEnum1(-1) is t.SEnum1.C
