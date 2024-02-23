.. _stubs:

Stub generation
===============

A *stub file* provides a *typed* and potentially documented summary of a
module's class, function, and variable declarations. Stub files have the
extension ``.pyi`` and are often shipped along with Python extensions. They
are needed to enable autocompletion and static type checking in tools like
`Visual Studio Code <https://code.visualstudio.com>`__, `MyPy
<https://github.com/python/mypy>`__, `PyRight
<https://github.com/microsoft/pyright>`__ and `PyType
<https://github.com/google/pytype>`__.

Take for example the following function:

.. code-block:: python

   def square(x: int) -> int:
       '''Return the square of the input'''
       return x*x

The associated default stub removes the body, while retaining the docstring:

.. code-block:: python

   def square(x: int) -> int:
       '''Return the square of the input'''

An undocumented stub replaces the entire body with the Python ellipsis object
(``...``).

.. code-block:: python

   def square(x: int) -> int: ...

Complex default arguments are often also abbreviated with ``...`` to improve
the readability of signatures. You can read more about stub files in the
`typing documentation
<https://typing.readthedocs.io/en/latest/source/stubs.html>`__ and the `MyPy
documentation <https://mypy.readthedocs.io/en/stable/stubs.html>`__.

nanobind's ``stubgen`` tool automates the process of stub generation to turn
modules containing a mixture of ordinary Python code and C++ bindings into an
associated ``.pyi`` file.

The main challenge here is that C++ bindings are unlike ordinary Python
objects, which causes standard mechanisms to extract their signature to fail.
Existing tools like MyPy's `stubgen
<https://mypy.readthedocs.io/en/stable/stubgen.html>`__ and `pybind11-stubgen
<https://github.com/sizmailov/pybind11-stubgen>`__ must therefore parse
docstrings to infer function signatures, which is brittle and does not always
produce high-quality output.

nanobind functions expose a ``__nb_signature__`` property, which provides
structured information about typed function signatures, overload chains, and
default arguments. nanobind's ``stubgen`` leverages this information to
reliably generate high-quality stubs that are usable by static type checkers.

There are three ways to interface with the stub generator described in
the following subsections.

CMake interface
---------------

nanobind's CMake interface provides the :cmake:command:`nanobind_add_stub`
command for stub generation at build or install time. It generates a single
stub at a time--more complex cases involving large numbers of stubs are easily
handled using standard CMake constructs (e.g. a ``foreach()`` loop).

The command requires a target name (e.g., ``my_ext_stub``) that must be unique
but has no other significance. Once all dependencies (``DEPENDS`` parameter)
are met, it will invoke ``stubgen`` to turn a single module (``MODULE``
parameter) into a stub file (``OUTPUT`` parameter).

For this to work, the module must be importable. ``stubgen`` will add all paths
specified as part of the ``PYTHON_PATH`` parameter and then execute ``import
my_ext``, raising an error if this fails.

.. code-block:: cmake

   nanobind_add_stub(
     my_ext_stub
     MODULE my_ext
     OUTPUT my_ext.pyi
     PYTHON_PATH $<TARGET_FILE_DIR:my_ext>
     DEPENDS my_ext
   )

Typed extensions normally identify themselves via the presence of an empty file
named ``py.typed`` in each module directory. :cmake:command:`nanobind_add_stub`
can optionally generate this file as well.

.. code-block:: cmake

   nanobind_add_stub(
     ...
     MARKER_FILE py.typed
     ...
   )

CMake tracks the generated outputs in its dependency graph. The combination of
compiled extension module, stub, and marker file can subsequently be installed
by subsequent ``install()`` directives.

.. code-block:: cmake

  install(TARGETS my_ext DESTINATION ".")
  install(FILES py.typed my_ext.pyi DESTINATION ".")

In certain situations, it may be tricky to import an extension that is built
but not yet installed to its final destination. To handle such cases, specify
the ``INSTALL_TIME`` parameter to :cmake:command:`nanobind_add_stub` to delay
stub generation to the installation phase.

