import platform
import gc
import pytest

is_pypy = platform.python_implementation() == 'PyPy'

def collect():
    if is_pypy:
        for i in range(3):
            gc.collect()
    else:
        gc.collect()

skip_on_pypy = pytest.mark.skipif(
    is_pypy, reason="This test currently fails/crashes PyPy")
