"""Explicitly test stubgen behaviour for edge cases."""

import types
from nanobind.stubgen import StubGen


class MockBooleanArray:
    """
    Mock class simulating a boolean array that cannot be evaluated as a single boolean.
    Calling `bool()` on this object will raise a ValueError.
    """
    def __bool__(self):
        raise ValueError("MockBooleanArray cannot be evaluated as a boolean value.")

class MockNumpyArray:
    """
    Mock class simulating a numpy.ndarray where `__eq__` returns a boolean array-like object.
    """
    def __eq__(self, other):
        return MockBooleanArray()

def test_stubgen_recursion_check_with_array_like_objects():
    """
    This test verifies that the recursion check in `StubGen.put` uses an
    identity check (`is`) rather than an equality check (`in`).
    
    When an object like a NumPy array is checked for containment (`in`),
    its `__eq__` method is called. This returns a boolean array, and
    the `in` operator then tries to determine the truthiness of that array,
    which raises a ValueError. An identity check avoids calling `__eq__`
    altogether.
    """
    dummy_module = types.ModuleType("dummy")
    dummy_module.my_array = MockNumpyArray()

    sg = StubGen(dummy_module)

    # Ensure that no exception is raised when adding a value with a Numpy-like __eq__.
    sg.put(dummy_module, name="dummy")

    assert "my_array" in sg.get()
