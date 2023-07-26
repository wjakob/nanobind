# Adapted from:
# https://github.com/google/clif/blob/5718e4d0807fd3b6a8187dde140069120b81ecef/clif/testing/python/python_multiple_inheritance_test.py
# See also:
# https://github.com/pybind/pybind11/pull/4762

import test_python_multiple_inheritance_ext as m


class PC(m.CppBase):
    pass


# RuntimeError: nb_type_init(): invalid number of bases!
# class PPCC(PC, m.CppDrvd):
#     pass


def test_PC():
    d = PC(11)
    assert d.get_base_value() == 11
    d.reset_base_value(13)
    assert d.get_base_value() == 13
