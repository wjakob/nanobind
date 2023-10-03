
def test_read_globals():
    a = 1
    assert m.globals_contains_a()


def test_write_globals():
    assert "b" not in globals()
    m.globals_add_b()
    assert globals()["b"] == 123
