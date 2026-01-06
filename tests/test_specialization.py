import sys
import sysconfig
import dis
import pytest

# Note: these tests verify that CPython's adaptive specializing interpreter can
# optimize various expressions involving nanobind types. They are expected to
# be somewhat fragile across Python versions as the bytecode and specialization
# opcodes may change.

# Skip tests on PyPy and free-threaded Python
skip_tests = sys.implementation.name == "pypy" or \
    sysconfig.get_config_var("Py_GIL_DISABLED")

import test_classes_ext as t
def disasm(func):
    """Extract specialized opcode names from a function"""
    instructions = list(dis.get_instructions(func, adaptive=True))
    return [(instr.opname, instr.argval) for instr in instructions]

def warmup(fn):
    # Call the function a few times to ensure that it is specialized
    for _ in range(8):
        fn()

def count_op(ops, expected):
    hits = 0
    for opname, _ in ops:
        if opname == expected:
            hits += 1
    return hits

@pytest.mark.skipif(
    sys.version_info < (3, 14) or skip_tests,
    reason="Static attribute specialization requires CPython 3.14+")
def test_static_attribute_specialization():
    s = t.Struct
    def fn():
        return s.static_test

    ops = disasm(fn)
    print(ops)
    op_base = count_op(ops, "LOAD_ATTR")
    op_opt = (
        count_op(ops, "LOAD_ATTR_ADAPTIVE") +
        count_op(ops, "LOAD_ATTR_CLASS"))
    assert op_base == 1 and op_opt == 0

    warmup(fn)
    ops = disasm(fn)
    print(ops)

    op_base = count_op(ops, "LOAD_ATTR")
    op_opt = (
        count_op(ops, "LOAD_ATTR_ADAPTIVE") +
        count_op(ops, "LOAD_ATTR_CLASS"))
    assert op_base == 0 and op_opt == 1

@pytest.mark.skipif(
    sys.version_info < (3, 11) or skip_tests,
    reason="Method call specialization requires CPython 3.14+")
def test_method_call_specialization():
    s = t.Struct()
    def fn():
        return s.value()

    ops = disasm(fn)
    op_base = (
        count_op(ops, "LOAD_METHOD") +
        count_op(ops, "LOAD_ATTR"))
    op_opt = (
        count_op(ops, "LOAD_ATTR_METHOD_NO_DICT") +
        count_op(ops, "CALL_ADAPTIVE"))
    print(ops)
    assert op_base == 1 and op_opt == 0

    warmup(fn)
    ops = disasm(fn)
    print(ops)
    op_base = (
        count_op(ops, "LOAD_METHOD") +
        count_op(ops, "LOAD_ATTR"))
    op_opt = (
        count_op(ops, "LOAD_ATTR_METHOD_NO_DICT") +
        count_op(ops, "CALL_ADAPTIVE"))
    assert op_base == 0 and op_opt == 1


@pytest.mark.skipif(sys.version_info < (3, 11) or skip_tests,
    reason="Immutability requires Python 3.11+")
def test_immutability():
    # Test nb_method immutability
    method = t.Struct.value
    method_type = type(method)
    assert method_type.__name__ == "nb_method"
    with pytest.raises(TypeError, match="immutable"):
        method_type.test_attr = 123

    # Test metaclass immutability
    metaclass = type(t.Struct)
    assert metaclass.__name__.startswith("nb_type")
    with pytest.raises(TypeError, match="immutable"):
        metaclass.test_attr = 123
