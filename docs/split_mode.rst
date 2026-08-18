.. _split-mode:

.. cpp:namespace:: nanobind

Split mode
==========

Since version 3.0, nanobind extensions can be built in *split mode*. It exists
to fix the *wheel distribution problem*, where users must build binary wheels
for a frustratingly large matrix of Python versions and platforms.

Motivation
----------

The author of this project maintains an `open source renderer
<https://github.com/mitsuba-renderer/mitsuba3>`__ distributed via `PyPI
<https://pypi.org/>`__. To make it broadly accessible, it targets

- 3 operating systems: macOS, Linux, Windows.
- 2 processors: ``aarch64``, ``x86_64``.
- All non-EOL Python versions, e.g., 3.10-3.14 (usually 5 releases).

That's 30 wheels, each of which is quite big (~60MiB). Shipped this way, a
single release takes up 1.8 GiB. With the `10 GiB storage limit of PyPI
<https://docs.pypi.org/project-management/storage-limits/>`__, there is only
enough space for 5 releases!

While compiling per OS and processor is unavoidable, it would be nice to at
least reduce the matrix by a factor of 5 by not targeting each Python minor
version.

Stable ABI
----------

The Python stable ABI (supported by nanobind via the ``STABLE_ABI`` option) is
in principle designed to fix this problem. But it is not a perfect solution:

- Python's stable ABI makes nanobind slower. Performance-critical code like
  nanobind's function dispatcher needs access to Python internals to work
  efficiently, and such low-level access is prohibited by the stable ABI.

- Meaningfully reducing the wheel count only works when targeting a low stable
  ABI version. The earliest version usable by nanobind was 3.12, which means
  that users still have to ship separate wheels for Python 3.10 and 3.11.

- A Python 3.12 stable ABI floor would leave nanobind "forever frozen"
  at the stagnant 3.12 feature set, which lacks many important features
  and improvements shipped since then.

The solution
------------

Python extensions consist of two parts: the user binding code ("frontend"), and
the backend bits (``libnanobind``). Normally, they are both linked into a
combined binary. This can be very efficient when targeting a specific Python
version, but performance of the ``libnanobind`` backend suffers when targeting
the stable ABI.

Split mode addresses the problems by splitting the frontend and the backend into
separate Python modules, as illustrated below.

.. only:: not latex

   .. image:: images/split-light.svg
     :width: 800
     :align: center
     :class: only-light

   .. image:: images/split-dark.svg
     :width: 800
     :align: center
     :class: only-dark

.. only:: latex

   .. image:: images/split-light.svg
     :width: 800
     :align: center

This split also changes how extensions are distributed:

- The backend is separately compiled for each Python version and contains the
  complex and performance-critical parts that benefit from this tight coupling.
  It is tiny and available on `PyPI
  <https://pypi.org/project/nanobind-backend/>`__ for relevant platforms and
  Python versions, so users do not have to worry about it (though it is
  possible for them to also ship their own backend).

- The frontend part delegates complex steps to the backend. As a result, a
  *single* binary per platform covers all Python versions from 3.10 upwards.
  Extensions become slightly smaller, since they do not need to ship the
  backend (~150-200 KiB).

- Backend improvements (bug fixes, performance work, support for new Python
  versions) reach already-released extensions through a backend upgrade,
  without recompiling the extensions.

Split mode is an opt-in feature. The default static and shared modes continue
to work exactly as before.

Using split mode
----------------

To use split mode, pass the ``BACKEND_MODULE`` parameter to
:cmake:command:`nanobind_add_module`.

.. code-block:: cmake

   nanobind_add_module(my_ext my_ext.cpp BACKEND_MODULE nanobind_backend)

As with regular build, you may specify ``NB_DOMAIN`` to :ref:`isolate
<type-visibility>` your extension from others even when using a shared backend.

Next, declare the backend package as a runtime dependency in
``pyproject.toml``.

.. code-block:: toml

   [project]
   dependencies = ["nanobind-backend>=1.0"]

The ``>=`` constraint names the backend ABI version of the nanobind release
used for building (``nanobind-backend>=1.0`` for this release). CMake also
prints it when configuring a split-mode extension.

.. warning::

   Do **not** specify an upper bound (``<=``), and do not pin a specific
   version (``==``). Newer backends can always serve older extensions. If two
   hypothetical projects using nanobind were to pin different versions of
   ``nanobind-backend``, they could not be installed at the same time.

