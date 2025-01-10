.. _api_bazel:

Bazel API Reference (3rd party)
===============================

This page contains a reference of the basic APIs of
`nanobind-bazel <https://github.com/nicholasjng/nanobind-bazel>`__.

.. _rules-bazel:

Rules
-----

nanobind-bazel's rules can be used to declare different types of targets in
your Bazel project. Each of these rules is a thin wrapper around a
corresponding builtin Bazel rule producing the equivalent C++ target.

The main tool to build nanobind extensions is the ``nanobind_extension`` rule.

.. py:function:: nanobind_extension

    Declares a Bazel target representing a nanobind extension, which contains
    the Python bindings of your C++ code.

    .. code-block:: python

        def nanobind_extension(
            name,
            domain = "",
            srcs = [],
            copts = [],
            deps = [],
            local_defines = [],
            **kwargs):

    It corresponds directly to the builtin
    `cc_binary <https://bazel.build/reference/be/c-cpp#cc_binary>`__ rule,
    with all keyword arguments being directly forwarded to a ``cc_binary``
    target.

    The ``domain`` argument can be used to build the target extension under a
    different ABI domain, as described in the :ref:`FAQ <type-visibility>`
    section.

To generate typing stubs for an extension, you can use the ``nanobind_stubgen``
rule.

.. py:function:: nanobind_stubgen

    Declares a Bazel target for generating a stub file from a previously
    built nanobind bindings extension.

    .. code-block:: python

        def nanobind_stubgen(
            name,
            module,
            output_file = None,
            imports = [],
            pattern_file = None,
            marker_file = None,
            include_private_members = False,
            exclude_docstrings = False):

    It generates a `py_binary <https://bazel.build/reference/be/
    python#py_binary>`__ rule with a corresponding runfiles distribution,
    which invokes nanobind's builtin stubgen script, outputs a stub file and,
    optionally, a typing marker file into the build
    output directory (commonly called "bindir" in Bazel terms).

    All arguments (except the name, which is used only to refer to the target
    in Bazel) correspond directly to nanobind's stubgen command line interface,
    which is described in more detail in the :ref:`typing documentation <stubs>`.

    *New in nanobind-bazel version 2.1.0.*

To build a C++ library with nanobind as a dependency, use the
``nanobind_library`` rule.

.. py:function:: nanobind_library

    Declares a Bazel target representing a C++ library depending on nanobind.

    .. code-block:: python

        def nanobind_library(
            name,
            copts = [],
            deps = [],
            **kwargs):

    It corresponds directly to the builtin
    `cc_library <https://bazel.build/reference/be/c-cpp#cc_library>`__ rule,
    with all keyword arguments being directly forwarded to a ``cc_library``
    target.

To build a C++ shared library with nanobind as a dependency, use the
``nanobind_shared_library`` rule.

.. py:function:: nanobind_shared_library

    Declares a Bazel target representing a C++ shared library depending on
    nanobind.

    .. code-block:: python

        def nanobind_shared_library(
            name,
            deps = [],
            **kwargs):

    It corresponds directly to the builtin
    `cc_shared_library <https://bazel.build/reference/be/
    c-cpp#cc_shared_library>`__ rule, with all keyword arguments being directly
    forwarded to a ``cc_shared_library`` target.

    *New in nanobind-bazel version 2.1.0.*

To build a C++ test target requiring nanobind, use the ``nanobind_test`` rule.

.. py:function:: nanobind_test

    Declares a Bazel target representing a C++ test depending on nanobind.

    .. code-block:: python

        def nanobind_test(
            name,
            copts = [],
            deps = [],
            **kwargs):

    It corresponds directly to the builtin
    `cc_test <https://bazel.build/reference/be/c-cpp#cc_test>`__ rule, with all
    keyword arguments being directly forwarded to a ``cc_test`` target.

.. _flags-bazel:

Flags
-----

To customize some of nanobind's build options, nanobind-bazel exposes the
following flag settings.

.. py:function:: @nanobind_bazel//:minsize (boolean)

    Apply nanobind's size optimizations to the built extensions. Size
    optimizations are turned on by default, similarly to the CMake build.
    To turn off size optimizations, you can use the shorthand notation
    ``--no@nanobind_bazel//:minsize``.

.. py:function:: @nanobind_bazel//:py-limited-api (string)

    Build nanobind extensions against the stable ABI of the configured Python
    version. Allowed values are ``"cp312"``, ``"cp313"``, which target the
    stable ABI starting from Python 3.12 or 3.13, respectively. By default, all
    extensions are built without any ABI limitations.
