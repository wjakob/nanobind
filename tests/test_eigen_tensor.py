import pytest
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

    # try non-contiguous
    c = a[:, 1:, :]
    d = a[:, :-1, :]
    assert_array_equal(t.add3dTensor(c, d), c + d)
    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.add3dTensor_nc(c, d)

    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.add3dTensorCnstMap(c, d)

    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.update3dTensorRef(c)


@needs_numpy_and_eigen
def test02_update_tensorref():
    a = np.arange(0, 12, dtype=float).reshape(2, 3, 2)

    arr = np.asfortranarray(a.copy())
    t.update3dTensorRef(arr)
    assert arr[0, 0, 0] == 42.0

    # wrong scalar type is rejected
    with pytest.raises(TypeError, match='incompatible function arguments'):
        t.update3dTensorRef(np.asfortranarray(a).astype(np.int32))

@needs_numpy_and_eigen
def test03_prop():
    for j in range(3):
        c = t.ClassWithEigenMember()
        ref = np.ones((2, 1, 2))
        if j == 0:
            c.member = ref

        for i in range(2):
            member = c.member
            if j == 2 and i == 0:
                member[0, 0, 0] = 10
                ref[0, 0, 0] = 10
            assert_array_equal(member, ref)
            del member

        member = c.member
        assert_array_equal(c.member_ro_ref, ref)
        assert_array_equal(c.member_ro_copy, ref)
        del c
        assert_array_equal(member, ref)

@needs_numpy_and_eigen
def test04_map():
    b = t.Buffer()
    m = b.map()
    for i in range(2):
        for j in range(3):
            for k in range(3):
                m[i, j, k] = i*3*3+j*3+k
    del b
    for i in range(2):
        for j in range(3):
            for k in range(3):
                m[i, j, k] = i*3*3+j*3+k

@needs_numpy_and_eigen
def test05_cast():
    a = np.arange(12, dtype=np.int32).reshape(2, 2, 3, order='F')
    assert_array_equal(t.castTo3iTensorMap(a), a)
    assert_array_equal(t.castTo3iTensorMapAligned(a), a)


@needs_numpy_and_eigen
def test06_zero_size_tensor():
    a = np.ones((0, 2, 4), dtype=np.float64, order='F')
    b = np.ones((0, 2, 4), dtype=np.float64, order='F')
    assert_array_equal(t.add3dTensorCnstMap(a, b), a + b)

    c= np.ones((0, 2, 4), dtype=np.int32, order='F')
    c_map = t.castTo3iTensorMap(c)
    assert_array_equal(c_map, c)
    assert not c_map.flags.owndata
    assert c_map.flags.writeable

    c_map_const = t.castTo3iTensorMapCnst(c)
    assert_array_equal(c_map_const, c)
    assert not c_map_const.flags.owndata
    assert not c_map_const.flags.writeable
    assert_array_equal(t.castTo3iTensorMapAligned(c), c)

    # Pretty much a scalar
    d = np.ones((), order='F')
    d_cast = t.castTo0dTensorMap(d)
    assert_array_equal(d_cast, d)
    assert not d_cast.flags.owndata
