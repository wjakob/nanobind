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


def test10_installed_name_imports(stubgen, fresh_module, monkeypatch):
    root = fresh_module.__name__
    child = types.ModuleType(root + ".types")
    monkeypatch.setitem(sys.modules, child.__name__, child)
    value = root + ".types.Widget"
    sig = (f'@deco.identity("a\u2028b")\n'
           f'def f(cb: object = lambda a, b: "{value}", '
           f'x: "{value}" = None) -> {value}', None, None)
    baseline = stubgen.StubGen(module=fresh_module)
    mapped = stubgen.StubGen(module=fresh_module, installed_name="installed.ext")
    for sg in (baseline, mapped):
        sg.put_nb_overload(None, sig)
        sg.bind(root, "Widget", name="Renamed", export=True)
        sg.bind(root + "_other")
    # Only imports change, including for quoted annotations and runtime values.
    assert mapped.output == baseline.output
    out = mapped.get()
    assert f"from installed import ext as {root}" in out
    assert "from installed.ext import Widget as Renamed" in out
    assert f"import installed.ext.types as _{root}_types" in out
    assert f"import {root}_other\n" in out
    assert "import " + root + ".types\n" not in out
    assert mapped.get() == out


@pytest.mark.parametrize("installed_name", [None, "py_recursive_stub_test", "installed.ext"])
def test11_installed_name_recursive_cli(stubgen, tmp_path, monkeypatch, installed_name):
    here = pathlib.Path(__file__).parent
    monkeypatch.syspath_prepend(str(here))
    monkeypatch.setenv("NB_STUBGEN", "1")
    args = ["-m", "py_recursive_stub_test", "-r", "-O", str(tmp_path), "-q",
            "-p", str(here / "pattern_file_recursive.nb")]
    if installed_name is not None:
        args += ["--installed-name", installed_name]
    stubgen.main(args)
    root = (tmp_path / "__init__.pyi").read_text()
    nested = (tmp_path / "sub/mod.pyi").read_text()
    if installed_name == "installed.ext":
        assert "from installed import ext as py_recursive_stub_test" in root
        assert "from installed.ext import" in root
        assert "from installed.ext import alias as nickname, bar as bar" in nested
        assert "py_recursive_stub_test.sub.mod" not in root
    else:
        for ref in (here / "py_recursive_stub_test").rglob("*.pyi.ref"):
            generated = tmp_path / ref.relative_to(here / "py_recursive_stub_test").with_suffix("")
            assert generated.read_bytes() == ref.read_bytes()


@pytest.mark.parametrize("args", [
    ["-m", "ext", "--installed-name", ""],
    ["-m", "ext", "--installed-name", "pkg..ext"],
    ["-m", "ext", "--installed-name", "pkg.class"],
    ["-m", "one", "-m", "two", "--installed-name", "pkg.ext"],
    ["-m", "pkg.ext", "--installed-name", "installed.ext"],
])
def test12_installed_name_invalid_cli(stubgen, args, capsys):
    with pytest.raises(SystemExit) as exc:
        stubgen.parse_options(args)
    assert exc.value.code == 2
    error = capsys.readouterr().err
    assert "installed-name" in error
    assert "unrecognized arguments" not in error


@pytest.mark.parametrize("module_name,installed_name", [
    ("ext", "pkg..ext"),
    ("pkg.ext", "installed.ext"),
])
def test13_installed_name_invalid_constructor(stubgen, module_name, installed_name):
    with pytest.raises(ValueError, match="installed_name"):
        stubgen.StubGen(module=types.ModuleType(module_name), installed_name=installed_name)


@pytest.mark.parametrize("reverse", [False, True])
def test14_installed_name_child_import_collision(stubgen, fresh_module, reverse):
    root = fresh_module.__name__
    sg = stubgen.StubGen(module=fresh_module, installed_name="installed.ext")
    alias = "_" + root + "_types"
    setattr(fresh_module, alias, None)
    sg.bind("external", "Widget", name="_" + alias)
    refs = [root + ".types", "types", "__" + alias]
    for ref in reversed(refs) if reverse else refs:
        sg.bind(ref)
    sg.write_ln(f"def f(x: {root}.types.Widget) -> types.ModuleType: ...")
    out = sg.get()
    assert f"import installed.ext.types as ___{alias}\n" in out
    assert "import types\n" in out
    assert "from installed.ext import types" not in out
    assert out.endswith(f"def f(x: {root}.types.Widget) -> types.ModuleType: ...\n")
    assert sg.get() == out
