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
