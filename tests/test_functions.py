import test_functions_ext as t
import pytest

def test01_capture():
    # Functions with and without capture object of different sizes
    assert t.test_01() == None
    assert t.test_02(5, 3) == 2
    assert t.test_03(5, 3) == 44
    assert t.test_04() == 60


def test02_default_args():
    # Default arguments
    assert t.test_02() == 7
    assert t.test_02(7) == 6


def test03_kwargs():
    # Basic use of keyword arguments
    assert t.test_02(3, 5) == -2
    assert t.test_02(3, k=5) == -2
    assert t.test_02(k=5, j=3) == -2


def test04_overloads():
    assert t.test_05(0) == 1
    assert t.test_05(0.0) == 2


def test05_signature():
    assert t.test_01.__doc__ == 'test_01() -> None'
    assert t.test_02.__doc__ == 'test_02(j: int = 8, k: int = 1) -> int'
    assert t.test_05.__doc__ == (
        "test_05(*args, **kwargs) -> Any\n"
        "Overloaded function.\n"
        "\n"
        "1. test_05(arg0: int) -> int\n"
        "\n"
        "doc_1\n"
        "\n"
        "2. test_05(arg0: float) -> int\n"
        "\n"
        "doc_2\n")

    assert t.test_07.__doc__ == (
        "test_07(*args, **kwargs) -> Any\n"
        "Overloaded function.\n"
        "\n"
        "1. test_07(arg0: int, arg1: int, *args, **kwargs) -> Tuple[int, int]\n"
        "2. test_07(a: int, b: int, *myargs, **mykwargs) -> Tuple[int, int]")

def test06_signature_error():
    with pytest.raises(TypeError) as excinfo:
        t.test_05("x", y=4)
    assert str(excinfo.value) == (
        "test_05(): incompatible function arguments. The "
        "following argument types are supported:\n"
        "    1. test_05(arg0: int) -> int\n"
        "    2. test_05(arg0: float) -> int\n\n"
        "Invoked with types: str, kwargs = { y: int }")


def test07_raises():
    with pytest.raises(RuntimeError) as excinfo:
        t.test_06()
    assert str(excinfo.value) == "oops!"


def test08_args_kwargs():
    assert t.test_07(1, 2) == (0, 0)
    assert t.test_07(a=1, b=2) == (0, 0)
    assert t.test_07(a=1, b=2, c=3) == (0, 1)
    assert t.test_07(1, 2, 3, c=4) == (1, 1)
    assert t.test_07(1, 2, 3, 4, c=5, d=5) == (2, 2)


def test09_maketuple():
    assert t.test_tuple() == ("Hello", 123)
    with pytest.raises(RuntimeError) as excinfo:
        assert t.test_bad_tuple()
    assert str(excinfo.value) == (
        "nanobind::detail::tuple_check(...): conversion of argument 2 failed!")


def test10_cpp_call_simple():
    result = []
    def my_callable(a, b):
        result.append((a, b))

    t.test_call_2(my_callable)
    assert result == [(1, 2)]

    with pytest.raises(TypeError) as excinfo:
        t.test_call_1(my_callable)
    assert "my_callable() missing 1 required positional argument: 'b'" in str(excinfo.value)
    assert result == [(1, 2)]


def test11_call_complex():
    result = []
    def my_callable(*args, **kwargs):
        result.append((args, kwargs))

    t.test_call_extra(my_callable)
    assert result == [
        ((1, 2), {"extra" : 5})
    ]

    result.clear()
    t.test_call_extra(my_callable, 5, 6, hello="world")
    assert result == [
      ((1, 2, 5, 6), {"extra" : 5, "hello": "world"})
    ]


def test12_list_tuple_manipulation():
    li = [1, 5, 6, 7]
    t.test_list(li)
    assert li == [1, 5, 123, 7, 19]

    tu = (1, 5, 6, 7)
    assert t.test_tuple(tu) == 19
    assert tu == (1, 5, 6, 7)
