import test_dlpack_ext as t
import numpy as np
import pytest

def test01_metadata():
    a = np.zeros(shape=())
    assert t.get_shape(a) == []
    b = a.__dlpack__()
    assert t.get_shape(b) == []

    with pytest.raises(TypeError) as excinfo:
        # Capsule can only be consumed once
        assert t.get_shape(b) == []
    assert 'incompatible function arguments' in str(excinfo.value)

    a = np.zeros(shape=(3, 4, 5))
    assert t.get_shape(a) == [3, 4, 5]
    assert t.get_shape(a.__dlpack__()) == [3, 4, 5]
    assert not t.check_float(np.array([1], dtype=np.uint32)) and \
               t.check_float(np.array([1], dtype=np.float32))


def test02_docstr():
    assert t.get_shape.__doc__ == "get_shape(arg: tensor[], /) -> list"
    assert t.pass_uint32.__doc__ == "pass_uint32(arg: tensor[dtype=uint32], /) -> None"
    assert t.pass_float32.__doc__ == "pass_float32(arg: tensor[dtype=float32], /) -> None"
    assert t.pass_float32_shaped.__doc__ == "pass_float32_shaped(arg: tensor[dtype=float32, shape=(3, *, 4)], /) -> None"
    assert t.pass_float32_shaped_ordered.__doc__ == "pass_float32_shaped_ordered(arg: tensor[dtype=float32, order='C', shape=(*, *, 4)], /) -> None"
    assert t.check_device.__doc__ == ("check_device(arg: tensor[device='cpu'], /) -> str\n"
                                      "check_device(arg: tensor[device='cuda'], /) -> str")


def test03_constrain_dtype():
    a_u32 = np.array([1], dtype=np.uint32)
    a_f32 = np.array([1], dtype=np.float32)

    t.pass_uint32(a_u32)
    t.pass_float32(a_f32)

    with pytest.raises(TypeError) as excinfo:
        t.pass_uint32(a_f32)
    assert 'incompatible function arguments' in str(excinfo.value)

    with pytest.raises(TypeError) as excinfo:
        t.pass_float32(a_u32)
    assert 'incompatible function arguments' in str(excinfo.value)

def test04_constrain_shape():
    t.pass_float32_shaped(np.zeros((3, 0, 4), dtype=np.float32))
    t.pass_float32_shaped(np.zeros((3, 5, 4), dtype=np.float32))

    with pytest.raises(TypeError) as excinfo:
        t.pass_float32_shaped(np.zeros((3, 5), dtype=np.float32))

    with pytest.raises(TypeError) as excinfo:
        t.pass_float32_shaped(np.zeros((2, 5, 4), dtype=np.float32))

    with pytest.raises(TypeError) as excinfo:
        t.pass_float32_shaped(np.zeros((3, 5, 6), dtype=np.float32))

    with pytest.raises(TypeError) as excinfo:
        t.pass_float32_shaped(np.zeros((3, 5, 4, 6), dtype=np.float32))


def test04_constrain_order():
    assert t.check_order(np.zeros((3, 5, 4, 6), order='C')) == 'C'
    assert t.check_order(np.zeros((3, 5, 4, 6), order='F')) == 'F'
    assert t.check_order(np.zeros((3, 5, 4, 6), order='C')[:, 2, :, :]) == '?'
    assert t.check_order(np.zeros((3, 5, 4, 6), order='F')[:, 2, :, :]) == '?'
