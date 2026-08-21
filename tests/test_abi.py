"""Targeted tests of nanobind's split mode. They run only when the test suite
was built with -DNB_TEST_SPLIT_MODE=ON."""

import ctypes
import importlib
import platform
import shutil
import struct
import subprocess
import sys
import sysconfig

import pytest

try:
    import nanobind_backend_test
    import test_abi_multi_ext as abi_ext
    split_mode = True
except ImportError:
    split_mode = False

pytestmark = [
    pytest.mark.skipif(not split_mode, reason="requires -DNB_TEST_SPLIT_MODE=ON"),
    pytest.mark.skipif(platform.python_implementation() != "CPython",
                       reason="exercises CPython capsule internals"),
]


@pytest.fixture(params=["test_abi_multi_ext", "test_abi_multi_lto_ext"])
def multi(request):
    return importlib.import_module(request.param)


def test01_multi_tu_types(multi):
    # Types bound in one translation unit, used from another
    p1, p2 = multi.make_point(1, 2), multi.Point(5, 7)
    box = multi.box_from_points(p1, p2)
    assert box.min.x == 1 and box.max.y == 7
    assert multi.box_width(box) == 4

    box.max = multi.make_point(11, 13)
    assert multi.box_width(box) == 10


def test01b_stable_abi_target(multi):
    # Extensions in split mode always target a stable ABI. (with the default
    # floor 3.10 for abi3 and 3.15 for abi3t).
    free_threaded = sysconfig.get_config_var("Py_GIL_DISABLED") == 1
    assert multi.limited_api == (0x030F0000 if free_threaded else 0x030A0000)


def test04_missing_backend():
    # Importing an extension whose backend module is absent fails cleanly
    with pytest.raises(ImportError) as excinfo:
        import test_abi_missing_ext  # noqa: F401
    message = str(excinfo.value)
    assert "nanobind_backend_absent" in message
    # The pip hint is reserved for the default backend module
    assert "pip install" not in message


# --- fill() handshake failure modes ----------------------------------------

# The capsule name pointer must stay valid for the capsule's lifetime
_names = []

def make_capsule(name: bytes, ptr=None):
    _names.append(name)
    fn = ctypes.pythonapi.PyCapsule_New
    fn.restype = ctypes.py_object
    fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]
    return fn(ptr if ptr is not None else 1, name, None)


def make_table(slot_count: int, abi_minor: int):
    """Build a zero-filled nb_backend_table: an 8-byte prelude (uint16
    slot_count, uint16 abi_minor, 4 reserved bytes) followed by the slots."""
    buf = ctypes.create_string_buffer(8 + slot_count * struct.calcsize("P"))
    struct.pack_into("<HH", buf, 0, slot_count, abi_minor)
    return buf


def own_tag():
    return abi_ext.platform_abi_tag.encode()


def test06_fill_rejects_non_capsule():
    # The handshake only accepts a capsule holding the dispatch table
    with pytest.raises(ValueError):
        nanobind_backend_test.fill(abi_ext.abi_major,
                                   abi_ext.platform_abi_tag, 42)


def test07_fill_rejects_foreign_platform_tag():
    # Extensions built for another platform ABI (compiler, stdlib, ...) are refused
    capsule = make_capsule(b"some_other_platform_tag")
    with pytest.raises(ImportError, match="platform ABI"):
        nanobind_backend_test.fill(abi_ext.abi_major,
                                   "some_other_platform_tag", capsule)


def test08_fill_rejects_wrong_major():
    # A backend ABI major version mismatch is an import error
    table = make_table(0, 0)
    capsule = make_capsule(own_tag(), ctypes.addressof(table))
    with pytest.raises(ImportError, match="ABI major version"):
        nanobind_backend_test.fill(abi_ext.abi_major + 1,
                                   abi_ext.platform_abi_tag, capsule)


def test10_fill_rejects_newer_minor():
    # An extension built against a newer minor version than the backend offers
    table = make_table(0, 0xffff)
    capsule = make_capsule(own_tag(), ctypes.addressof(table))
    with pytest.raises(ImportError, match="only offers"):
        nanobind_backend_test.fill(abi_ext.abi_major,
                                   abi_ext.platform_abi_tag, capsule)


def test11_fill_fills_requested_slots():
    # An extension that declares a smaller table (an older nanobind) must
    # have exactly its declared slots filled
    table = make_table(4, 0)
    capsule = make_capsule(own_tag(), ctypes.addressof(table))
    assert nanobind_backend_test.fill(abi_ext.abi_major,
                                      abi_ext.platform_abi_tag,
                                      capsule) is None
    slots = struct.unpack_from("4P", table, 8)
    assert all(s != 0 for s in slots)
