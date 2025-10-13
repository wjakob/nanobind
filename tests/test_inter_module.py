import test_inter_module_1_ext as t1
import test_inter_module_2_ext as t2
import test_classes_ext as t3
import pytest
from common import xfail_on_pypy_darwin

try:
    from concurrent import interpreters  # Added in Python 3.14
    def needs_interpreters(x):
        return x
except:
    needs_interpreters = pytest.mark.skip(reason="interpreters required")


@xfail_on_pypy_darwin
def test01_inter_module():
    s = t1.create_shared()
    assert t2.check_shared(s, 123)
    t2.increment_shared(s)
    assert t2.check_shared(s, 124)
    with pytest.raises(TypeError) as excinfo:
        assert t3.check_shared(s)
    assert 'incompatible function arguments' in str(excinfo.value)


@xfail_on_pypy_darwin
def test02_reload_module():
    s1 = t1.create_shared()
    s2 = t1.create_shared()
    assert s2 is not s1
    assert type(s2) is type(s1)
    t2.increment_shared(s2)
    import importlib
    new_t1 = importlib.reload(t1)
    assert new_t1 is t1
    s3 = new_t1.create_shared()
    assert type(s3) is type(s1)
    new_t2 = importlib.reload(t2)
    assert new_t2 is t2
    s4 = new_t1.create_shared()
    assert type(s4) is type(s1)
    assert new_t2.check_shared(s2, 124)


@xfail_on_pypy_darwin
def test03_reimport_module():
    s1 = t1.create_shared()
    s2 = t1.create_shared()
    t2.increment_shared(s2)
    import sys
    del sys.modules['test_inter_module_1_ext']
    import test_inter_module_1_ext as new_t1
    assert new_t1 is not t1
    s3 = new_t1.create_shared()
    assert type(s3) is type(s1)
    del sys.modules['test_inter_module_2_ext']
    with pytest.warns(RuntimeWarning, match="'Shared' was already registered"):
        import test_inter_module_2_ext as new_t2
    assert new_t2 is not t2
    s4 = new_t1.create_shared()
    assert type(s4) is type(s1)
    assert new_t2.check_shared(s2, 124)


def run():
    import sys
    if 'tests' not in sys.path[0]:
        import os
        builddir = sys.path[0]
        sys.path.insert(0, os.path.join(builddir, 'tests', 'Release'))
        sys.path.insert(0, os.path.join(builddir, 'tests', 'Debug'))
        sys.path.insert(0, os.path.join(builddir, 'tests'))
    import test_inter_module_1_ext as new_t1
    import test_inter_module_2_ext as new_t2
    success = True
    s = new_t1.create_shared()
    success &= new_t2.check_shared(s, 123)
    new_t2.increment_shared(s)
    success &= new_t2.check_shared(s, 124)
    return success

@needs_interpreters
def test04_subinterpreters():
    assert run()
    interp = interpreters.create()
    with pytest.raises(interpreters.ExecutionFailed) as excinfo:
        assert interp.call(run)
    assert 'does not support loading in subinterpreters' in str(excinfo.value)
    interp.close()
