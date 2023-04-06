.. _packaging:

Packaging
=========

A Python *wheel* is a self-contained binary file that bundles Python code and
extension libraries along with metadata such as versioned package dependencies.
Wheels are easy to download and install, and they are the recommended mechanism
for distributing extensions created using nanobind.

This section walks through the recommended sequence of steps to build wheels
and optionally automate this process to simultaneously target many platforms
(Linux, Windows, macOS) and processors (x86_64, arm64) using the `GitHub
Actions <https://github.com/features/actions>`__ CI service.

Note that all of the recommended practices have already been implemented in the
`nanobind_example repository <https://github.com/wjakob/nanobind_example>`_,
which is a minimal C++ project with nanobind-based bindings. You may therefore
prefer to clone this repository and modify its contents.

Step 1: Specify build dependencies
----------------------------------

In the root directory of your project, create a file named ``pyproject.toml``
listing build-time dependencies. Runtime dependencies don't need to be added
here. The following core dependencies are required by nanobind:

.. code-block:: toml

   [build-system]
   requires = [
       "setuptools>=42",
       "wheel",
       "scikit-build>=0.16.7",
       "cmake>=3.18",
       "nanobind>=1.1.0",
       "ninja; platform_system!='Windows'"
   ]

   build-backend = "setuptools.build_meta"

Step 2: Create a ``setup.py`` file
----------------------------------

Most wheels are built using the `setuptools
<https://packaging.python.org/en/latest/guides/distributing-packages-using-setuptools/>`__
package. We also use it here in combination with `scikit-build
<https://scikit-build.readthedocs.io/en/latest>`__, which can be thought of as
a glue layer connecting setuptools with CMake.

To do so, create the file ``setup.py`` at the root of your project directory.
This file provides metadata about the project and also constitutes the entry
point of the build system.

.. code-block:: python

   import sys

   try:
       from skbuild import setup
       import nanobind
   except ImportError:
       print("The preferred way to invoke 'setup.py' is via pip, as in 'pip "
             "install .'. If you wish to run the setup script directly, you must "
             "first install the build dependencies listed in pyproject.toml!",
             file=sys.stderr)
       raise

   setup(
       name="my_ext",          # <- The package name (e.g. for PyPI) goes here
       version="0.0.1",        # <- The current version number.
       author="Your name",
       author_email="your.email@address.org",
       description="A brief description of what the package does",
       long_description="A long format description in Markdown format",
       long_description_content_type='text/markdown',
       url="https://github.com/username/repository_name",
       python_requires=">=3.8",
       license="BSD",
       packages=['my_ext'],     # <- The package will install one module named 'my_ext'
       package_dir={'': 'src'}, # <- Root directory containing the Python package
       cmake_install_dir="src/my_ext", # <- CMake will place the compiled extension here
       include_package_data=True
   )

The warning message at the top will be explained shortly. This particular
``setup.py`` file assumes the following project directory structure:

.. code-block:: text

   ├── CMakeLists.txt
   ├── pyproject.toml
   ├── setup.py
   └── src/
       ├── my_ext.cpp
       └── my_ext/
           └── __init__.py

In other words, the source code is located in a ``src`` directory containing
the Python package as a subdirectory. Naturally, the code can also be
arranged differently, but this will require corresponding modifications in
``setup.py``.

In practice, it can be convenient to add further code that extracts relevant
fields like version number, short/long description, etc. from code or the
project's README file to avoid duplication.

Step 3: Create a CMake build system
-----------------------------------

