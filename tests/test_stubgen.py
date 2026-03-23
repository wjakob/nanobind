"""Explicitly test stubgen behaviour for edge cases."""

import importlib.util
import os
import types

import pytest


# Load StubGen directly from src/stubgen.py so the test works without
# nanobind being pip-installed (the CI runs pytest from the cmake build
# directory where nanobind is not on sys.path).
def _load_stubgen():
    # When run from the source tree: tests/ -> src/stubgen.py
    # When run from the build tree: build/tests/ -> ../../src/stubgen.py
    test_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(test_dir, "..", "src", "stubgen.py"),
        os.path.join(test_dir, "..", "..", "src", "stubgen.py"),
    ]
    for path in candidates:
        path = os.path.normpath(path)
        if os.path.isfile(path):
            spec = importlib.util.spec_from_file_location("stubgen", path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
    pytest.skip("Could not locate src/stubgen.py")


stubgen_mod = _load_stubgen()
StubGen = stubgen_mod.StubGen


class MockBooleanArray:
    """
    Simulates a boolean array that cannot be evaluated as a single boolean.
    Calling ``bool()`` on this object raises a ValueError, mimicking the
    behaviour of numpy.ndarray.
    """
    def __bool__(self):
        raise ValueError("MockBooleanArray cannot be evaluated as a boolean value.")


class MockNumpyArray:
    """
    Simulates a numpy.ndarray where ``__eq__`` returns a boolean array-like
    object rather than a plain bool.
    """
    def __eq__(self, other):
        return MockBooleanArray()


def test_put_uses_identity_check_for_recursion_guard():
    """
    StubGen.put must use identity (``is``) rather than equality (``in``) to
    detect cycles.  When an object overrides ``__eq__`` to return a
    non-boolean (e.g. numpy arrays), using ``in`` calls ``__eq__`` and then
    ``__bool__`` on the result, raising a ValueError.
    """
    dummy_module = types.ModuleType("dummy")
    dummy_module.my_array = MockNumpyArray()

    sg = StubGen(dummy_module)

    # This line would raise ValueError before the fix.
    sg.put(dummy_module, name="dummy")

    assert "my_array" in sg.get()
