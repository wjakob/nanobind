import typing
from .submodule import py_stub_submodule2_test

class AClass(py_stub_submodule2_test.BClass):
    STATIC_VAR: int = 5

    class NestedClass:
        pass

    def __init__(self, x):
        pass

    def method(self, x: str):
        pass

    @staticmethod
    def static_method(x):
        pass

    @classmethod
    def class_method(cls, x):
        pass

    @typing.overload
    def overloaded(self, x: int) -> None:
        """docstr 1"""

    @typing.overload
    def overloaded(self, x: str) -> None:
        """docstr 2"""

    def overloaded(self, x):
        pass

    @typing.overload
    def overloaded_2(self, x: int) -> None: ...

    @typing.overload
    def overloaded_2(self, x: str) -> None: ...

    def overloaded_2(self, x):
        "docstr 3"
