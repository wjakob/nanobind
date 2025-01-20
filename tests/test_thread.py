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
