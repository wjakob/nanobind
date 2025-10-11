import gc
import sys
import time
import threading
import weakref

import test_inter_module_1_ext as t1
import test_inter_module_2_ext as t2
import test_inter_module_foreign_ext as tf
import test_classes_ext as t3
import pytest
from common import xfail_on_pypy_darwin, parallelize

free_threaded = hasattr(sys, "_is_gil_enabled") and not sys._is_gil_enabled()

@xfail_on_pypy_darwin
def test01_inter_module():
    s = t1.create_shared()
    assert t2.check_shared(s)
    with pytest.raises(TypeError) as excinfo:
        assert t3.check_shared(s)
    assert 'incompatible function arguments' in str(excinfo.value)


# t1 and t2 are in the same nanobind domain. t2 has a binding for Shared.
# tf is in a separate domain. It has a binding for Shared that appears to be
# from a foreign framework (not nanobind). It also has a create_nb_binding()
# method that will create a nanobind binding for Shared in its domain.
# So in total we have three separate pytypes that bind the C++ type Shared,
# and two domains that might need to accept them.

# NB: there is some potential for different test cases to interfere with
# each other: we can't un-register a framework once it's registered and we
# can't undo automatic import/export all once they're requested. The ordering
# of these tests is therefore important. They work standalone and they work
# when run all in one process, but they might not work in a different order.


@pytest.fixture
def clean():
    tf.remove_all_bindings()
    if sys.implementation.name == "pypy":
        gc.collect()
    yield
    tf.remove_all_bindings()
    if sys.implementation.name == "pypy":
        gc.collect()


def test02_interop_exceptions_without_registration():
    # t2 defines the exception translator for Shared. Since the t1/t2 domain
    # hasn't taken any interop actions yet, it hasn't registered with pymetabind
    # and tf won't be able to use that translator.
    with pytest.raises(SystemError, match="exception could not be translated"):
        tf.throw_shared()

    # t1 can use t2's translator though, since they're in the same domain
    with pytest.raises(ValueError, match="Shared.123"):
        t1.throw_shared()


def expect(from_mod, to_mod, pattern, **extra):
    outcomes = {}
    extra_info = {}

    for thing in ("shared", "shared_sp", "shared_up", "enum"):
        print(thing)
        create = getattr(from_mod, f"create_{thing}")
        check = getattr(to_mod, f"check_{thing}")
        try:
            obj = create()
        except Exception as ex:
            outcomes[thing] = None
            extra_info[thing] = ex
            continue
        try:
            ok = check(obj)
        except Exception as ex:
            outcomes[thing] = False
            extra_info[thing] = ex
            continue
        assert ok, "instance appears corrupted"
        outcomes[thing] = True

    expected = {}
    if pattern == "local":
        expected = {
            "shared": True, "shared_sp": True, "shared_up": True, "enum": True
        }
    elif pattern == "foreign":
        expected = {
            "shared": True, "shared_sp": True, "shared_up": False, "enum": True
        }
    elif pattern == "isolated":
        expected = {
            "shared": False, "shared_sp": False, "shared_up": False, "enum": False
        }
    else:
        assert False, "unknown pattern"
    expected.update(extra)
    assert outcomes == expected


def test03_interop_unimported(clean):
    expect(tf, tf, "foreign", enum=None)
    expect(t1, t2, "local")
    expect(t1, tf, "isolated")
    expect(tf, t2, "isolated", enum=None)

    # Just an export isn't enough; you need an import too
    t2.export_for_interop(t2.Shared)
    expect(tf, t2, "isolated", enum=None)


def test04_interop_import_export_errors():
    with pytest.raises(
        RuntimeError, match="does not define a __pymetabind_binding__"
    ):
        t2.import_for_interop(tf.Convertible)

    with pytest.raises(
        RuntimeError, match="not a nanobind class or enum bound in this domain"
    ):
        tf.export_for_interop(t2.Shared)

    with pytest.raises(
        RuntimeError, match="not a nanobind class or enum bound in this domain"
    ):
        tf.export_for_interop(t2.SharedEnum)

    t2.export_for_interop(t2.Shared)
    t2.export_for_interop(t2.SharedEnum)
    t2.export_for_interop(t2.Shared)  # should be idempotent
    t2.export_for_interop(t2.SharedEnum)

    with pytest.raises(
        RuntimeError, match="is already bound by this nanobind domain"
    ):
        t2.import_for_interop(t2.Shared)


