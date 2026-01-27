import numpy as np
from numpy.testing import assert_array_equal, assert_array_almost_equal

import test_xtensor_ext as t


def test_add():
    a = np.array([1.0, 2.0, 3.0])
    b = np.array([4.0, 5.0, 6.0])
    assert_array_equal(t.test_add(a, b), a + b)


def test_funcs():
    a = np.array([1.0, 2.0])
    b = np.array([3.0, 4.0])
    result = t.test_funcs(a, b)
    expected = np.sin(a) + np.cos(b)
    assert_array_almost_equal(result, expected, decimal=10)


def test_scalar():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_equal(t.test_scalar(a, 2.0, 3.0), a * 2.0 + 3.0)
