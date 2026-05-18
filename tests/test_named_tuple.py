import test_named_tuple_ext as t


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
    assert c.r == 1
    assert c.g == 2


def test03_kwargs_construction():
    p = t.Point(x=10, y=20.5)
    assert p.x == 10
    assert p.y == 20.5
    assert t.point_x(p) == 10
    assert t.point_y(p) == 20.5


def test04_roundtrip_named_instance():
    p = t.Point(7, 8.25)
    p2 = t.roundtrip_point(p)
    assert isinstance(p2, t.Point)
    assert p2 == p


def test05_roundtrip_plain_tuple():
    # from_python accepts a plain tuple of the right shape.
    p = t.roundtrip_point((11, 12.5))
    assert isinstance(p, t.Point)
    assert p.x == 11
    assert p.y == 12.5


def test06_sentinel_attribute():
    # Task 2 (stubgen) relies on this sentinel to discover bound NamedTuple
    # classes. Keep it stable.
    assert getattr(t.Point, "__nb_named_tuple__", False) is True
    assert getattr(t.Color, "__nb_named_tuple__", False) is True


def test07_macro_api_sum():
    c = t.Color(r=5, g=7)
    assert t.color_sum(c) == 12
    assert t.color_sum((5, 7)) == 12
