import test_typing_ext as t
import sys
import pytest

@pytest.mark.skipif(sys.version_info < (3, 9), reason="requires python3.9 or higher")
def test01_parameterize_generic():
    assert str(type(t.Wrapper[int]) == 't.Wrapper[int]')

