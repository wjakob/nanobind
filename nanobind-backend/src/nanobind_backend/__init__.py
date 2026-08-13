"""nanobind backend module.

This package contains the backend part of the nanobind C++ binding library. It
serves extension modules built using nanobind's *split mode*, in which the
extension delegates to an externally hosted backend instead of shipping its
own. End users receive this package as a transitive dependency of such an
extension and do not interact with it directly. There is no user-facing API.
Please see

The package bundles one compiled backend per backend ABI major version.
``fill()`` dispatches lazily on the requested major, importing only the
requested backend.
"""

def fill(abi_major, platform_tag, table_capsule):
    """Fill a split-mode extension's function table."""
    if abi_major == 1:
        from . import _nb_backend_v1 as backend
    else:
        raise ImportError(
            f"This nanobind-backend package bundles the backend for backend "
            f"ABI major version 1, but this extension requires major "
            f"version {abi_major}. If the required major is newer, upgrade "
            f"via 'pip install -U nanobind-backend'; if it was retired, pin the "
            f"last nanobind-backend release that still bundled it."
        )
    return backend.fill(abi_major, platform_tag, table_capsule)
