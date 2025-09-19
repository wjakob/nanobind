import test_ndarray_ext as t
import test_tensorflow_ext as ttf
import pytest
import warnings
import importlib
from common import collect

try:
    import tensorflow as tf
    import tensorflow.config
    def needs_tensorflow(x):
        return x
except:
    needs_tensorflow = pytest.mark.skip(reason="TensorFlow is required")


@needs_tensorflow
def test01_constrain_order_tensorflow():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        try:
            c = tf.zeros((3, 5))
        except:
            pytest.skip('tensorflow is missing')

    assert t.check_order(c) == 'C'


@needs_tensorflow
def test02_implicit_conversion_tensorflow():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        try:
            c = tf.zeros((3, 5))
        except:
            pytest.skip('tensorflow is missing')

        t.implicit(tf.zeros((2, 2), dtype=tf.int32))
        t.implicit(tf.zeros((2, 2, 10), dtype=tf.float32)[:, :, 4])
        t.implicit(tf.zeros((2, 2, 10), dtype=tf.int32)[:, :, 4])
        t.implicit(tf.zeros((2, 2, 10), dtype=tf.bool)[:, :, 4])

        with pytest.raises(TypeError) as excinfo:
            t.noimplicit(tf.zeros((2, 2), dtype=tf.int32))

        with pytest.raises(TypeError) as excinfo:
            t.noimplicit(tf.zeros((2, 2), dtype=tf.bool))


@needs_tensorflow
def test03_return_tensorflow():
    collect()
    dc = ttf.destruct_count()
    x = ttf.ret_tensorflow()
    assert x.get_shape().as_list() == [2, 4]
    assert tf.math.reduce_all(
               x == tf.constant([[1,2,3,4], [5,6,7,8]], dtype=tf.float32))
    del x
    collect()
    assert ttf.destruct_count() - dc == 1


@needs_tensorflow
def test04_check_tensorflow():
    assert t.check(tf.zeros((1)))