def test05_interop_exceptions():
    # Once t2 registers with pymetabind, which happens as soon as it imports
    # or exports anything, tf can translate its exceptions.
    t2.export_for_interop(t2.Shared)
    with pytest.raises(ValueError, match="Shared.123"):
        tf.throw_shared()


def test06_interop_with_cpp(clean):
    # Export t2.Shared to tf, but not the enum yet, and not from tf to t2
    t2.export_for_interop(t2.Shared)
    tf.import_for_interop(t2.Shared)
    expect(t1, tf, "foreign", enum=False)
    expect(tf, t2, "isolated", enum=None)

    # Now export t2.SharedEnum too. Note that tf doesn't have its own
    # definition of SharedEnum, so it will use the imported one and create
    # t2.SharedEnums.
    t2.export_for_interop(t2.SharedEnum)
    tf.import_for_interop(t2.SharedEnum)
    expect(t1, tf, "foreign")
    expect(tf, t2, "isolated", enum=True)

    # Enable automatic import in the t1/t2 domain. Still doesn't help with
    # tf->t1/t2 since tf.Shared is not a C++ type.
    t1.import_all()
    expect(t1, tf, "foreign")
    expect(tf, t2, "isolated", enum=True)


def test07_interop_import_explicit_errors(clean):
    with pytest.raises(RuntimeError, match=r"is not written in C\+\+"):
        t2.import_for_interop(tf.RawShared)

    t2.import_for_interop_explicit(tf.RawShared)
    t2.import_for_interop_explicit(tf.RawShared)

    with pytest.raises(RuntimeError, match=r"was already mapped to C\+\+ type"):
        t2.import_for_interop_wrong_type(tf.RawShared)


def test08_interop_with_c(clean):
    t2.export_for_interop(t2.Shared)
    tf.import_for_interop(t2.Shared)
    t2.export_for_interop(t2.SharedEnum)
    tf.import_for_interop(t2.SharedEnum)
    t2.import_for_interop_explicit(tf.RawShared)

    # Now that tf.RawShared is imported to t1/t2, everything should work.
    expect(t1, tf, "foreign")
    expect(tf, t2, "foreign")


@pytest.mark.skipif(
    sys.implementation.name == "pypy", reason="can't GC type object on pypy"
)
def test09_remove_binding(clean):
    t2.import_for_interop_explicit(tf.RawShared)

    # Remove the binding for tf.RawShared. We expect the t1/t2 domain will
    # notice the removal and automatically forget about the defunct binding.
    tf.remove_raw_binding()
    tf.create_raw_binding()

    t2.export_for_interop(t2.Shared)
    tf.import_for_interop(t2.Shared)
    t2.export_for_interop(t2.SharedEnum)
    tf.import_for_interop(t2.SharedEnum)

    expect(t1, tf, "foreign")
    expect(tf, t2, "isolated", enum=True)

    if not free_threaded:
        # More binding removal tests. These only work on non-freethreading
        # builds because nanobind immortalizes all its types on FT builds.
        t2.remove_bindings()
        t2.create_bindings()

        expect(t1, tf, "isolated")
        expect(tf, t2, "isolated", enum=None)

        t2.export_for_interop(t2.Shared)
        tf.import_for_interop(t2.Shared)
        t2.export_for_interop(t2.SharedEnum)
        tf.import_for_interop(t2.SharedEnum)

        expect(t1, tf, "foreign")
        expect(tf, t2, "isolated", enum=True)

        # Removing the binding capsule should work just as well as removing
        # the type object.
        del t2.Shared.__pymetabind_binding__
        del t2.SharedEnum.__pymetabind_binding__

        expect(t1, tf, "isolated")
        expect(tf, t2, "isolated", enum=None)

        t2.export_for_interop(t2.Shared)
        tf.import_for_interop(t2.Shared)
        t2.export_for_interop(t2.SharedEnum)
        tf.import_for_interop(t2.SharedEnum)

        expect(t1, tf, "foreign")
        expect(tf, t2, "isolated", enum=True)

    # Re-import RawShared and now everything works again.
    t2.import_for_interop_explicit(tf.RawShared)
    expect(t1, tf, "foreign")
    expect(tf, t2, "foreign")

    # Removing the binding capsule should work just as well as removing
    # the type object.
    del tf.RawShared.__pymetabind_binding__
    tf.export_raw_binding()

    # tf.RawShared was removed from the beginning of tf's list for Shared
    # and re-added on the end; also remove and re-add t2.Shared so that
    # tf.create_shared() returns a tf.RawShared
    del t2.Shared.__pymetabind_binding__
    t2.export_for_interop(t2.Shared)
    tf.import_for_interop(t2.Shared)

    expect(t1, tf, "foreign")
    expect(tf, t2, "isolated", enum=True)

    # Re-import RawShared and now everything works again.
    t2.import_for_interop_explicit(tf.RawShared)
    expect(t1, tf, "foreign")
    expect(tf, t2, "foreign")


