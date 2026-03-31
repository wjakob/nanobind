import pytest

try:
    import numpy as np
    from numpy.testing import assert_array_equal
    import test_eigen_tensor_ext as t
    HAS_NUMPY_AND_EIGEN = True
except ImportError:
    HAS_NUMPY_AND_EIGEN = False

needs_numpy_and_eigen = pytest.mark.skipif(
    not HAS_NUMPY_AND_EIGEN,
    reason="NumPy and Eigen are required")


@needs_numpy_and_eigen
def test01_tensor3d():
    a = np.arange(0, 12, dtype=float).reshape(2, 3, 2)
    assert_array_equal(t.square3dTensorR(a), a ** 2)
    a_colmaj = np.asfortranarray(a)
    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.square3dTensorR(a_colmaj)

    # add3dTensor: col-major inputs (with implicit conversion from row-major)
    b = np.arange(12, 24, dtype=float).reshape(2, 3, 2)
    assert_array_equal(t.add3dTensor(a, b), a + b)
    # row-major numpy arrays are implicitly converted
    assert_array_equal(t.add3dTensor(a, a), a + a)

    # add3dTensor_nc: noconvert — rejects row-major (C-order) arrays
    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.add3dTensor_nc(a, b)
    # Fortran-order (col-major) arrays are accepted without conversion
    assert_array_equal(t.add3dTensor_nc(a_colmaj, np.asfortranarray(b)), a + b)

    # mul3dTensor: scalar * col-major tensor
    assert_array_equal(t.mul3dTensor(2.0, a), 2.0 * a)

    # mul3dTensorMap: read-only TensorMap, returns a new tensor
    assert_array_equal(t.mul3dTensorMap(3.0, a_colmaj), 3.0 * a)

    # mul3dTensorMapInPlace: mutates the array in place via TensorMap
    arr = np.asfortranarray(a.copy())
    t.mul3dTensorMapInPlace(2.0, arr)
    assert_array_equal(arr, 2.0 * a)


@needs_numpy_and_eigen
def test02_update_tensorref():
    a = np.arange(0, 12, dtype=float).reshape(2, 3, 2)

    arr = np.asfortranarray(a.copy())
    t.update3dTensorRef(arr)
    assert arr[0, 0, 0] == 42.0

    # wrong scalar type is rejected
    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.update3dTensorRef(np.asfortranarray(a).astype(np.int32))
