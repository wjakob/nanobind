import inspect
import subprocess
import sys
import sysconfig
import weakref
from common import collect
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import MethodType

import pytest
import test_functions_ext as functions
import test_introspection_ext as t

P = inspect.Parameter


def check_signature(fn, expected):
    # Check both public annotation APIs and the annotations carried by Signature.
    assert fn.__annotations__ == inspect.get_annotations(fn) == expected
    signature = inspect.signature(fn)
    for name, annotation in expected.items():
        actual = signature.return_annotation if name == "return" else signature.parameters[name].annotation
        assert actual == annotation
    return signature


@pytest.mark.parametrize("fn,kinds", [
    (functions.test_kw_only_all, [P.KEYWORD_ONLY] * 2),
    (functions.test_kw_only_with_defaults, [P.POSITIONAL_ONLY, P.POSITIONAL_OR_KEYWORD] + [P.KEYWORD_ONLY] * 2),
    (functions.test_args_kwonly_kwargs, [P.POSITIONAL_OR_KEYWORD] * 2 +
     [P.VAR_POSITIONAL, P.KEYWORD_ONLY, P.VAR_KEYWORD]),
])
def test_parameter_kinds(fn, kinds):
    # Native positional, keyword-only, and variadic arguments keep their kinds.
    assert [p.kind for p in inspect.signature(fn).parameters.values()] == kinds


def test_signature_binding():
    # Signature.bind must accept/reject the same argument layouts as dispatch.
    fn = functions.test_kw_only_with_defaults
    signature = inspect.signature(fn)
    bound = signature.bind(z=6)
    bound.apply_defaults()
    assert fn(*bound.args, **bound.kwargs) == (3, 4, 5, 6)
    for args, kwargs in [((3, 4, 5), {"z": 6}), ((), {})]:
        for call in [signature.bind, fn]:
            with pytest.raises(TypeError):
                call(*args, **kwargs)


@pytest.mark.parametrize("display_override", [False, True], ids=["default", "arg_sig"])
def test_default_identity(display_override):
    # Defaults retain identity without invoking repr or using nb::arg::sig text.
    class Default:
        calls = 0
        def __repr__(self):
            self.calls += 1
            raise AssertionError("introspection must not render a default")

    value = Default()
    ref = weakref.ref(value)
    fn = t.make_function(value, display_override)
    signature = check_signature(fn, {"x": "object", "return": "object"})
    assert signature.parameters["x"].default is value and fn() is value
    # Results are fresh derived objects and do not leak references after release.
    fn.__annotations__["x"] = "changed"
    assert fn.__annotations__["x"] == "object"
    assert signature is not inspect.signature(fn) and value.calls == 0
    if not sysconfig.get_config_var("Py_GIL_DISABLED"):  # FT functions are immortal.
        del signature, fn, value
        collect()
        assert ref() is None


@pytest.mark.parametrize("name,x,kind", [
    ("unnamed", 3, P.POSITIONAL_ONLY),
    ("unnamed_none", None, P.POSITIONAL_ONLY),
    ("unnamed_default", 3, P.POSITIONAL_ONLY),
    ("named", 3.5, P.POSITIONAL_OR_KEYWORD),
])
def test_method_receiver(name, x, kind):
    # Only a dispatcher that accepts keyword receivers may advertise self=.
    obj = t.Methods()
    fn = getattr(t.Methods, name)
    signature = inspect.signature(fn)
    assert signature.parameters["self"].kind is kind
    bound = signature.bind(self=obj, x=x) if kind is P.POSITIONAL_OR_KEYWORD else signature.bind(obj, x)
    assert fn(*bound.args, **bound.kwargs) == getattr(obj, name)(x) == x
    if kind is P.POSITIONAL_ONLY:
        for call in [signature.bind, fn]:
            with pytest.raises(TypeError):
                call(self=obj, arg=x)