Next, we will set up a suitable ``CMakeLists.txt`` file in the root directory.
Since this build system is designed to be invoked through ``setup.py`` and
``scikit-build``, it does not make sense to perform a standalone CMake build.
The message at the top warns users attempting to do this.

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.18...3.22)
   project(my_ext)

   if (NOT SKBUILD)
     message(WARNING "\
     This CMake file is meant to be executed using 'scikit-build'. Running
     it directly will almost certainly not produce the desired result. If
     you are a user trying to install this package, please use the command
     below, which will install all necessary build dependencies, compile
     the package in an isolated environment, and then install it.
     =====================================================================
      $ pip install .
     =====================================================================
     If you are a software developer, and this is your own package, then
     it is usually much more efficient to install the build dependencies
     in your environment once and use the following command that avoids
     a costly creation of a new virtual environment at every compilation:
     =====================================================================
      $ python setup.py install
     =====================================================================")
   endif()

   # Perform a release build by default
   if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
     set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
     set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
   endif()

   # Create CMake targets for Python components needed by nanobind
   find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

   # Determine the nanobind CMake include path and register it
   execute_process(
     COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
     OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
   list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")

   # Import nanobind through CMake's find_package mechanism
   find_package(nanobind CONFIG REQUIRED)

   # We are now ready to compile the actual extension module
   nanobind_add_module(
     _my_ext_impl
     src/my_ext.cpp
   )

   # Install directive for scikit-build
   install(TARGETS _my_ext_impl LIBRARY DESTINATION .)

A simple definition of ``src/my_ext.cpp`` could contain the following:

.. code-block:: cpp

   #include <nanobind/nanobind.h>

   NB_MODULE(_my_ext_impl, m) {
       m.def("hello", []() { return "Hello world!"; });
   }

Compilation and installation will turn this binding code into a shared library
located in the ``src/my_ext`` directory with an undescored platform-dependent
name (e.g., ``_my_ext_impl.cpython-311-darwin.so``) indicating that the
extension is an implementation detail. The ``__init__.py`` file in the same
directory has the purpose of importing the extension and exposing relevant
functionality, e.g.:

.. code-block:: python

   from ._my_ext_impl import hello

Step 4: Install the package locally
-----------------------------------

It used to be common to run ``setup.py`` files directly (as in ``python
setup.py install``), but this is fragile when the environment doesn't have the
exact right versions of all build dependencies. The recommended method is via

.. code-block:: bash

   $ cd <project-directory>
   $ pip install .

``pip`` will parse the ``pyproject.toml`` file and create a fresh environment
containing all needed dependencies. Following this, you should be able to
install and access the extension.

.. code-block:: python

   >>> import my_ext
   >>> my_ext.hello()
   'Hello world!'

Alternatively, you can use the following command to generate a ``.whl`` file
instead of installing the package.

.. code-block:: bash

   $ pip wheel .

Step 5: Build wheels in the cloud
---------------------------------

On my machine, the previous step produced a file named
``my_ext-0.0.1-cp311-cp311-macosx_13_0_arm64.whl`` that is specific to
Python 3.11 running on an arm64 macOS machine. Other Python versions
and operating systems will each require their own wheels, which leads
to a challenging build matrix.

In the future (once Python 3.12 is more widespread), nanobind's Stable ABI
support will help to reduce the size of this build matrix. More information
about this will be added here at a later point.

In the meantime, we can use GitHub actions along with the powerful
`cibuildwheel <https://cibuildwheel.readthedocs.io/en/stable/>`__ package to
fully automate the process of wheel generation.

To do so, create a file named ``.github/workflows/wheels.yml`` containing
the contents of the `following file
<https://github.com/wjakob/nanobind_example/blob/master/.github/workflows/wheels.yml>`__.
You may want to remove the ``on: push:`` lines, otherwise, the action will run
after every commit, which is perhaps a bit excessive. In this case, you can
still trigger the action manually on the *Actions* tab of the GitHub project
page.

Following each run, the action provides a downloadable *build artifact*, which
is a ZIP file containing all the individual wheel files for each platform.

If you set up a GitHub actions `secret
<https://docs.github.com/en/actions/security-guides/encrypted-secrets>`__ named
``pypi_password`` containing a PyPI authentication token, the action will
automatically upload the generated wheels to the `Python Package Index (PyPI)
<https://pypi.org>`__ when the action is triggered by a `software release event
<https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository>`__.
