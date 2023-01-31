import test_eigen_ext as t
import pytest
import gc

try:
    import numpy as np
    def needs_numpy(x):
        return x
except:
    needs_numpy = pytest.mark.skip(reason="NumPy is required")

@needs_numpy
def test01_vector_fixed():
    a  = np.array([1, 2, 3],    dtype=np.int32)
    b  = np.array([0, 1, 2],    dtype=np.int32)
    c  = np.array([1, 3, 5],    dtype=np.int32)
    x  = np.array([1, 3, 5, 6], dtype=np.int32)
    af = np.float32(a)
    bf = np.float32(b)

    assert np.all(t.addV3i_1(a, b) == c)
    assert np.all(t.addV3i_2(a, b) == c)
    assert np.all(t.addV3i_3(a, b) == c)
    assert np.all(t.addV3i_4(a, b) == c)
    assert np.all(t.addV3i_5(a, b) == c)

    # Implicit conversion supported for first argument
    assert np.all(t.addV3i_1(af, b) == c)
    assert np.all(t.addV3i_2(af, b) == c)
    assert np.all(t.addV3i_3(af, b) == c)
    assert np.all(t.addV3i_4(af, b) == c)

    # But not the second one
    with pytest.raises(TypeError) as e:
        t.addV3i_1(a, bf)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_2(a, bf)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_3(a, bf)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_4(a, bf)
    assert 'incompatible function arguments' in str(e)

    # Catch size errors
    with pytest.raises(TypeError) as e:
        t.addV3i_1(x, b)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_2(x, b)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_3(x, b)
    assert 'incompatible function arguments' in str(e)
    with pytest.raises(TypeError) as e:
        t.addV3i_4(x, b)
    assert 'incompatible function arguments' in str(e)


@needs_numpy
def test02_vector_dynamic():
    a  = np.array([1, 2, 3],    dtype=np.int32)
    b  = np.array([0, 1, 2],    dtype=np.int32)
    c  = np.array([1, 3, 5],    dtype=np.int32)
    x  = np.arange(10000, dtype=np.int32)
    af = np.float32(a)

    # Check call with dynamically sized arrays
    assert np.all(t.addVXi(a, b) == c)

    # Implicit conversion
    assert np.all(t.addVXi(af, b) == c)

    # Try with a big array. This will move the result to avoid a copy
    r = np.all(t.addVXi(x, x) == 2*x)


@needs_numpy
def test03_update_map():
    a = np.array([1, 2, 3], dtype=np.int32)
    b = np.array([1, 2, 123], dtype=np.int32)
    c = a.copy()
    t.updateV3i(c)
    assert np.all(c == b)

    c = a.copy()
    t.updateVXi(c)
    assert np.all(c == b)


@needs_numpy
def test04_matrix():
    A = np.vander((1, 2, 3, 4,))
    At = A.T
    A2 = 2*A
    At2 = 2*At
    assert A.flags['C_CONTIGUOUS']
    assert At.flags['F_CONTIGUOUS']
    assert np.all(t.addM4u_1(A, A) == A2)
    assert np.all(t.addM4u_1(At, At) == At2)
    assert np.all(t.addM4u_2(A, A) == A2)
    assert np.all(t.addM4u_2(At, At) == At2)
    assert np.all(t.addM4u_3(A, A) == A2)
    assert np.all(t.addM4u_3(At, At) == At2)
    assert np.all(t.addM4u_4(A, A) == A2)
    assert np.all(t.addM4u_4(At, At) == At2)
    assert np.all(t.addMXu_1(A, A) == A2)
    assert np.all(t.addMXu_1(At, At) == At2)
    assert np.all(t.addMXu_2(A, A) == A2)
    assert np.all(t.addMXu_2(At, At) == At2)
    assert np.all(t.addMXu_3(A, A) == A2)
    assert np.all(t.addMXu_3(At, At) == At2)
    assert np.all(t.addMXu_4(A, A) == A2)
    assert np.all(t.addMXu_4(At, At) == At2)


@needs_numpy
@pytest.mark.parametrize("start", (0, 10))
def test05_matrix_large_nonsymm(start):
    A = np.uint32(np.vander(np.arange(80)))
    A = A[:, start:]
    A2 = A+A
    out = t.addMXu_1(A, A)
    assert np.all(t.addMXu_1(A, A) == A2)
    assert np.all(t.addMXu_2(A, A) == A2)
    assert np.all(t.addMXu_3(A, A) == A2)
    assert np.all(t.addMXu_4(A, A) == A2)

    A = np.ascontiguousarray(A)
    assert A.flags['C_CONTIGUOUS']
    assert np.all(t.addMXu_2_nc(A, A) == A2)

    A = np.asfortranarray(A)
    assert A.flags['F_CONTIGUOUS']
    assert np.all(t.addMXu_1_nc(A, A) == A2)

    A = A.T
    A2 = A2.T
    assert np.all(t.addMXu_1(A, A) == A2)
    assert np.all(t.addMXu_2(A, A) == A2)
    assert np.all(t.addMXu_3(A, A) == A2)
    assert np.all(t.addMXu_4(A, A) == A2)


@needs_numpy
def test06_map():
    b = t.Buffer()
    m = b.map()
    dm = b.dmap()
    for i in range(10):
        for j in range(3):
            m[i, j] = i*3+j
    for i in range(10):
        for j in range(3):
            assert dm[i, j] == i*3+j
    del dm
    del b
    gc.collect()
    gc.collect()
    for i in range(10):
        for j in range(3):
            assert m[i, j] == i*3+j

