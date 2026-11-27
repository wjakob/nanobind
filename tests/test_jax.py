import test_ndarray_ext as t
import test_jax_ext as tj
import pytest
import warnings
import importlib
from common import collect

try:
    import jax.numpy as jnp
    def needs_jax(x):
        return x
except:
    needs_jax = pytest.mark.skip(reason="JAX is required")


@needs_jax
def test01_constrain_order():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        try:
            c = jnp.zeros((3, 5))
        except:
            pytest.skip('jax is missing')

    z = jnp.zeros((3, 5, 4, 6))
    assert t.check_order(z) == 'C'


@needs_jax
def test02_implicit_conversion():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        try:
            c = jnp.zeros((3, 5))
        except:
            pytest.skip('jax is missing')

    t.implicit(jnp.zeros((2, 2), dtype=jnp.int32))
    t.implicit(jnp.zeros((2, 2, 10), dtype=jnp.float32)[:, :, 4])
    t.implicit(jnp.zeros((2, 2, 10), dtype=jnp.int32)[:, :, 4])
    t.implicit(jnp.zeros((2, 2, 10), dtype=jnp.bool_)[:, :, 4])

    with pytest.raises(TypeError) as excinfo:
        t.noimplicit(jnp.zeros((2, 2), dtype=jnp.int32))

    with pytest.raises(TypeError) as excinfo:
        t.noimplicit(jnp.zeros((2, 2), dtype=jnp.uint8))


@needs_jax
def test03_return_jax():
    collect()
    dc = tj.destruct_count()
    x = tj.ret_jax()
    assert x.shape == (2, 4)
    assert jnp.all(x == jnp.array([[1,2,3,4], [5,6,7,8]], dtype=jnp.float32))
    del x
    collect()
    assert tj.destruct_count() - dc == 1


@needs_jax
def test04_check():
    assert t.check(jnp.zeros((1)))


@needs_jax
def test05_passthrough():
    a = tj.ret_jax()
    b = t.passthrough(a)
    assert a is b

    a = jnp.array([1, 2, 3])
    b = t.passthrough(a)
    assert a is b

    a = None
    with pytest.raises(TypeError) as excinfo:
        b = t.passthrough(a)
    assert 'incompatible function arguments' in str(excinfo.value)
    b = t.passthrough_arg_none(a)
    assert a is b


@needs_jax
def test06_ro_array():
    if (not hasattr(jnp, '__array_api_version__')
        or jnp.__array_api_version__ < '2024'):
        pytest.skip('jax version is too old')
    a = jnp.array([1, 2], dtype=jnp.float32)  # JAX arrays are immutable.
    assert t.accept_ro(a) == 1
    # If the next line fails, delete it, update the array_api_version above,
    # and uncomment the three lines below.
    assert t.accept_rw(a) == 1
    # with pytest.raises(TypeError) as excinfo:
    #     t.accept_rw(a)
    # assert 'incompatible function arguments' in str(excinfo.value)


@needs_jax
def test07_reference_does_not_copy():
    import numpy as np

    storage = np.arange(32, dtype=np.float32)
    start = (-storage.ctypes.data % 64) // storage.itemsize
    aligned = storage[start:start + 4]
    assert np.shares_memory(np.asarray(tj.reference_jax(aligned)), aligned)
    assert tj.reference_jax(aligned[:0]).shape == (0,)

    # Some JAX versions can share an unaligned CPU buffer; others must refuse it.
    unaligned = storage[start + 1:start + 5]
    try:
        result = tj.reference_jax(unaligned)
    except ValueError as exc:
        assert "copy" in str(exc).lower()
    else:
        assert np.shares_memory(np.asarray(result), unaligned)