def test_unnamed_defaults():
    # An unnamed default stays positional-only and retains its actual value.
    obj = t.Methods()
    for fn, args in [(t.Methods.unnamed_default, (obj,)), (obj.unnamed_default, ())]:
        signature = inspect.signature(fn)
        assert signature.parameters["arg"].kind is P.POSITIONAL_ONLY
        assert signature.parameters["arg"].default == 1
        assert fn(*signature.bind(*args).args) == 1
    for fn, args in [(t.Methods.mixed_defaults, (obj,)), (obj.mixed_defaults, ())]:
        signature = check_signature(fn, {"arg0": "int", "y": "int", "return": "int"})
        assert signature.parameters["arg0"].kind is P.POSITIONAL_ONLY
        assert signature.parameters["arg0"].default == 1
        assert signature.parameters["y"].default == 2
        bound = signature.bind(*args, y=4)
        bound.apply_defaults()
        assert fn(*bound.args, **bound.kwargs) == 5
    # Follow inspect's validation; Python 3.10 accepts this cross-kind ordering.
    try:
        inspect.Signature([P("arg0", P.POSITIONAL_ONLY, default=1),
                           P("y", P.POSITIONAL_OR_KEYWORD)])
    except ValueError:
        accepts_order = False
    else:
        accepts_order = True
    for fn, args in [(t.Methods.mixed, (obj,)), (obj.mixed, ())]:
        assert fn(*args, y=4) == 5
        assert fn.__annotations__ == {"arg0": "int", "y": "int", "return": "int"}
        if accepts_order:
            bound = inspect.signature(fn).bind(*args, y=4)
            bound.apply_defaults()
            assert fn(*bound.args, **bound.kwargs) == 5
        else:
            with pytest.raises(ValueError, match="non-default argument follows default argument"):
                inspect.signature(fn)


def test_keyword_receiver():
    # Constructors and methods can accept self= with all other arguments keyword-only.
    cls = functions.kw_only_methods
    obj = cls.__new__(cls)
    init = cls.__init__
    init(**inspect.signature(init).bind(self=obj, v=3).arguments)
    fn = cls.method_2k
    assert fn(**inspect.signature(fn).bind(self=obj, i=2, j=3).arguments) == (2, 3)
    assert obj.v == 3 and list(inspect.signature(obj.method_2k).parameters) == ["i", "j"]


@pytest.mark.parametrize("fn", [
    pytest.param(t.Methods.unnamed, id="native_self"),
    pytest.param(t.Methods.empty, id="native_empty"),
] + [
    pytest.param(t.make_custom_method(f"def example({params})"), id=f"nb_sig({params})") for params in
    ["self, x", "self, /, x", "*args", "", "*, self", "**kwargs", "x, x"]])
def test_bound_signature(fn):
    # Python's MethodType is the oracle, including invalid unbound signatures.
    obj = t.Methods()
    bound = fn.__get__(obj, t.Methods)
    try:
        expected = inspect.signature(MethodType(fn, obj))
    except ValueError:
        with pytest.raises(ValueError):
            inspect.signature(bound)
    else:
        assert inspect.signature(bound) == expected
    assert bound.__annotations__ == fn.__annotations__


def test_overloaded_signature():
    # Reject overloads through function, method, bound method, and nb::sig access.
    obj = t.Methods()
    t.add_overload(7, False)
    t.add_overload(7, False)
    custom = t.target
    del t.target
    for fn in [functions.test_05, t.Methods.overloaded, obj.overloaded, custom]:
        with pytest.raises(ValueError, match="overloaded"):
            inspect.signature(fn)
        assert fn.__annotations__ == {} and len(fn.__nb_signature__) > 1
    assert functions.test_05(1) == 1 and functions.test_05(1.5) == 2


def test_bound_varargs():
    # Binding keeps a leading *args, and dispatch inserts the receiver into it.
    fn = t.Methods().varargs
    assert fn(*inspect.signature(fn).bind(1, 2).args) == 3


def test_custom_signature():
    # nb::sig advertises an interface without executing any of its expressions.
    value = object()
    fn = t.make_custom_function("@missing()\ndef example(x: typing.Literal['->'] = missing()) -> Unknown", value)
    signature = check_signature(fn, {"x": "typing.Literal['->']", "return": "Unknown"})
    assert signature.parameters["x"].default is value and fn() is value
    # A default with no corresponding bound object is recovered from its literal.
    fn = t.make_custom_function("def example(x, /, *args: int, y: str = 'yes', **kwargs: bool) -> None", value)
    signature = check_signature(fn, {"args": "int", "y": "str", "kwargs": "bool", "return": "None"})
    assert [p.kind for p in signature.parameters.values()] == [
        P.POSITIONAL_ONLY, P.VAR_POSITIONAL, P.KEYWORD_ONLY, P.VAR_KEYWORD]
    assert signature.parameters["y"].default == "yes"
    # Invalid syntax blocks introspection but does not affect the actual callable.
    fn = t.make_custom_function("def example(x:)", 7)
    assert fn() == 7
    for attribute in ["__signature__", "__annotations__"]:
        with pytest.raises(SyntaxError):
            getattr(fn, attribute)
    # Unrecoverable defaults block Signature only; annotations need no defaults.
    fn = t.make_custom_function("def example(x, y: int = missing()) -> None", 7)
    assert fn.__annotations__ == {"y": "int", "return": "None"}
    with pytest.raises(ValueError):
        inspect.signature(fn)


