How to make a new release?
--------------------------

Releasing nanobind X.Y.Z
^^^^^^^^^^^^^^^^^^^^^^^^

0. If the backend ABI changed since the last release, temporarily pause here
   and follow the instructions below to release ``nanobind-backend`` first.

1. Ensure that the full version of nanobind is checked out (including the
   ``robin_map`` submodule).

2. Run ``python src/version.py -w X.Y.Z``.

3. Add the release date to ``docs/changelog.rst``. Mention the backend ABI
   version if it changed.

4. Update ``cmake/nanobind-config.cmake`` if new C++ source or header files
   were added.

5. Commit: ``git commit -am "vX.Y.Z release"``

6. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

7. Push: ``git push`` and ``git push --tags``

9. Run ``pipx run build``

10. Upload: ``twine upload --repository nanobind <filename>``

11. Run ``python src/version.py -w X.Y.Z-dev1``

Releasing nanobind-backend
^^^^^^^^^^^^^^^^^^^^^^^^^^

The backend package is versioned separately from nanobind (see :ref:`split mode
<abi-versioning>`). Backend releases are made together with nanobind releases,
from the release commit.

1. Run ``python src/version.py -b X.Y.Z``. This updates
   ``nb_backend.h``, ``nanobind-backend/pyproject.toml``, and the
   documentation.

2. Commit: ``git commit -am "vX.Y.Z backend release"``

3. Tag: ``git tag -a backend-vX.Y.Z -m "vX.Y.Z backend release"``

2. On the *Actions* tab of the GitHub project, run the ``backend-wheels``
   workflow on the release tag. It builds the wheel matrix with
   ``cibuildwheel``, runs the test suite against the wheels, and attaches them
   as a build artifact.

3. Download the artifact and upload the wheels (no sdist!) via:
   ``twine upload --repository nanobind-backend dist/*.whl``.
