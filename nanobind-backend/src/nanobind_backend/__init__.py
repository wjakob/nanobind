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
        try:
            from . import _nb_backend_v1 as backend
        except ModuleNotFoundError as e:
            raise ImportError(
                "The installed nanobind-backend package contains no compiled "
                "backend for this Python interpreter. This can happen when a "
                "virtual environment is reused across a Python upgrade. "
                "Reinstall it via 'pip install --force-reinstall "
                "nanobind-backend'."
            ) from e
    else:
        raise ImportError(
            f"This nanobind-backend package bundles the backend for ABI major "
            f"version 1, but this extension requires major version "
            f"{abi_major}. Upgrade it via 'pip install -U nanobind-backend'."
        )
    return backend.fill(abi_major, platform_tag, table_capsule)