@pytest.mark.parametrize("annotation", ["", "nonascii_你好\0tail"])
def test_annotation_strings(annotation):
    # Explicit string annotations survive empty text, Unicode, and embedded NULs.
    check_signature(t.make_custom_function(f"def example(x: {annotation!r}) -> {annotation!r}", 7),
                    {"x": annotation, "return": annotation})


def test_native_annotations():
    # Embedded arrows must not switch nested input casters to their output form.
    check_signature(t.arrow_annotations, {"first": "typing.Literal['->']",
        "values": "collections.abc.Sequence[typing.Literal['->']]", "return": "list[typing.Literal['->']]"})
    assert t.arrow_annotations("->", ["->"]) == ["->"]
    assert t.Methods.unnamed_none.__annotations__["arg"] == "object | None"
    # An unrepresentable parameter order must still expose annotations.
    with pytest.raises(ValueError):
        inspect.signature(t.unnamed_after_named)
    assert t.unnamed_after_named.__annotations__["return"] == "int"


@pytest.mark.parametrize("attribute", ["__annotations__", "__signature__"])
@pytest.mark.parametrize("callback", ["ast_import", "type_lookup"])
def test_reentrant_introspection(attribute, callback):
    # Imports/type lookups may rebind the function while its metadata is in use.
    # A fresh process keeps the AST import cold and exposes stale-record crashes.
    subprocess.run([sys.executable, "-c", """
import gc, sys, weakref
import test_introspection_ext as t
assert 'inspect' not in sys.modules and 'ast' not in sys.modules
assert t.make_function(7)() == 7
assert t.make_function(7).__annotations__ == {'x': 'object', 'return': 'object'}
assert 'inspect' not in sys.modules and 'ast' not in sys.modules
class Default:
    pass
value = Default()
ref = weakref.ref(value)
native = sys.argv[2] == 'type_lookup'
t.add_overload(value, native)
fn = t.target
del value
def replace_binding():
    # Limited-API/PyPy lookup probes then fetches, so rebind only once.
    if not hasattr(t, 'target'):
        return
    t.add_overload(object(), native)
    del t.target
    if native:
        del t.ReentrantType
    gc.collect()
if not native:
    class AstImportHook:
        def find_spec(self, fullname, path, target=None):
            if fullname == 'ast':
                sys.meta_path.remove(self)
                replace_binding()
            return None
    sys.meta_path.insert(0, AstImportHook())
else:
    class ModuleDescriptor:
        def __get__(self, instance, owner):
            replace_binding()
            return 'test_introspection_ext'
    t.ReentrantType.__module__ = ModuleDescriptor()
result = getattr(fn, sys.argv[1])
assert ('inspect' in sys.modules) == (sys.argv[1] == '__signature__')
expected = ({'value': 'test_introspection_ext.ReentrantType', 'x': 'object', 'return': 'object'}
            if native else {'x': 'int', 'return': 'int'})
if sys.argv[1] == '__annotations__':
    assert result == expected
else:
    for name, annotation in expected.items():
        actual = result.return_annotation if name == 'return' else result.parameters[name].annotation
        assert actual == annotation
    assert ref() is not None and result.parameters['x'].default is ref()
assert fn.__nb_signature__ == () and fn.__annotations__ == {}
try:
    fn.__signature__
except ValueError:
    pass
else:
    raise AssertionError('superseded callable has no signature')
""", attribute, callback], check=True, cwd=Path(t.__file__).parent)


def test_concurrent_introspection():
    # Exercise both collectors and overload rejection with concurrent readers.
    def check(_):
        assert inspect.signature(functions.test_02).parameters["up"].default == 8
        assert inspect.signature(t.make_custom_function("def example(x: int = 1) -> int", 7)).parameters["x"].default == 7
        assert functions.test_07.__annotations__ == {}
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(check, range(16)))
