.. _api_cmake:

CMake API Reference
===================

nanobind's CMake API simplifies the process of building python extension
modules. This is needed because quite a few steps are involved: nanobind must
build the module, a library component, link the two together, and add a
different set of compilation and linker flags depending on the target platform.

The section on :ref:`building extensions <building>` provided an introductory
example of how to set up a basic build system via the
:cmake:command:`nanobind_add_module` command, which is the :ref:`high level
<highlevel-cmake>` build interface. The defaults chosen by this function are
somewhat opinionated, however. For this reason, nanobind also provides an
alternative :ref:`low level <lowlevel-cmake>` interface that decomposes it into
smaller steps.


.. _highlevel-cmake:

High-level interface
--------------------

The high-level interface consists of just one CMake command:

.. cmake:command:: nanobind_add_module

   Compile a nanobind extension module using the specified target name,
   optional flags, and source code files. Use it as follows:

   .. code-block:: cmake

      nanobind_build_library(
        my_ext                   # Target name
        NB_STATIC STABLE_API LTO # Optional flags (see below)
        my_ext.h                 # Source code files below
        my_ext.cpp)

   It supports the following optional parameters:

   .. list-table::

      * - ``NOMINSIZE``
        - Don't perform optimizations to minimize binary size.
      * - ``STABLE_ABI``
        - Perform a `stable ABI
          <https://docs.python.org/3/c-api/stable.html>`_ build, making it
          possible to use a compiled extension across Python minor versions.
          Only Python >= 3.12 is supported. The flag is ignored on older
          Python versions.
      * - ``NOSTRIP``
        - Don't strip debug symbols from the compiled extension
          when performing release builds.
      * - ``NB_SHARED``
        - Compile the core nanobind library as a shared library (the default).
      * - ``NB_STATIC``
        - Compile the core nanobind library as a static library. This
          simplifies redistribution but unnecessarily increases the binary
          storage footprint when a project contains many Python extensions.
      * - ``PROTECT_STACK``
        - Don't remove stack smashing-related protections.
      * - ``LTO``
        - Perform link time optimization.

   :cmake:command:`nanobind_add_module` performs the following
   steps to produce bindings.

   - It creates a CMake library via ``add_library(target_name MODULE ...)`` and
     enables the use of C++17 features during compilation.

   - It creates a CMake target for an internal library component required by
     nanobind (named ``nanobind-..`` where ``..`` depends on the compilation
     flags). This is only done once when compiling multiple extensions.

     This library component can either be a static or shared library depending
     on whether the optional ``NB_STATIC`` or ``NB_SHARED`` parameter was
     provided to ``nanobind_add_module()``. The default is a static build,
     which simplifies redistribution (only one shared library must be deployed).

     When a project contains many Python extensions, a shared build is
     preferable to avoid unnecessary binary size overheads that arise from
     redundant copies of the ``nanobind-...`` component.

   - It links the newly created library against the ``nanobind-..`` target.

   - It appends the library suffix (e.g., ``.cpython-39-darwin.so``) based
     on information provided by CMake’s ``FindPython`` module.

   - When requested via the optional ``STABLE_ABI`` parameter,
     the build system will create a `stable ABI
     <https://docs.python.org/3/c-api/stable.html>`_ extension module with a
     different suffix (e.g., ``.abi3.so``).

     Once compiled, a stable ABI extension can be reused across Python minor
     versions. In contrast, ordinary builds are only compatible across patch
     versions. This feature requires Python >= 3.12 and is ignored on older
     versions. Note that use of the stable ABI come at a small performance
     cost since nanobind can no longer access the internals of various data
     structures directly. If in doubt, benchmark your code to see if the cost
     is acceptable.

   - In non-debug modes, it compiles with *size optimizations* (i.e.,
     ``-Os``). This is generally the mode that you will want to use for
     C++/Python bindings. Switching to ``-O3`` would enable further
     optimizations like vectorization, loop unrolling, etc., but these all
     increase compilation time and binary size with no real benefit for
     bindings.

     If your project contains portions that benefit from ``-O3``-level
     optimizations, then it’s better to run two separate compilation
     steps. An example is shown below:

     .. code:: cmake

        # Compile project code with current optimization mode configured in CMake
        add_library(example_lib STATIC source_1.cpp source_2.cpp)
        # Need position independent code (-fPIC) to link into 'example_ext' below
        set_target_properties(example_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

        # Compile extension module with size optimization and add 'example_lib'
        nanobind_add_module(example_ext common.h source_1.cpp source_2.cpp)
        target_link_libraries(example_ext PRIVATE example_lib)

     Size optimizations can be disabled by specifying the optional
     ``NOMINSIZE`` argument, though doing so is not recommended.

   - ``nanobind_add_module()`` also disables stack-smashing protections
     (i.e., it specifies ``-fno-stack-protector`` to Clang/GCC).
     Protecting against such vulnerabilities in a Python VM seems futile,
     and it adds non-negligible extra cost (+8% binary size in
     benchmarks). This behavior can be disabled by specifying the optional
     ``PROTECT_STACK`` flag. Either way, is not recommended that you use
     nanobind in a setting where it presents an attack surface.

   - In non-debug compilation modes, it strips internal symbol names from
     the resulting binary, which leads to a substantial size reduction.
     This behavior can be disabled using the optional ``NOSTRIP``
     argument.

   - Link-time optimization (LTO) is *not active* by default; benefits
     compared to pybind11 are relatively low, and this tends to make
     linking a build bottleneck. That said, the optional ``LTO`` argument
     can be specified to enable LTO in non-debug modes.

.. _lowlevel-cmake:

Low-level interface
-------------------

Instead of :cmake:command:`nanobind_add_module` nanobind also exposes a more
fine-grained interface to the underlying operations.
The following

.. code-block:: cmake

    nanobind_add_module(my_ext NB_SHARED LTO my_ext.cpp)

is equivalent to

.. code-block:: cmake

    # Build the core parts of nanobind once
    nanobind_build_library(nanobind SHARED)

    # Compile an extension library
    add_library(my_ext MODULE my_ext.cpp)

    # .. and link it against the nanobind parts
    target_link_libraries(my_ext PRIVATE nanobind)

    # .. enable size optimizations
    nanobind_opt_size(my_ext)

    # .. enable link time optimization
    nanobind_lto(my_ext)

    # .. disable the stack protector
    nanobind_disable_stack_protector(my_ext)

    # .. set the Python extension suffix
    nanobind_extension(my_ext)

    # .. set important compilation flags
    nanobind_compile_options(my_ext)

    # .. set important linker flags
    nanobind_link_options(my_ext)

The various commands are described below:

.. cmake:command:: nanobind_build_library

   Compile the core nanobind library. The function expects only the target
   name and uses a slightly unusual parameter passing policy: its behavior
   changes based on whether or not one the following substrings is detected
   in the target name:

   .. list-table::
      :widths: 10 50

      * - ``-static``
        - Perform a static library build (shared is the default).
      * - ``-abi3``
        - Perform a stable ABI build.
      * - ``-lto``
        - Use link time optimization when compiling for release mode. This
          is done by default for shared builds, and the flag only controls
          the behavior of static builds.

   .. code-block:: cmake

      # Normal shared library build
      nanobind_build_library(nanobind)

      # Static ABI3 build with LTO
      nanobind_build_library(nanobind-static-abi3-lto)

.. cmake:command:: nanobind_opt_size

   This function enable size optimizations in ``Release``, ``MinSizeRel``,
   ``RelWithDebInfo`` modes. It expects a single target as argument, as in

   .. code-block:: cmake

      nanobind_opt_size(my_target)

.. cmake:command:: nanobind_lto

   This function enable link-time optimization in ``Release`` and
   ``MinSizeRel`` modes. It expects a single target as argument, as in

   .. code-block:: cmake

      nanobind_lto(my_target)


.. cmake:command:: nanobind_disable_stack_protector

   The stack protector affects the binary size of bindings negatively (+8%
   on Linux in benchmarks). Protecting from stack smashing in a Python VM
   seems in any case futile, so this function disables it for the specified
   target when performing a build with optimizations. Use it as follows:

   .. code-block:: cmake

      nanobind_disable_stack_protector(my_target)

.. cmake:command:: nanobind_extension

   This function assigns an extension name to the compiled binding, e.g.,
   ``.cpython-311-darwin.so``. Use it as follows:

   .. code-block:: cmake

      nanobind_extension(my_target)

.. cmake:command:: nanobind_extension_abi3

   This function assigns a stable ABI extension name to the compiled binding,
   e.g., ``.abi3.so``. Use it as follows:

   .. code-block:: cmake

      nanobind_extension_abi3(my_target)


.. cmake:command:: nanobind_compile_options

   This function sets recommended compilation flags. Currently, it specifies
   ``/bigobj`` and ``/MP`` on MSVC builds, and it does nothing other platforms
   or compilers. Use it as follows:

   .. code-block:: cmake

      nanobind_compile_options(my_target)

.. cmake:command:: nanobind_link_options

   This function sets recommended linker flags. Currently, it controls link
   time handling of undefined symbols on Apple platforms related to Python C
   API calls, and it does nothing other platforms. Use it as follows:

   .. code-block:: cmake

      nanobind_link_options(my_target)