def test10_access_binding_concurrently(clean):
    any_failed = False

    def repeatedly_attempt_conversions():
        deadline = time.time() + 1
        while time.time() < deadline:
            try:
                assert tf.check_shared(tf.create_shared())
            except:
                nonlocal any_failed
                any_failed = True
                raise

    parallelize(repeatedly_attempt_conversions, n_threads=8)
    assert not any_failed


@pytest.mark.skipif(not free_threaded, reason="not relevant on non-FT")
@pytest.mark.parametrize("multi", (False, True))
def test11_remove_binding_concurrently(clean, multi):
    transitions = 0
    limit = 5000

    if multi:
        # In 'multi' mode, we add more exports so the `tf` domain exercises
        # the linked-list-of-foreign-bindings logic
        t2.export_for_interop(t2.Shared)
        tf.import_for_interop(t2.Shared)

    def repeatedly_remove_and_readd():
        nonlocal transitions
        try:
            while transitions < limit:
                del tf.RawShared.__pymetabind_binding__
                tf.export_raw_binding()
                if multi:
                    del t2.Shared.__pymetabind_binding__
                    # The actual destruction of the capsule may be slightly
                    # delayed since it was created on a different thread.
                    # nanobind won't export a binding for a type that it thinks
                    # already has one. Retry export until the capsule shows up.
                    for _ in range(10):
                        t2.export_for_interop(t2.Shared)
                        if hasattr(t2.Shared, "__pymetabind_binding__"):
                            break
                        time.sleep(0.001)
                    else:
                        assert False, "binding removal was too delayed"
                    tf.import_for_interop(t2.Shared)
                transitions += 1
        except:
            transitions = limit
            raise

    thread = threading.Thread(target=repeatedly_remove_and_readd)
    thread.start()

    num_failed = 0
    num_successful = 0

    def repeatedly_attempt_conversions():
        nonlocal num_failed
        nonlocal num_successful
        while transitions < limit:
            try:
                tf.check_shared(tf.create_shared())
            except TypeError:
                num_failed += 1
            else:
                num_successful += 1

    try:
        parallelize(repeatedly_attempt_conversions, n_threads=8)
    finally:
        transitions = limit
        thread.join()

    # typical numbers from my machine: with limit=5000, the test takes a
    # decent fraction of a second, and num_failed and num_successful are each
    # several 10k's
    print(num_failed, num_successful)
    assert num_successful > 0
    assert num_failed > 0


def test12_multi_and_implicit(clean):
    # Create three different types of pyobject, all of which have C++ type Shared
    s1 = t1.create_shared()
    sf_raw = tf.create_shared()
    tf.create_nb_binding()
    sf_nb = tf.create_shared()

    assert type(s1) is t2.Shared
    assert type(sf_raw) is tf.RawShared
    assert type(sf_nb) is tf.NbShared

    # Test automatic import/export all
    t1.export_all()
    tf.import_all()

    # Test implicit conversions from foreign types
    for obj in (sf_nb, sf_raw, s1):
        val = tf.test_implicit(obj)
        assert val.value == 123

    # We should only be sharing in the t1->tf direction, not vice versa
    assert tf.check_shared(s1)
    assert tf.check_shared(sf_raw)
    assert tf.check_shared(sf_nb)
    with pytest.raises(TypeError):
        t2.check_shared(sf_raw)
    with pytest.raises(TypeError):
        t2.check_shared(sf_nb)

    # Now add the other direction
    t1.import_all()
    tf.export_all()
    assert t2.check_shared(sf_nb)
    # Still need an explicit import for non-C++ type
    t2.import_for_interop_explicit(tf.RawShared)
    assert t2.check_shared(sf_raw)

    # Test normally passing these various objects
    for mod in (t2, tf):
        for obj in (s1, sf_raw, sf_nb):
            assert mod.check_shared(obj)
            assert mod.check_shared_sp(obj)
        for obj in (t1.create_enum(), tf.create_enum()):
            assert mod.check_enum(obj)
