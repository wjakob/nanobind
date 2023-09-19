import os

import pytest

import env  # noqa: F401
import test_eval_ext as m


def test_evals(capture):
    with capture:
        assert m.test_eval_statements()
    assert capture == "Hello World!"

    assert m.test_eval()
    assert m.test_eval_single_statement()

    assert m.test_eval_failure()


def test_eval_empty_globals():
    assert "__builtins__" in m.eval_empty_globals(None)

    g = {}
    assert "__builtins__" in m.eval_empty_globals(g)
    assert "__builtins__" in g


def test_eval_closure():
    global_, local = m.test_eval_closure()

    assert global_["closure_value"] == 42
    assert local["closure_value"] == 0

    assert "local_value" not in global_
    assert local["local_value"] == 0

    assert "func_global" not in global_
    assert local["func_global"]() == 42

    assert "func_local" not in global_
    with pytest.raises(NameError):
        local["func_local"]()