.. code-block:: cmake

    install(TARGETS my_ext DESTINATION ".")

    nanobind_add_stub(
      my_ext_stub
      INSTALL_TIME
      MODULE my_ext
      OUTPUT my_ext.pyi
      PYTHON_PATH "."
    )

This requires several changes:

1. ``PYTHON_PATH`` must be adjusted so that it references a location relative
   to ``CMAKE_INSTALL_PREFIX`` from which the installed module is importable.

2. The :cmake:command:`nanobind_add_stub` command should be preceded by
   ``install(TARGETS my_ext)`` and ``install(FILES`` commands that place all
   data (compiled extension files, plain Python code, etc.) needed to bring the
   module into an importable state.

   Place all relevant ``install()`` directives within the same
   ``CMakeLists.txt`` file to ensure that these steps are executed
   sequentially.

3. Dependencies (``DEPENDS``) no longer need to be listed. These are build-time
   constraints that do not apply in the installation phase.

4. The output file path (``OUTPUT``) is relative to ``CMAKE_INSTALL_PREFIX``
   and may need adjustments as well.

The :cmake:command:`nanobind_add_stub` command has a few other options, please
refer to its documentation for details.

Command line interface
----------------------

Alternatively, you can invoke ``stubgen`` on the command line. The nanobind
package must be installed for this to work, e.g., via ``pip install nanobind``.
The command line interface is also able to generate multiple stubs at once
(simply specify ``-m MODULE`` several times).

.. code-block:: bash

   $ python -m nanobind.stubgen -m my_ext -M py.typed
   Module "my_ext" ..
     - importing ..
     - analyzing ..
     - writing stub "my_ext.pyi" ..
     - writing marker file "py.typed" ..

Unless an output file (``-o``) or output directory (``-O``) is specified, this
places the ``.pyi`` files directly into the module. Existing stubs are
overwritten without warning.

The program has the following command line options:

.. code-block:: text

   usage: python -m nanobind.stubgen [-h] [-o FILE] [-O PATH] [-i PATH] [-m MODULE]
                                     [-M FILE] [-P] [-D] [-q]

   Generate stubs for nanobind-based extensions.

   options:
     -h, --help                   show this help message and exit
     -o FILE, --output-file FILE  write generated stubs to the specified file
     -O PATH, --output-dir PATH   write generated stubs to the specified directory
     -i PATH, --import PATH       add the directory to the Python import path (can
                                  specify multiple times)
     -m MODULE, --module MODULE   generate a stub for the specified module (can
                                  specify multiple times)
     -M FILE, --marker FILE       generate a marker file (usually named 'py.typed')
     -P, --include-private        include private members (with single leading or
                                  trailing underscore)
     -D, --exclude-docstrings     exclude docstrings from the generated stub
     -q, --quiet                  do not generate any output in the absence of failures


Python interface
----------------

Finally, you can import ``stubgen`` into your own Python programs and use it to
programmatically generate stubs with a finer degree of control.

To do so, construct an instance of the ``StubGen`` class and repeatedly call
``.put()`` to register modules or contents within the modules (specific
methods, classes, etc.). Afterwards, the ``.get()`` method returns a string
containing the stub declarations.


.. code-block:: python

   from nanobind.stubgen import StubGen
   import my_module

   sg = StubGen()
   sg.put(my_module)
   print(sg.get())

Note that for now, the ``nanobind.stubgen.StubGen`` API is considered
experimental and not subject to the semantic versioning policy used by the
nanobind project.

Customizing type and function signatures
----------------------------------------

In larger binding projects, some customization is often needed so that static
type checkers accept the generated stubs. 

Please see the section on :ref:`overriding function signatures
<typing_signatures>` and on :ref:`parameterizing generic types
<typing_generics>` for ideas on this topic.

