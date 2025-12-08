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

