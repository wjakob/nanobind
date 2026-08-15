import pytest

try:
    import numpy as np
    from numpy.testing import assert_array_equal, assert_array_almost_equal
    import test_xtensor_ext as t
    def needs_numpy_and_xtensor(x):
        return x
except:
    needs_numpy_and_xtensor = pytest.mark.skip(reason="NumPy and xtensor are required")

# xt::xarray tests

@needs_numpy_and_xtensor
def test_xarray():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_almost_equal(t.test_xarray(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_accepts_2d():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_almost_equal(t.test_xarray(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_not_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::-2]
    assert_array_almost_equal(t.test_xarray(b, 2.0, 3.0), np.sin(b) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_column_major():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_mutate():
    a = np.array([1.0, 2.0, 3.0])
    b = t.test_xarray_mutate(a)
    assert_array_equal(b, [999.0, 2.0, 3.0])
    assert_array_equal(a, [1.0, 2.0, 3.0])

@needs_numpy_and_xtensor
def test_xarray_return_by_value():
    result = t.test_xarray_return_by_value()
    assert_array_equal(result, [1.0, 2.0, 3.0])

@needs_numpy_and_xtensor
def test_xarray_return_by_ref():
    result = t.test_xarray_return_by_ref()
    assert_array_equal(result, [10.0, 20.0, 30.0])

@needs_numpy_and_xtensor
def test_xarray_return_by_const_ref():
    result = t.test_xarray_return_by_const_ref()
    assert_array_equal(result, [10.0, 20.0, 30.0])

@needs_numpy_and_xtensor
def test_xarray_accept_column_major():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_equal(t.test_xarray_accept_column_major(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_accept_custom_allocator():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_equal(t.test_xarray_accept_custom_allocator(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_dynamic_type():
    a = np.array([1.0, 2.0, 3.0], dtype = np.float32)
    b = np.array([4.0, 5.0, 6.0], dtype = np.float64)
    assert_array_equal(t.test_xarray_dynamic_type(a), a * 2.0)
    assert_array_equal(t.test_xarray_dynamic_type(b), b * 2.0)

@needs_numpy_and_xtensor
def test_xarray_type_overload():
    a = np.array([1.0, 2.0, 3.0], dtype = np.float64)
    b = np.array([4.0, 5.0, 6.0], dtype = np.float32)
    assert_array_equal(t.test_xarray_type_overload(a), 2.0 * a)
    assert_array_equal(t.test_xarray_type_overload(b), 3.0 * b)

@needs_numpy_and_xtensor
def test_xarray_template_func():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_equal(t.test_xarray_template_func(a), a * 1234.0)

@needs_numpy_and_xtensor
def test_xarray_complex():
    a = np.array([1 + 2j, 3 + 4j])
    b = np.array([5 + 6j, 7 + 8j])
    assert_array_equal(t.test_xarray_complex(a, b), a + b)

@needs_numpy_and_xtensor
def test_xarray_row_major_noconvert_accepts_column_major():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0, 4.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_equal(t.test_xarray_row_major_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_row_major_noconvert_rejects_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::2]
    try:
        t.test_xarray_row_major_noconvert(b)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xarray_column_major_noconvert_accepts_row_major():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_equal(t.test_xarray_column_major_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_column_major_noconvert_rejects_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::2]
    try:
        t.test_xarray_column_major_noconvert(b)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xarray_dynamic_noconvert_accepts_row_major():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    assert_array_equal(t.test_xarray_dynamic_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_dynamic_noconvert_accepts_column_major():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0, 4.0]))
    assert_array_equal(t.test_xarray_dynamic_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xarray_dynamic_noconvert_accepts_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::2]
    assert_array_equal(t.test_xarray_dynamic_noconvert(b), b * 2.0)

@needs_numpy_and_xtensor
def test_xarray_return_column_major():
    out = t.test_xarray_return_column_major()
    assert out.flags["F_CONTIGUOUS"]
    assert_array_equal(out, [1.0, 2.0, 3.0, 4.0])

@needs_numpy_and_xtensor
def test_xarray_mixed_layouts():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    assert a.flags["C_CONTIGUOUS"]
    out = t.test_xarray_mixed_layouts(a)
    assert out.flags["F_CONTIGUOUS"]
    assert_array_equal(out, a)


# xt::xtensor tests

@needs_numpy_and_xtensor
def test_xtensor():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_almost_equal(t.test_xtensor(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_wrong_dimension():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    try:
        t.test_xtensor(a, 2.0, 3.0)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_not_contiguous():
    a = np.arange(16.0).reshape(4, 4)
    b = a[::2, ::2]
    assert_array_almost_equal(t.test_xtensor(b, 2.0, 3.0), np.sin(b) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_mutate():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    b = t.test_xtensor_mutate(a)
    assert_array_equal(b, [[999.0, 2.0], [3.0, 4.0]])
    assert_array_equal(a, [[1.0, 2.0], [3.0, 4.0]])

@needs_numpy_and_xtensor
def test_xtensor_return_by_value():
    result = t.test_xtensor_return_by_value()
    assert_array_equal(result, [[1.0, 2.0], [3.0, 4.0]])

@needs_numpy_and_xtensor
def test_xtensor_return_by_ref():
    result = t.test_xtensor_return_by_ref()
    assert_array_equal(result, [[10.0, 20.0], [30.0, 40.0]])

@needs_numpy_and_xtensor
def test_xtensor_return_by_const_ref():
    result = t.test_xtensor_return_by_const_ref()
    assert_array_equal(result, [[10.0, 20.0], [30.0, 40.0]])

@needs_numpy_and_xtensor
def test_xtensor_accept_column_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_equal(t.test_xtensor_accept_column_major(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_accept_custom_allocator():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_equal(t.test_xtensor_accept_custom_allocator(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_dynamic_type():
    a = np.array([[1.0, 2.0], [3.0, 4.0]], dtype = np.float32)
    b = np.array([[5.0, 6.0], [7.0, 8.0]], dtype = np.float64)
    assert_array_equal(t.test_xtensor_dynamic_type(a), a * 2.0)
    assert_array_equal(t.test_xtensor_dynamic_type(b), b * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_type_overload():
    a = np.array([[1.0, 2.0], [3.0, 4.0]], dtype = np.float64)
    b = np.array([[5.0, 6.0], [7.0, 8.0]], dtype = np.float32)
    assert_array_equal(t.test_xtensor_type_overload(a), 2.0 * a)
    assert_array_equal(t.test_xtensor_type_overload(b), 3.0 * b)

@needs_numpy_and_xtensor
def test_xtensor_template_func():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_equal(t.test_xtensor_template_func(a), a * 1234.0)

@needs_numpy_and_xtensor
def test_xtensor_complex():
    a = np.array([[1 + 1j, 2 + 2j], [3 + 3j, 4 + 4j]])
    b = np.array([[5 + 5j, 6 + 6j], [7 + 7j, 8 + 8j]])
    assert_array_equal(t.test_xtensor_complex(a, b), a + b)

@needs_numpy_and_xtensor
def test_xtensor_row_major_noconvert_1d():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0, 4.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_equal(t.test_xtensor_row_major_noconvert_1d(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_column_major_noconvert_1d():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_equal(t.test_xtensor_column_major_noconvert_1d(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_row_major_noconvert_rejects_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    try:
        t.test_xtensor_row_major_noconvert(a)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_row_major_noconvert_rejects_non_contiguous():
    a = np.arange(16.0).reshape(4, 4)
    b = a[::2, ::2]
    try:
        t.test_xtensor_row_major_noconvert(b)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_column_major_noconvert_rejects_row_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert a.flags["C_CONTIGUOUS"]
    try:
        t.test_xtensor_column_major_noconvert(a)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_column_major_noconvert_rejects_non_contiguous():
    a = np.arange(16.0).reshape(4, 4)
    b = a[::2, ::2]
    try:
        t.test_xtensor_column_major_noconvert(b)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_dynamic_noconvert_accepts_row_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_equal(t.test_xtensor_dynamic_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_dynamic_noconvert_accepts_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert_array_equal(t.test_xtensor_dynamic_noconvert(a), a * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_dynamic_noconvert_accepts_non_contiguous():
    a = np.arange(16.0).reshape(4, 4)
    b = a[::2, ::2]
    assert_array_equal(t.test_xtensor_dynamic_noconvert(b), b * 2.0)

@needs_numpy_and_xtensor
def test_xtensor_return_column_major():
    out = t.test_xtensor_return_column_major()
    assert out.flags["F_CONTIGUOUS"]
    assert_array_equal(out, [[1.0, 2.0], [3.0, 4.0]])

@needs_numpy_and_xtensor
def test_xtensor_mixed_layouts():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert a.flags["C_CONTIGUOUS"]
    out = t.test_xtensor_mixed_layouts(a)
    assert out.flags["F_CONTIGUOUS"]
    assert_array_equal(out, a)

# nb::xarray_view tests

@needs_numpy_and_xtensor
def test_xarray_view():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_almost_equal(t.test_xarray_view(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_accepts_2d():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_almost_equal(t.test_xarray_view(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_row_major_accepts_column_major_1d():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_default_accepts_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::-2]
    assert_array_almost_equal(t.test_xarray_view(b, 2.0, 3.0), np.sin(b) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_row_major_rejects_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::-2]
    try:
        t.test_xarray_view_row_major(b, 2.0, 3.0)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xarray_view_row_major_accepts_row_major():
    a = np.array([1.0, 2.0, 3.0])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view_row_major(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_column_major():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view_column_major(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_column_major_accepts_row_major_1d():
    a = np.array([1.0, 2.0, 3.0])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view_column_major(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_dynamic_row_major():
    a = np.array([1.0, 2.0, 3.0])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view_dynamic(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_dynamic_column_major():
    a = np.asfortranarray(np.array([1.0, 2.0, 3.0]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xarray_view_dynamic(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_dynamic_non_contiguous():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
    b = a[::-2]
    assert_array_almost_equal(t.test_xarray_view_dynamic(b, 2.0, 3.0), np.sin(b) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xarray_view_zerocopy():
    a = np.array([1.0, 2.0, 3.0])
    b = t.test_xarray_view_zerocopy(a)
    a[0] = 1234.0
    assert_array_equal(a, b)
    b[0] = 5678.0
    assert_array_equal(a, b)

@needs_numpy_and_xtensor
def test_xarray_view_mutate():
    a = np.array([1.0, 2.0, 3.0])
    t.test_xarray_view_mutate(a)
    assert_array_equal(a, [999.0, 2.0, 3.0])

@needs_numpy_and_xtensor
def test_xarray_view_strict_type():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
    try:
        t.test_xarray_view_strict_type(a)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xarray_view_type_overload():
    a = np.array([1.0, 2.0, 3.0], dtype=np.float64)
    b = np.array([4.0, 5.0, 6.0], dtype=np.float32)
    assert_array_equal(t.test_xarray_view_type_overload(a), 2.0 * a)
    assert_array_equal(t.test_xarray_view_type_overload(b), 3.0 * b)

@needs_numpy_and_xtensor
def test_xarray_view_template_func():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_equal(t.test_xarray_view_template_func(a), a * 1234.0)

@needs_numpy_and_xtensor
def test_xarray_view_complex():
    a = np.array([1 + 1j, 2 + 2j, 3 + 3j])
    assert_array_equal(t.test_xarray_view_complex(a), a + (1 + 1j))


# nb::xtensor_view tests

@needs_numpy_and_xtensor
def test_xtensor_view():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_almost_equal(t.test_xtensor_view(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_wrong_dimension():
    a = np.array([1.0, 2.0, 3.0, 4.0])
    try:
        t.test_xtensor_view(a, 2.0, 3.0)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_view_default_accepts_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor_view(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_row_major_rejects_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    try:
        t.test_xtensor_view_row_major(a, 2.0, 3.0)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_view_row_major_accepts_row_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor_view_row_major(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor_view_column_major(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_column_major_rejects_row_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert a.flags["C_CONTIGUOUS"]
    try:
        t.test_xtensor_view_column_major(a, 2.0, 3.0)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_view_dynamic_row_major():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert a.flags["C_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor_view_dynamic(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_dynamic_column_major():
    a = np.asfortranarray(np.array([[1.0, 2.0], [3.0, 4.0]]))
    assert a.flags["F_CONTIGUOUS"]
    assert_array_almost_equal(t.test_xtensor_view_dynamic(a, 2.0, 3.0), np.sin(a) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_dynamic_non_contiguous():
    a = np.arange(16.0).reshape(4, 4)
    b = a[::2, ::2]
    assert_array_almost_equal(t.test_xtensor_view_dynamic(b, 2.0, 3.0), np.sin(b) * 2.0 + 3.0)

@needs_numpy_and_xtensor
def test_xtensor_view_zerocopy():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    b = t.test_xtensor_view_zerocopy(a)
    a[0, 0] = 1234.0
    assert_array_equal(a, b)
    b[0, 0] = 5678.0
    assert_array_equal(a, b)

@needs_numpy_and_xtensor
def test_xtensor_view_mutate():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    t.test_xtensor_view_mutate(a)
    assert_array_equal(a, [[999.0, 2.0], [3.0, 4.0]])

@needs_numpy_and_xtensor
def test_xtensor_view_strict_type():
    a = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float32)
    try:
        t.test_xtensor_view_strict_type(a)
        assert False, "should raise TypeError"
    except TypeError:
        pass

@needs_numpy_and_xtensor
def test_xtensor_view_type_overload():
    a = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float64)
    b = np.array([[5.0, 6.0], [7.0, 8.0]], dtype=np.float32)
    assert_array_equal(t.test_xtensor_view_type_overload(a), 2.0 * a)
    assert_array_equal(t.test_xtensor_view_type_overload(b), 3.0 * b)

@needs_numpy_and_xtensor
def test_xtensor_view_template_func():
    a = np.array([[1.0, 2.0], [3.0, 4.0]])
    assert_array_equal(t.test_xtensor_view_template_func(a), a * 1234.0)

@needs_numpy_and_xtensor
def test_xtensor_view_complex():
    a = np.array([[1 + 1j, 2 + 2j], [3 + 3j, 4 + 4j]])
    assert_array_equal(t.test_xtensor_view_complex(a), a + (1 + 1j))


# vectorization tests

@needs_numpy_and_xtensor
def test_vectorize():
    a = np.array([1.0, 2.0, 3.0])
    b = np.array([4.0, 5.0, 6.0])
    assert_array_equal(t.test_vectorize(a, b), a + b)

@needs_numpy_and_xtensor
def test_vectorize_lambda():
    a = np.array([1.0, 2.0, 3.0])
    assert_array_almost_equal(t.test_vectorize_lambda(a), np.sin(a))


# strided_view + adaptor tests

@needs_numpy_and_xtensor
def test_strided_view_return():
    out = t.test_strided_view_return()
    assert_array_equal(out, [10.0, 30.0])

@needs_numpy_and_xtensor
def test_xarray_adaptor_return():
    out = t.test_xarray_adaptor_return()
    assert_array_equal(out, [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])

@needs_numpy_and_xtensor
def test_xtensor_adaptor_return():
    out = t.test_xtensor_adaptor_return()
    assert_array_equal(out, [[10.0, 20.0], [30.0, 40.0]])


# reference_internal tests

@needs_numpy_and_xtensor
def test_reference_internal_xarray():
    owner = t.Owner()
    a = owner.get_array()
    assert_array_equal(a, [1.0, 2.0, 3.0])
    a[0] = 999.0
    assert_array_equal(owner.get_array(), [999.0, 2.0, 3.0])

@needs_numpy_and_xtensor
def test_reference_internal_xtensor():
    owner = t.Owner()
    a = owner.get_tensor()
    assert_array_equal(a, [[1.0, 2.0], [3.0, 4.0]])
    a[0, 0] = 999.0
    assert_array_equal(owner.get_tensor(), [[999.0, 2.0], [3.0, 4.0]])
