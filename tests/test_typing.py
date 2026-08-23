import test_typing_ext as t
import sys
import pytest
import platform

def test01_parameterize_generic():
    assert str(type(t.Wrapper[int]) == 't.Wrapper[int]')
    if platform.python_implementation() != 'PyPy':
        assert issubclass(t.WrapperFoo, t.Wrapper)
        assert t.WrapperFoo.__bases__ == (t.Wrapper,)
        assert t.WrapperFoo.__orig_bases__ == (t.Wrapper[t.Foo],)


def test02_stub_only_overload():
    # The overload whose 'self' is a 'std::nullptr_t' never matches, so the
    # implementation answers the call while the stub keeps both signatures
    assert t.StubOnlyOverload().value() == 42
