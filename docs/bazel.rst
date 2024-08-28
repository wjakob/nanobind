.. _bazel:

Building extensions using Bazel
===============================

If you prefer the Bazel build system to CMake, you can build extensions using
the `nanobind-bazel <https://github.com/nicholasjng/nanobind-bazel>`__ project.

.. note::

    This project is a community contribution maintained by
    `Nicholas Junge <https://github.com/nicholasjng>`__, please report issues
    directly in the nanobind-bazel repository linked above.

.. _bazel-setup:

Adding nanobind-bazel to your Bazel project
-------------------------------------------

To use nanobind-bazel in your project, you need to add it to your project's
dependency graph. Using bzlmod, the de-facto dependency management system
in Bazel starting with version 7.0, you can simply specify it as a ``bazel_dep``
in your MODULE.bazel file:

.. code-block:: python

    # Place this in your MODULE.bazel file.
    # The major version of nanobind-bazel is equal to the major version
    # of the internally used nanobind.
    # In this case, we are building bindings with nanobind@v2.
    bazel_dep(name = "nanobind_bazel", version = "2.1.0")

To instead use a development version from GitHub, you can declare the
dependency as a ``git_override()`` in your MODULE.bazel:

.. code-block:: python

    # MODULE.bazel
    bazel_dep(name = "nanobind_bazel", version = "")
    git_override(
        module_name = "nanobind_bazel",
        commit = COMMIT_SHA, # replace this with the actual commit you want.
        remote = "https://github.com/nicholasjng/nanobind-bazel",
    )

In local development scenarios, you can clone nanobind-bazel to your machine,
and then declare it as a ``local_path_override()`` dependency:

.. code-block:: python

    # MODULE.bazel
    bazel_dep(name = "nanobind_bazel", version = "")
    local_path_override(
        module_name = "nanobind_bazel",
        path = "/path/to/nanobind-bazel/", # replace this with the actual path.
    )

.. note::

    At minimum, Bazel version 6.4.0 is required to use nanobind-bazel.


.. _bazel-build:

Declaring and building nanobind extension targets
-------------------------------------------------

The main tool to build nanobind C++ extensions for your Python bindings is the
:py:func:`nanobind_extension` rule.

Like all public nanobind-bazel APIs, it resides in the ``build_defs`` submodule.
To import it into a BUILD file, use the builtin ``load`` command:

.. code-block:: python

    # In a BUILD file, e.g. my_project/BUILD
    load("@nanobind_bazel//:build_defs.bzl", "nanobind_extension")

    nanobind_extension(
        name = "my_ext",
        srcs = ["my_ext.cpp"],
    )

In this short snippet, a nanobind Python module called ``my_ext`` is declared,
with its contents coming from the C++ source file of the same name.
Conveniently, only the actual module name must be declared - its place in your
Python project hierarchy is automatically determined by the location of your
build file.

For a comprehensive list of all available build rules in nanobind-bazel, refer
to the rules section in the :ref:`nanobind-bazel API reference <rules-bazel>`.

.. _bazel-stable-abi:

Building against the stable ABI
-------------------------------

As in nanobind's CMake config, you can build bindings targeting Python's
stable ABI, starting from version 3.12. To do this, specify the target
version using the ``@nanobind_bazel//:py-limited-api`` flag. For example,
to build extensions against the CPython 3.12 stable ABI, pass the option
``@nanobind_bazel//:py-limited-api="cp312"`` to your ``bazel build`` command.

For more information about available flags, refer to the flags section in the
:ref:`nanobind-bazel API reference <flags-bazel>`.

Generating stubs for built extensions
-------------------------------------

You can also use Bazel to generate stubs for an extension directly at build
time with the ``nanobind_stubgen`` macro. Here is an example of a nanobind
extension with a stub file generation target declared directly alongside it:

.. code-block:: python

    # Same as before in a BUILD file
    load(
        "@nanobind_bazel//:build_defs.bzl",
        "nanobind_extension",
        "nanobind_stubgen",
    )

    nanobind_extension(
        name = "my_ext",
        srcs = ["my_ext.cpp"],
    )

    nanobind_stubgen(
        name = "my_ext_stubgen",
        module = ":my_ext",
    )

You can then generate stubs on an extension by invoking
``bazel run //my_project:my_ext_stubgen``. Note that this requires actually
running the target instead of only building it via ``bazel build``, since a
Python script needs to be executed for stub generation.

Naturally, since stub generation relies on the given shared object files, the
actual extensions are built in the process before invocation of the stub
generation script.

nanobind-bazel and Python packaging
-----------------------------------

Unlike CMake, which has a variety of projects supporting PEP517-style
Python package builds, Bazel does not currently have a fully featured
PEP517-compliant packaging backend available.

To produce Python wheels containing bindings built with nanobind-bazel,
you have various options, with two of the most prominent strategies being

1. Using a wheel builder script with the facilities provided by a Bazel
support package for Python, such as ``py_binary`` or ``py_wheel`` from
`rules_python <https://github.com/bazelbuild/rules_python/>`__. This is
a lower-level, more complex workflow, but it provides more granular
control of how your Python wheel is built.

2. Building all extensions with Bazel through a subprocess, by extending
a Python build backend such as ``setuptools``. This allows you to stick to
those well-established build tools, like ``setuptools``, at the expense
of more boilerplate Python code and slower build times, since Bazel is
only invoked to build the bindings extensions (and their dependencies).

In general, while the latter method requires less setup and customization,
its drawbacks weigh more severely for large projects with more extensions.

.. note::

    An example of packaging with the mentioned setuptools customization method
    can be found in the
    `nanobind_example <https://github.com/wjakob/nanobind_example/tree/bazel>`__
    repository, specifically, on the ``bazel`` branch. It also contains an
    example of how to customize flag names and set default build options across
    platforms with a ``.bazelrc`` file.
