import pytest

import test_named_tuple_ext as t


# ---------------------------------------------------------------------------
# Basic round-trips with both binding flavours
# ---------------------------------------------------------------------------

def test01_helper_api_basic():
    p = t.make_point(3, 4.5)
    assert isinstance(p, tuple)
    assert p._fields == ("x", "y")
    assert p.x == 3
    assert p.y == 4.5
    assert p == (3, 4.5)


def test02_macro_api_basic():
    c = t.make_color(1, 2)
    assert isinstance(c, tuple)
    assert c._fields == ("r", "g")
    assert c.r == 1 and c.g == 2


def test03_kwargs_construction():
    p = t.Point(x=10, y=20.5)
    assert p.x == 10 and p.y == 20.5
    assert t.point_x(p) == 10 and t.point_y(p) == 20.5


def test04_roundtrip_named_instance():
    p = t.Point(7, 8.25)
    p2 = t.roundtrip_point(p)
    assert isinstance(p2, t.Point)
    assert p2 == p


def test05_roundtrip_plain_tuple_input():
    # from_python accepts a plain tuple of the right shape.
    p = t.roundtrip_point((11, 12.5))
    assert isinstance(p, t.Point)
    assert p.x == 11 and p.y == 12.5


def test06_from_cpp_always_returns_subclass():
    # Returned tuples are always the registered NamedTuple subclass,
    # even when the function constructs the C++ struct from raw values.
    p = t.make_point(1, 2.0)
    assert type(p) is t.Point
    p2 = t.roundtrip_point((1, 2.0))  # plain tuple in, Point out
    assert type(p2) is t.Point


def test07_sentinel_attribute():
    # Task 2 (stubgen) keys off this sentinel to discover bound NamedTuple
    # classes -- it must stay stable.
    assert getattr(t.Point, "__nb_named_tuple__", False) is True
    assert getattr(t.Color, "__nb_named_tuple__", False) is True


def test08_macro_api_sum():
    c = t.Color(r=5, g=7)
    assert t.color_sum(c) == 12
    assert t.color_sum((5, 7)) == 12


# ---------------------------------------------------------------------------
# collections.namedtuple surface
# ---------------------------------------------------------------------------

def test09_tuple_protocol_methods():
    p = t.Point(1, 2.0)
    assert p._asdict() == {"x": 1, "y": 2.0}
    p2 = p._replace(x=99)
    assert p2 == (99, 2.0)
    assert isinstance(p2, t.Point)
    # Pattern-matching support (PEP 634): __match_args__ mirrors _fields.
    assert t.Point.__match_args__ == ("x", "y")


def test10_module_attribute():
    # finalize() rewrites __module__ to the binding scope so type-checkers
    # and stubgen render the canonical path instead of _frozen_importlib.
    assert t.Point.__module__ == "test_named_tuple_ext"
    assert t.Color.__module__ == "test_named_tuple_ext"


# ---------------------------------------------------------------------------
# Per-field defaults
# ---------------------------------------------------------------------------

def test11_defaults_partial_construction():
    # Only ``name`` is required: width/height fall back to defaults.
    c = t.Config("hello")
    assert c == ("hello", 80, 24)
    c2 = t.Config("custom", 100)
    assert c2 == ("custom", 100, 24)
    c3 = t.Config(name="kw", width=120, height=40)
    assert c3 == ("kw", 120, 40)


def test12_field_defaults_metadata():
    # The defaults dict is exposed via collections.namedtuple's standard
    # _field_defaults attribute -- stubgen reads it to emit ``= 80`` etc.
    assert t.Config._field_defaults == {"width": 80, "height": 24}


def test13_default_config_roundtrip():
    c = t.default_config()
    assert isinstance(c, t.Config)
    assert c == ("untitled", 80, 24)
    assert t.roundtrip_config(c) == c


# ---------------------------------------------------------------------------
# Optional fields
# ---------------------------------------------------------------------------

def test14_optional_some():
    o = t.make_optitem(1, "hi")
    assert o == (1, "hi")
    assert t.roundtrip_optitem(o) == o


def test15_optional_none():
    o = t.make_optitem(2, None)
    assert o == (2, None)
    assert t.roundtrip_optitem((3, None)) == (3, None)


# ---------------------------------------------------------------------------
# Nested NamedTuples
# ---------------------------------------------------------------------------

def test16_nested_roundtrip():
    outer = t.make_outer(1, 2.0, 5)
    assert isinstance(outer.origin, t.Point)
    assert outer.origin == (1, 2.0)
    assert outer.weight == 5
    outer2 = t.roundtrip_outer(outer)
    assert outer2 == outer
    assert isinstance(outer2.origin, t.Point)


def test17_nested_accepts_plain_tuples():
    # An ``Outer`` accepts ``(Point-shape, weight)`` where the nested
    # field is itself a plain tuple.
    outer = t.roundtrip_outer(((9, 8.0), 3))
    assert isinstance(outer, t.Outer)
    assert isinstance(outer.origin, t.Point)
    assert outer.origin == (9, 8.0) and outer.weight == 3


# ---------------------------------------------------------------------------
# Self-referential type
# ---------------------------------------------------------------------------

def test18_tree_leaf_and_branch():
    leaf = t.tree_leaf(7)
    assert leaf == (7, [])
    branch = t.tree_branch(1, [t.tree_leaf(2), t.tree_leaf(3)])
    assert branch.value == 1
    assert [c.value for c in branch.children] == [2, 3]
    assert all(isinstance(c, t.Tree) for c in branch.children)


def test19_tree_sum_roundtrip():
    # Build a small tree and verify it round-trips through C++.
    tree = t.tree_branch(1, [
        t.tree_leaf(2),
        t.tree_branch(3, [t.tree_leaf(4), t.tree_leaf(5)]),
    ])
    assert t.tree_sum(tree) == 15


# ---------------------------------------------------------------------------
# Error messages on wrong-shape input
# ---------------------------------------------------------------------------

def test20_wrong_arity_rejected():
    with pytest.raises(TypeError, match="incompatible function arguments"):
        t.roundtrip_point((1,))  # too few fields
    with pytest.raises(TypeError, match="incompatible function arguments"):
        t.roundtrip_point((1, 2.0, 3.0))  # too many fields


def test21_wrong_field_type_rejected():
    with pytest.raises(TypeError, match="incompatible function arguments"):
        t.roundtrip_point(("not", "numbers"))


def test22_non_sequence_rejected():
    with pytest.raises(TypeError, match="incompatible function arguments"):
        t.roundtrip_point(42)


# ---------------------------------------------------------------------------
# Cross-module reuse (separate extension, identical struct layout)
# ---------------------------------------------------------------------------

def test23_cross_module_input():
    import test_named_tuple_b_ext as tb
    # A ``Point`` produced by ``test_named_tuple_ext`` is accepted
    # structurally by another module's function -- the caster only
    # requires the right shape, not class identity.
    p = t.make_point(10, 20.5)
    assert tb.consume_point(p) == 30
    # Cross-module ``from_cpp`` still returns the second module's own
    # subclass (cross-module class identity is a non-goal per the spec).
    p2 = tb.roundtrip_point(p)
    assert type(p2) is tb.Point
    assert p2 == (10, 20.5)
