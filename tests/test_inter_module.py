import test_inter_module_1_ext as t1
import test_inter_module_2_ext as t2


def test01_inter_module():
    s = t1.create_shared()
    assert t2.check_shared(s)
