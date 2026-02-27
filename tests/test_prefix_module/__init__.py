from test_prefix_module.prefixabc import Type
import test_prefix_module.prefix  # noqa: F401


def func() -> Type:
    return Type()
