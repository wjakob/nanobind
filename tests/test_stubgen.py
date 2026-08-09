"""
Unit tests that examine the behavior of the ``StubGen`` class directly.
"""

import ast
import importlib.util
import pathlib
import platform
import re
import sys
import types

import pytest

is_unsupported = platform.python_implementation() == 'PyPy' or sys.version_info < (3, 10)
pytestmark = pytest.mark.skipif(
    is_unsupported, reason="Stub generation is only tested on CPython >= 3.10.0")


@pytest.fixture(scope="module")
def stubgen():
    """Load stubgen.py from the build directory copy or the source tree"""
    here = pathlib.Path(__file__).parent
    candidates = [here / "stubgen.py", here.parent / "src" / "stubgen.py"]
    for path in candidates:
        if path.exists():
            spec = importlib.util.spec_from_file_location("stubgen", path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
    raise RuntimeError("Could not locate stubgen.py")


@pytest.fixture()
def fresh_module():
    mod = types.ModuleType("stubgen_test_mod")
    sys.modules[mod.__name__] = mod
    yield mod
    del sys.modules[mod.__name__]


@pytest.fixture()
def sg(stubgen, fresh_module):
    return stubgen.StubGen(module=fresh_module)


# ---------------------------------------------------------------------------
# format_docstr
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("docstr", [
    'contains an embedded """ triple quote',
    'ends in a double quote: "',
    'ends in a backslash: \\',
    'short',
    "multi\nline\ntext with '' single quotes",
])
def test01_format_docstr_roundtrip(sg, docstr):
    formatted = sg.format_docstr(docstr, 0)
    value = ast.literal_eval(formatted.strip())
    assert value.strip() == docstr.strip()
