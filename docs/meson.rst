.. _meson:

Building extensions using Meson
===============================

If you prefer the Meson build system to CMake, you can build extensions using
the `Meson WrapDB <https://mesonbuild.com/Wrapdb-projects.html>`__ package.

.. note::

    This package is a community contribution maintained by
    `Will Ayd <https://github.com/WillAyd/>`__, please report issues
    directly in the
    `Meson WrapDB <https://github.com/mesonbuild/wrapdb/issues>`__ repository.

.. _meson-setup:

Adding nanobind to your Meson project
-------------------------------------

To use Meson as the build generator in your Python project, you will want to
install the
`meson-python <https://meson-python.readthedocs.io/en/latest/index.html>`__
project as a build dependency. To do so, simply add the following to your
pyproject.toml file:

.. code-block:: toml

   [project]
   name = "my_project_name"
   dynamic = ['version']

   [build-system]
   requires = ['meson-python']

   build-backend = 'mesonpy'

In your project root, you will also want to create the subprojects folder
that Meson can install into. Then you will need to install the wrap packages
for both nanobind and robin-map:

.. code-block:: sh

   mkdir -p subprojects
   meson wrap install robin-map
   meson wrap install nanobind

The ``meson.build`` definition in your project root should look like:

.. code-block:: python

   project(
     'my_project_name',
     'cpp',
     version: '0.0.1',
   )

   py = import('python').find_installation()
   nanobind_dep = dependency('nanobind', static: true)
   py.extension_module(
     'my_module_name',
     sources: ['path_to_module.cc'],
     dependencies: [nanobind_dep],
     install: true,
   )

With this configuration, you may then call:

.. code-block:: sh

   meson setup builddir
   meson compile -C builddir

To compile the extension in the ``builddir`` folder.

Alternatively, if you don't care to have a local build folder, you can use
the Python build frontend of your choosing to install the package as an
editable install. With pip, this would look like:

.. code-block:: sh

   python -m pip install -e .

.. _meson-stable-abi:

Building against the stable ABI
-------------------------------

As in nanobind's CMake config, you can build bindings targeting Python's
stable ABI, starting from version 3.12. To do this, specify the target
version using the ``limited_api`` argument in your configuration. For example,
to build extensions against the CPython 3.12 stable ABI, you would use:

.. code-block:: python

   py.extension_module(
     'my_module_name',
     sources: ['path_to_module.cc'],
     dependencies: [nanobind_dep],
     install: true,
     limited_api: '3.12',
   )

In your ``meson.build`` file.
