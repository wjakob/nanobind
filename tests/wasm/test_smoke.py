import smoke_ext


def test_add():
    assert smoke_ext.add(2, 3) == 5


def test_targeted_stable_abi():
    assert smoke_ext.targeted_stable_abi is True


def test_platform_abi_tag():
    assert "libcpp" in smoke_ext.platform_abi_tag
