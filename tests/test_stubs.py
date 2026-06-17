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
        if v.strip().startswith('float16'):
            i += 1
        elif v == 'import mlx.core':
            s2.append('import mlx')
            i += 1
        elif v.startswith('def ret_numpy_half()') or \
           v.startswith('def test_slots()') or \
           v.startswith('TypeAlias'):
            i += 2
        else:
            s2.append(v)
            i += 1
    return s2


ref_paths = list(pathlib.Path(__file__).parent.rglob('*.pyi.ref'))
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
        s_ref = [line.replace("types.CapsuleType", "CapsuleType") for line in s_ref]
        s_ref.insert(5, "")
        s_ref.insert(6, "from typing_extensions import CapsuleType")

    if "test_enum_ext" in p_in.name and sys.version_info < (3, 11):
        # fallback to Python 3.10's `(str, Enum)` MRO.
        s_ref = [line.replace("(enum.StrEnum)", "(str, enum.Enum)") for line in s_ref]

    if "test_typing_ext" in p_in.name and sys.version_info < (3, 13):
        # The 'T4'/'T5' bindings from test_typing.cpp carry PEP 696 'default='
        # values (and pull in the Unpack/TypeVarTuple imports) that only exist
        # on Python 3.13+, which the reference file captures and the 3.13+ CI
        # job checks exactly. On older interpreters, drop those lines and the
        # 'from typing import' block from both the generated and reference text
        # -- collapsing the blank gaps -- so the rest still diffs cleanly.
        def strip(lines):
            out, i = [], 0
            while i < len(lines):
                l = lines[i]
                if l.startswith("from typing import"):
                    if l.endswith("("):  # skip the rest of a wrapped block
                        while lines[i].strip() != ")":
                            i += 1
                elif l.startswith(("T4 = TypeVar", "T5 = TypeVarTuple")):
                    pass
                elif not (l == "" and (not out or out[-1] == "")):
                    out.append(l)
                i += 1
            return out

        s_in, s_ref = strip(s_in), strip(s_ref)

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