Limitations
-----------

The frontend and backend must agree on a *platform ABI*, which includes the
compiler family, C++ standard library, CRT flavor on Windows, debug mode, and
free-threading. The official wheel targets the "mainstream" ABI for each
platform:

.. list-table::
   :header-rows: 1

   * - Platform
     - Architectures
     - Toolchain
   * - Windows
     - ``x86_64``, ``arm64`` (Python 3.11 and newer)
     - MSVC with the ``/MD`` runtime (release, shared CRT)
   * - Linux
     - ``x86_64``, ``aarch64``, ``riscv64``
     - GCC or Clang with libstdc++ as provided by manylinux
   * - macOS
     - ``x86_64``, ``arm64``
     - AppleClang with libc++

Projects using a different toolchain or standard library cannot use the
``nanobind-backend`` package. They can usually still use split mode but must
ship their own :ref:`custom backend module <custom-backend>`.

Finally, statically linking the C++ library into an extension or a backend
module is unsupported in split mode, as the C++ runtime must be shared to
correctly propagate exceptions. The same goes for a private copy bundled into
the wheel, which is what ``auditwheel`` does on ``musllinux``. There are
therefore no ``musllinux`` backend wheels.

Free-threading
--------------

Split mode supports Python 3.15 and newer via the provisional `abi3t` stable
ABI (see `PEP 803 <https://peps.python.org/pep-0803/>`__). To do so, specify
both ``FREE_THREADED`` and ``BACKEND_MODULE``. Older Python versions lack a
stable ABI for free-threading and are unsupported.

Free-threaded Python cannot load classic ``abi3`` stable ABI modules.
Therefore, to support both regular and free-threaded Python, you must build
your extension once for ``abi3`` and once for ``abi3t``.

.. _custom-backend:

Compiling a custom backend
--------------------------

To compile a custom backend module, use the
:cmake:command:`nanobind_add_backend` command. It accepts a subset of the
options of :cmake:command:`nanobind_add_module`. The :ref:`reference
documentation <highlevel-cmake>` provides more details.

.. code-block:: cmake

   nanobind_add_backend(my_backend)

A backend module serves any number of extensions and :ref:`type visibility
domains <type-visibility>` at once; the ``NB_DOMAIN`` parameter of
:cmake:command:`nanobind_add_module` works in split mode exactly as it does
in the linked modes.

Backend modules are always compiled with nanobind's detailed assertion
messages. In a linked build, an internal check that fails in an optimized build
only prints a generic message and asks the user to rebuild in ``Debug`` mode.
That advice is useless in split mode, where the backend arrives as a prebuilt
wheel, so backends report what actually went wrong.

The ``BACKEND_MODULE`` argument of :cmake:command:`nanobind_add_module` accepts
any importable module name. Use this to point the extension to your own
backend:

.. code-block:: cmake

   nanobind_add_module(
     my_ext
     my_ext.cpp
     BACKEND_MODULE my_backend
     BACKEND_PYPI my-backend
   )

The optional ``BACKEND_PYPI`` parameter specifies the associated PyPI package
name. In the case where ``my_backend`` is not available, a better error
message will then advise the user to ``pip install`` the PyPI package ``my-backend``.

.. _abi-versioning:

Backend versioning
------------------

The ``MAJOR.MINOR.REVISION`` version of the ``nanobind-backend`` package has
the following interpretation:

- ``MAJOR.MINOR`` encodes the newest supported backend ABI contract. The minor
  version advances whenever the package adds a new feature to the ABI
  (e.g., a function slot, bit flag, etc.) without breaking the ABI itself. The
  major version advances when the ABI changes in a fundamental way. The
  ``nanobind-backend`` package must continue to serve existing extensions
  following both minor and major version changes.

- ``REVISION`` covers non-contractual changes like backend bug fixes,
  performance improvements, and support for newly released CPython versions.

nanobind also has a separately versioned *internals ABI* (mentioned
periodically in the release notes). This number versions the internal data
structures and broadly expresses which nanobind releases can communicate with
each other. Two nanobind extensions with different internals ABIs will not be
aware of each other's C++ type bindings. The internals ABI of
``nanobind-backend`` can change even in a patch release. However, this is
transparent to split mode extensions, since all extensions will move to the new
backend version together.
