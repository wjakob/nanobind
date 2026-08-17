import random
import threading

import pytest

import test_thread_ext as t
from test_thread_ext import Counter, GlobalData, ClassWithProperty, ClassWithClassProperty
from common import parallelize

def test01_object_creation(n_threads=8):
    # This test hammers 'inst_c2p' from multiple threads, and
    # checks that the locking of internal data structures works

    n = 100000
    def f():
        r = [None]*n
        for i in range(n):
            c = Counter()
            c.inc_unsafe()
            r[i] = c
        for i in range(n):
            assert t.return_self(r[i]) is r[i]
        return r

    v = parallelize(f, n_threads=n_threads)
    assert len(v) == n_threads
    for v2 in v:
        assert len(v2) == n
        for v3 in v2:
            assert v3.value == 1

def test02_global_lock(n_threads=8):
    # Test that a global PyMutex protects the counter
    n = 100000
    c = Counter()
    def f():
        for _ in range(n):
            t.inc_global(c)

    parallelize(f, n_threads=n_threads)
    assert c.value == n * n_threads


def test03_locked_method(n_threads=8):
    # Checks that nb::lock_self() protects an internal counter
    n = 100000
    c = Counter()
    def f():
        for i in range(n):
            c.inc_safe()

    parallelize(f, n_threads=n_threads)
    assert c.value == n * n_threads


def test04_locked_function(n_threads=8):
    # Checks that nb::lock_self() protects an internal counter
    n = 100000
    c = Counter()
    def f():
        for _ in range(n):
            t.inc_safe(c)

    parallelize(f, n_threads=n_threads)
    assert c.value == n * n_threads


def test05_locked_twoargs(n_threads=8):
    # Check two-argument locking
    n = 100000
    c = Counter()
    def f():
        c2 = Counter()
        for i in range(n):
            c2.inc_unsafe()
            if i & 1 == 0:
                c2.merge_safe(c)
            else:
                c.merge_safe(c2)

    parallelize(f, n_threads=n_threads)
    assert c.value == n * n_threads


def test06_global_wrapper(n_threads=8):
    # Check wrapper lookup racing with wrapper deallocation
    n = 10000
    def f():
        for _ in range(n):
            GlobalData.get()
            GlobalData.get()
            GlobalData.get()
            GlobalData.get()

    parallelize(f, n_threads=n_threads)


def test07_access_attributes(n_threads=8):
    n = 1000
    c1 = ClassWithProperty(123)
    c2 = ClassWithClassProperty(c1)

    def f():
        for i in range(n):
            _ = c2.prop1.prop2

    parallelize(f, n_threads=n_threads)


def test08_shared_ptr_threaded_access(n_threads=8):
    # Test for keep_alive racing with other fields.
    def f(barrier):
        i = random.randint(0, 4)
        barrier.wait()
        p = t.fetch_shared_int(i)
        assert t.consume_an_int(p) == i

    for _ in range(100):
        barrier = threading.Barrier(n_threads)
        parallelize(lambda: f(barrier), n_threads=n_threads)


def test09_bind_vector(n_threads=8):
    # Hammer a shared nb::bind_vector() container. The locking annotations in
    # bind_vector.h must keep the underlying std::vector consistent.
    n = 10000
    v = t.IntVector()
    v.append(0)

    def f():
        for i in range(n):
            v.append(i)
            v[0] = i
            v.count(i)
            _ = i in v
            _ = len(v)

    parallelize(f, n_threads=n_threads)
    assert len(v) == n * n_threads + 1


def test10_bind_vector_two_locks(n_threads=8):
    # Exercise the two-argument locking of 'extend' and slice assignment
    n = 200
    dst = t.IntVector()

    def f():
        src = t.IntVector()
        for i in range(n):
            src.append(i)
            dst.extend(src)
            src[:] = src

    parallelize(f, n_threads=n_threads)
    assert len(dst) == n_threads * n * (n + 1) // 2


def test11_bind_map(n_threads=8):
    # Hammer a shared nb::bind_map() container, including its views
    n = 2000
    m = t.StringIntMap()

    def f():
        for i in range(n):
            k = str(i)
            m[k] = i
            _ = k in m
            _ = len(m)
            _ = len(m.keys())
            _ = k in m.keys()

    parallelize(f, n_threads=n_threads)
    assert len(m) == n


def test12_bind_vector_iter_realloc():
    # The iterator refers to its position by index and re-derives the element
    # on every step, so growing the vector cannot invalidate it. An iterator
    # pair would refer to the buffer abandoned by the first reallocation.
    v = t.IntVector()
    for i in range(8):
        v.append(i)

    seen = 0
    for x in v:
        seen += 1
        if len(v) < 4096:
            v.append(x)

    assert seen == 4096 and len(v) == 4096

    # Like a list iterator, an exhausted iterator stays exhausted even if the
    # vector grows afterwards
    it = iter(v)
    for _ in it:
        pass
    v.append(0)
    with pytest.raises(StopIteration):
        next(it)


def test13_bind_vector_iter_threaded(n_threads=8):
    # The same hazard, reached from another thread: iterate while the vector
    # grows underneath, crossing many reallocations
    n = 2000
    v = t.IntVector()
    v.append(0)

    def f():
        for i in range(n):
            v.append(i)
            if (i & 15) == 0:
                for _ in v:
                    pass

    parallelize(f, n_threads=n_threads)
    assert len(v) == n * n_threads + 1


def test14_bind_map_two_locks(n_threads=8):
    # Exercise the two-argument locking of 'update'
    n = 200
    dst = t.StringIntMap()

    def f():
        src = t.StringIntMap()
        for i in range(n):
            src[str(i)] = i
            dst.update(src)

    parallelize(f, n_threads=n_threads)
    assert len(dst) == n


def test15_bind_map_iter_threaded(n_threads=8):
    # Iterate while other threads insert. Each step re-derives the position
    # by key under the map's lock, and either succeeds or raises RuntimeError
    # when it detects the modification. It must never crash.
    n = 2000
    m = t.StringIntMap()

    def f():
        for i in range(n):
            m[str(i)] = i
            if (i & 15) == 0:
                try:
                    for _ in m:
                        pass
                except RuntimeError:
                    pass

    parallelize(f, n_threads=n_threads)
    assert len(m) == n
