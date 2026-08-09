"""PEP 563 turns the annotations below into strings that name modules by
their local alias."""

from __future__ import annotations

import collections as coll
import collections.abc as cabc
import decimal as dec

import py_recursive_stub_test.alias as me


class Local:
    pass


def f(x: dec.Decimal) -> None: ...


def g(x: cabc.Sequence) -> me.Local: ...


def h(x: coll.abc.Sequence) -> None: ...
