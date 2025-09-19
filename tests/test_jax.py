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
def test01_constrain_order_jax():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        try:
            c = jnp.zeros((3, 5))
        except:
            pytest.skip('jax is missing')

    z = jnp.zeros((3, 5, 4, 6))
    assert t.check_order(z) == 'C'


@needs_jax
def test02_implicit_conversion_jax():
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
def test04_check_jax():
    assert t.check(jnp.zeros((1)))
