Introduction
============

*nanobind* is a small binding library that exposes C++ types in Python and vice
versa. It is reminiscent of `Boost.Python
<https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html>`_ and `pybind11
<http://github.com/pybind/pybind11>`_ and uses near-identical syntax. In
contrast to these existing tools, nanobind is *more efficient*: bindings
compile in a shorter amount of time, produce smaller binaries, and have better
runtime performance.

More concretely, :ref:`benchmarks <benchmarks>` show **~2-3× faster** compile
time, **~3× smaller** binaries, and up to **~8× lower** runtime overheads
compared to pybind11.

Dependencies
------------

nanobinds depends on

- **Python 3.8+** or **PyPy 7.3.10+** (either the 3.8 or 3.9 PyPy flavors).
- **CMake 3.15+**.
- **A C++17 compiler**: Only Clang 7, GCC 8, MSVC2019 (or newer) are officially
  supported. Others (MinGW, Intel, NVIDIA, ..) may work as well but aren't
  officially supported.

How to cite this project?
-------------------------

Please use the following BibTeX template to cite nanobind in scientific
discourse:

.. code-block:: bibtex

    @misc{nanobind,
       author = {Wenzel Jakob},
       year = {2022},
       note = {https://github.com/wjakob/nanobind},
       title = {nanobind -- Seamless operability between C++17 and Python}
    }


Table of contents
-----------------

.. toctree::
   :maxdepth: 1

   changelog
   why
   benchmark
   porting

.. toctree::
   :caption: The Basics
   :maxdepth: 1

   installing
   building
   basics
   interop
   classes
   packaging

.. toctree::
   :caption: Advanced Topics
   :maxdepth: 1

   ownership
   intrusive
   lowlevel
   typeslots
   tensors

.. toctree::
   :caption: API Reference
   :maxdepth: 1

   api_core
   api_extra
   api_cmake
