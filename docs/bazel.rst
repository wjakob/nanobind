.. _bazel:

Building extensions using Bazel
===============================

nanobind can be used directly with Bazel's C++ and Python rules. The Bazel
Central Registry (BCR) provides a convenient packaged setup, including stubgen.
A module extension makes sense when a project needs a specific nanobind
revision, or a custom BUILD file.

.. _bazel-setup:

Adding nanobind from the BCR
----------------------------

In your MODULE.bazel, add the BCR release of nanobind:

.. code-block:: python

    bazel_dep(name = "nanobind", version = "3.0.1")
    bazel_dep(name = "rules_cc", version = "0.2.17")
    bazel_dep(name = "rules_python", version = "1.7.0")

Choose a version suitable for your project from the `nanobind BCR page
<https://registry.bazel.build/modules/nanobind>`__.

Adding nanobind with a module extension
----------------------------------------

For a development checkout, an unreleased revision, or a project-specific BUILD
definition, fetch nanobind with a module extension instead. This is the pattern
used by larger Bazel projects like `OpenXLA <https://github.com/openxla/xla>`__.
For example, you can place the following in ``third_party/nanobind/extension.bzl``:

.. code-block:: python

    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

    def _nanobind_impl(_):
        http_archive(
            name = "nanobind",
            urls = ["https://github.com/wjakob/nanobind/archive/<revision>.tar.gz"],
            strip_prefix = "nanobind-<revision>",
            sha256 = "<sha256>",
            build_file = "//third_party/nanobind:nanobind.BUILD.bazel",
        )

    nanobind = module_extension(implementation = _nanobind_impl)

``nanobind.BUILD.bazel`` contains the BUILD targets for the fetched source.
The BCR's `BUILD overlay
<https://github.com/bazelbuild/bazel-central-registry/tree/main/modules/nanobind/3.0.1/overlay>`__
is a useful starting point for customizing your build.
Activate the extension from ``MODULE.bazel``:

.. code-block:: python

    bazel_dep(name = "bazel_skylib", version = "1.8.2")
    bazel_dep(name = "platforms", version = "1.0.0")
    bazel_dep(name = "robin-map", version = "1.4.1")
    bazel_dep(name = "rules_cc", version = "0.2.17")
    bazel_dep(name = "rules_python", version = "1.7.0")

    nanobind = use_extension(
        "//third_party/nanobind:extension.bzl", "nanobind"
    )
    use_repo(nanobind, "nanobind")

.. _bazel-build:

Declaring and building nanobind extension targets
-------------------------------------------------

The BCR overlay exposes nanobind as the ``@nanobind//:nanobind`` C++ library.
Use it with ordinary Bazel C++ rules. Splitting the implementation library from
the shared library lets you give the resulting Python extension its required
name.

.. code-block:: python

    load("@rules_cc//cc:cc_library.bzl", "cc_library")
    load("@rules_cc//cc:cc_shared_library.bzl", "cc_shared_library")

    cc_library(
        name = "my_ext_impl",
        srcs = ["my_ext.cpp"],
        deps = ["@nanobind//:nanobind"],
    )

    cc_shared_library(
        name = "my_ext",
        deps = [":my_ext_impl"],
        shared_lib_name = select({
            "@platforms//os:windows": "my_ext.pyd",
            "//conditions:default": "my_ext.so",
        }),
    )

The library must be named exactly like the module declared by ``NB_MODULE``.
Package it or add it as ``data`` to a Python target so it is importable at runtime.
``shared_lib_name`` uses the ``.pyd`` suffix required by Python on Windows.

Building for the stable ABI (ABI3)
----------------------------------

The current BCR overlay provides a regular CPython build. To build an ABI3
extension, use the module-extension setup above and customize the copied
``nanobind.BUILD.bazel`` overlay. Add the following build setting:

.. code-block:: python

    load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

    bool_flag(
        name = "abi3",
        build_setting_default = False,
        visibility = ["//visibility:public"],
    )

Then change the existing ``nanobind`` library's ``defines`` and ``deps``,
so that both nanobind and its consumers compile against the stable ABI:

.. code-block:: python

    defines = ["NB_SHARED"] + select({
        ":abi3": ["Py_LIMITED_API=0x030C0000"],
        "//conditions:default": [],
    }),
    deps = select({
        ":abi3": ["@rules_python//python/cc:current_py_cc_headers_abi3"],
        "//conditions:default": [
            "@rules_python//python/cc:current_py_cc_headers",
        ],
    }),

The ``defines`` attribute propagates ``Py_LIMITED_API`` to the extension's
sources. Name the extension using the ABI3 suffix as well:

.. code-block:: python

    cc_shared_library(
        name = "my_ext",
        deps = [":my_ext_impl"],
        shared_lib_name = select({
            "@platforms//os:windows": "my_ext.pyd",
            "@nanobind//:abi3": "my_ext.abi3.so",
            "//conditions:default": "my_ext.so",
        }),
    )

Build the ABI3 configuration with ``bazel build //my_project:my_ext
--@nanobind//:abi3``. Stable ABI builds require CPython 3.12 or newer;
see :ref:`the stable ABI documentation <stable-abi>` for its compatibility
and performance implications.

Generating stubs
----------------

Recent BCR releases also expose ``@nanobind//:stubgen`` as a ``py_binary`` and
``@nanobind//:stubgen_lib`` as its importable library. A project can use the
former directly, or define its own ``py_binary`` when it needs to arrange an
extension's runfiles or select an output location.

For example, a stub generation executable can depend on the extension as data
and on the nanobind stubgen library:

.. code-block:: python

    load("@rules_python//python:py_binary.bzl", "py_binary")

    py_binary(
        name = "my_ext_stubgen",
        srcs = ["stubgen.py"],
        data = [":my_ext"],
        deps = [
            "@nanobind//:stubgen_lib",
            "@rules_python//python/runfiles",
        ],
    )

Here, ``stubgen.py`` is a small wrapper around ``nanobind.stubgen`` that uses
the Bazel runfiles library to find ``my_ext``, makes its containing directory
importable, and invokes ``nanobind.stubgen.main(["-m", "my_ext"])``.
Run it with ``bazel run //my_project:my_ext_stubgen``.
See :ref:`stub generation <stubs>` for a list of available command-line options.

Python packaging
----------------

Unlike CMake, which has a variety of projects supporting PEP517-style
Python package builds, Bazel does not currently have a fully featured
PEP517-compliant packaging backend available.

To create Python wheels with nanobind bindings, two common strategies are

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
