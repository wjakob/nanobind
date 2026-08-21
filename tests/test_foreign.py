import test_foreign_ext as t


def test01_bindings_work():
    f = t.Foreign(5)
    assert f.value == 5
    f.value = 7
    assert repr(f) == "Foreign(7)"
    assert t.roundtrip(f) == 7


def test02_anchor_present():
    # The domain record is kept alive by a capsule in the module dictionary
    assert "__nanobind_internals__" in vars(t)
