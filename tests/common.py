import platform
import gc
import pytest
import threading

is_pypy = platform.python_implementation() == 'PyPy'
is_darwin = platform.system() == 'Darwin'

def collect() -> None:
    if is_pypy:
        for _ in range(3):
            gc.collect()
    else:
        gc.collect()

skip_on_pypy = pytest.mark.skipif(
    is_pypy, reason="This test currently fails/crashes PyPy")

xfail_on_pypy_darwin = pytest.mark.xfail(
    is_pypy and is_darwin, reason="This test for some reason fails on PyPy/Darwin")


# Helper function to parallelize execution of a function. We intentionally
# don't use the Python threads pools here to have threads shut down / start
# between test cases.
def parallelize(func, n_threads):
    barrier = threading.Barrier(n_threads)
    result = [None]*n_threads

    def wrapper(i):
        barrier.wait()
        result[i] = func()

    workers = []
    for i in range(n_threads):
        t = threading.Thread(target=wrapper, args=(i,))
        t.start()
        workers.append(t)

    for worker in workers:
        worker.join()
    return result