"""
Unit tests that examine the behavior of the ``StubGen`` class directly.

Only test aspects here that require access to stubgen internals and cannot be
tested end-to-end via the ``.pyi.ref`` comparison in test_stubs.py.
"""

import ast
import importlib.util
import pathlib
import platform
import re
import sys
import types

import pytest

is_unsupported = platform.python_implementation() == 'PyPy'
pytestmark = pytest.mark.skipif(
    is_unsupported, reason="Stub generation is only tested on CPython")


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


# ---------------------------------------------------------------------------
# simplify_types / is_valid_module
# ---------------------------------------------------------------------------

def test02_no_import_of_nonexistent_module(sg):
    # 'email.NoSuchThing' does not exist, only 'email' may be imported
    assert sg.simplify_types("email.NoSuchThing.X") == "email.NoSuchThing.X"
    assert "email" in sg.module_refs
    assert "email.NoSuchThing" not in sg.module_refs


def test03_id_seq_boundaries(sg):
    # A dotted sequence that continues an identifier is not a name reference
    assert sg.simplify_types("3foo.bar") == "3foo.bar"
    assert "foo" not in sg.module_refs
    assert sg.simplify_types("typing.Optional") == "Optional"


# ---------------------------------------------------------------------------
# classmethod first-parameter rewrite
# ---------------------------------------------------------------------------

def test04_classmethod_bracketed_annotation(sg):
    sig = ("def create(cls: GenericType[SomeOtherType[A, B]], arg: int) -> None", None, None)
    sg.put_nb_overload(None, sig, is_classmethod=True)
    assert "def create(cls, arg: int) -> None: ..." in sg.output


# ---------------------------------------------------------------------------
# write-only properties (nanobind itself cannot create a write-only property
# with an nb_func setter, hence the test emulates one via __nb_signature__)
# ---------------------------------------------------------------------------

def test05_write_only_property_nb(sg):
    class FakeSetter:
        __nb_signature__ = (("def p(self, arg: dict[str, int], /) -> None", ""),)
    prop = property(fset=FakeSetter())
    sg.put_property(prop, "p")
    assert "p: dict[str, int]" in sg.output


# ---------------------------------------------------------------------------
# pattern application
# ---------------------------------------------------------------------------

def test06_doc_marker_needs_object(stubgen, fresh_module):
    # '\doc' is meaningless in '__prefix__'/'__suffix__' patterns
    pattern = stubgen.ReplacePattern(
        re.compile(r"mod\.__prefix__"), ["\\doc", ""], 0)
    sg = stubgen.StubGen(module=fresh_module, patterns=[pattern])
    with pytest.raises(RuntimeError, match="doc"):
        sg.apply_pattern("mod.__prefix__", None)


# ---------------------------------------------------------------------------
# import bindings
# ---------------------------------------------------------------------------

def test07_bind_rendering(stubgen, fresh_module):
    sg = stubgen.StubGen(module=fresh_module)
    sg.bind("numpy")
    sg.bind("numpy", name="numpy", export=True)
    sg.bind("pkg.sub", name="sub", export=True)
    sg.bind("pkg.sub", name="renamed")
    sg.bind("a.b")
    sg.bind("typing", "overload")
    out = sg.get()
    # The redundant form both binds the name and supports 'numpy.X' references
    assert "import numpy as numpy" in out and "import numpy\n" not in out
    assert "from pkg import sub as renamed, sub as sub" in out
    assert "import a.b" in out
    assert "from typing import overload" in out


def test08_helper_import_conflict(stubgen, fresh_module):
    fresh_module.overload = None  # unrelated attribute of the same name
    sg = stubgen.StubGen(module=fresh_module)
    assert sg.bind("typing", "overload") == "_overload"
    assert "from typing import overload as _overload" in sg.get()


def test09_flat_stub_reexports_submodule(stubgen, fresh_module):
    child = types.ModuleType(fresh_module.__name__ + ".child")
    fresh_module.child = child
    sys.modules[child.__name__] = child
    try:
        sg = stubgen.StubGen(module=fresh_module)
        sg.put(fresh_module)
        assert f"from {fresh_module.__name__} import child as child" in sg.get()
    finally:
        del sys.modules[child.__name__]
