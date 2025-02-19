import os
import pathlib
import difflib
import sys
import platform
import pytest

is_unsupported = platform.python_implementation() == 'PyPy' or sys.version_info < (3, 10)
skip_on_unsupported = pytest.mark.skipif(
    is_unsupported, reason="Stub generation is only tested on CPython >= 3.10.0")

def remove_platform_dependent(s):
    '''Remove platform-dependent functions from the stubs'''
    s2 = []
    i = 0
    while i < len(s):
        v = s[i]
        if v.startswith('def ret_numpy_half()') or \
           v.startswith('def test_slots()') or \
           v.startswith('TypeAlias'):
            i += 2
        else:
            s2.append(v)
            i += 1
    return s2


ref_paths = list(pathlib.Path(__file__).parent.glob('*.pyi.ref'))
ref_path_ids = [p.name[:-len('.pyi.ref')] for p in ref_paths]
assert len(ref_paths) > 0, "Stub reference files not found!"

@skip_on_unsupported
@pytest.mark.parametrize('p_ref', ref_paths, ids=ref_path_ids)
def test01_check_stub_refs(p_ref, request):
    """
    Check that generated stub files match reference input
    """
    if not request.config.getoption('enable-slow-tests') and any(
        (x in p_ref.name for x in ['jax', 'tensorflow'])):
        pytest.skip("skipping because slow tests are not enabled")

    p_in = p_ref.with_suffix('')
    with open(p_ref, 'r') as f:
        s_ref = f.read().split('\n')
    with open(p_in, 'r') as f:
        s_in = f.read().split('\n')

    if "test_functions_ext" in p_in.name and sys.version_info < (3, 13):
        s_ref = [line.replace("types.CapsuleType", "typing_extensions.CapsuleType") for line in s_ref]
        s_ref.insert(3, "")
        s_ref.insert(4, "import typing_extensions")

    s_in = remove_platform_dependent(s_in)
    s_ref = remove_platform_dependent(s_ref)

    diff = list(difflib.unified_diff(
        s_ref,
        s_in,
        fromfile=str(p_ref),
        tofile=str(p_in)
    ))
    if len(diff):
        for p in diff:
            print(p.rstrip(), file=sys.stderr)
        print(
            '\nWarning: generated stubs do not match their references. If you\n'
            'intentionally changed a test suite extension, it may be necessary\n'
            'to replace the .pyi.ref file with the generated .pyi file. But\n'
            'please double-check that the change makes sense.',
            file=sys.stderr
        )
        assert False
