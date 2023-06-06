.. cpp:namespace:: nanobind

.. _faq:

Frequently asked questions
==========================

Importing my module fails with an ``ImportError``
-------------------------------------------------

If importing the module fails as shown below, you have not specified a
matching module name in :cmake:command:`nanobind_add_module()` and
:c:macro:`NB_MODULE() <NB_MODULE>`.

.. code-block:: pycon

   >>> import my_ext
   ImportError: dynamic module does not define module export function (PyInit_my_ext)

Importing fails due to missing ``[lib]nanobind.{dylib,so,dll}``
---------------------------------------------------------------

If importing the module fails as shown below, the extension cannot find the
``nanobind`` shared library component.

.. code-block:: pycon

   >>> import my_ext
   ImportError: dlopen(my_ext.cpython-311-darwin.so, 0x0002):
   Library not loaded: '@rpath/libnanobind.dylib'

This is really more of a general C++/CMake/build system issue than one of
nanobind specifically. There are two solutions:

1. Build the library component statically by specifying the ``NB_STATIC`` flag
   in :cmake:command:`nanobind_add_module()` (this is the default starting with
   nanobind 0.2.0).

2. Ensure that the various shared libraries are installed in the right
   destination, and that their `rpath <https://en.wikipedia.org/wiki/Rpath>`_
   is set so that they can find each other.

   You can control the build output directory of the shared library component
   using the following CMake command:

   .. code-block:: pycon

      set_target_properties(nanobind
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY                <path>
        LIBRARY_OUTPUT_DIRECTORY_RELEASE        <path>
        LIBRARY_OUTPUT_DIRECTORY_DEBUG          <path>
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO <path>
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     <path>
      )

   Depending on the flags provided to :cmake:command:`nanobind_add_module()`,
   the shared library component may have a different name following the pattern
   ``nanobind[-abi3][-lto]``.

   The following CMake commands may be useful to adjust the build and install
   `rpath <https://en.wikipedia.org/wiki/Rpath>`_ of the extension:

   .. code-block:: cmake

      set_property(TARGET my_ext APPEND PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:nanobind>")
      set_property(TARGET my_ext APPEND PROPERTY INSTALL_RPATH ".. ?? ..")


Why are reference arguments not updated?
----------------------------------------

Functions like the following example can be exposed in Python, but they won't
propagate updates to mutable reference arguments.

.. code-block:: cpp

   void increment(int &i) {
       i++;
   }

This isn't specific to builtin types but also applies to STL collections and
other types when they are handled using :ref:`type casters <type_casters>`.
Please read the full section on :ref:`information exchange between C++ and
Python <exchange>` to understand the issue and alternatives.

Compilation fails with a static assertion mentioning ``NB_MAKE_OPAQUE()``
-------------------------------------------------------------------------

If your compiler generates an error of the following sort, you are mixing type
casters and bindings in a way that has them competing for the same types:

.. code-block:: text

   nanobind/include/nanobind/nb_class.h:207:40: error: static assertion failed: ↵
   Attempted to create a constructor for a type that won't be  handled by the nanobind's ↵
   class type caster. Is it possible that you forgot to add NB_MAKE_OPAQUE() somewhere?

For example, the following won't work:

.. code-block:: cpp

   #include <nanobind/stl/vector.h>
   #include <nanobind/stl/bind_vector.h>

   namespace nb = nanobind;

   NB_MODULE(my_ext, m) {
       // The following line cannot be compiled
       nb::bind_vector<std::vector<int>>(m, "VectorInt");

       // This doesn't work either
       nb::class_<std::vector<int>>(m, "VectorInt");
   }

This is not specific to STL vectors and will happen whenever casters and
bindings target overlapping types.

:ref:`Type casters <type_casters>` employ a pattern matching technique known as
`partial template specialization
<https://en.wikipedia.org/wiki/Partial_template_specialization>`_. For example,
``nanobind/stl/vector.h`` installs a pattern that detects *any* use of
``std::vector<T, Allocator>``, which overlaps with the above binding of a specific
vector type.

The deeper reason for this conflict is that type casters enable a
*compile-time* transformation of nanobind code, which can conflict with
binding declarations that are a *runtime* construct.

To fix the conflict in this example, add the line :c:macro:`NB_MAKE_OPAQUE(T)
<NB_MAKE_OPAQUE>`, which adds another partial template specialization pattern
for ``T`` that says: "ignore ``T`` and don't use a type caster to handle it".

.. code-block:: cpp

   NB_MAKE_OPAQUE(std::vector<int>);

.. warning::

   If your extension consists of multiple source code files that involve
   overlapping use of type casters and bindings, you are *treading on thin
   ice*. It is easy to violate the *One Definition Rule* (ODR) [`details
   <https://en.wikipedia.org/wiki/One_Definition_Rule>`_] in such a case, which
   may lead to undefined behavior (miscompilations, etc.).

   Here is a hypothetical example of an ODR violation: an extension
   contains two source code files: ``src_1.cpp`` and ``src_2.cpp``.

   - ``src_1.cpp`` binds a function that returns an ``std::vector<int>`` using
     a :ref:`type caster <type_casters>` (``nanobind/stl/vector.h``).

   - ``src_2.cpp`` binds a function that returns an ``std::vector<int>`` using
     a :ref:`binding <bindings>` (``nanobind/stl/bind_vector.h``), and it also
     installs the needed type binding.

   The problem is that a partially specialized class in the nanobind
   implementation namespace (specifically,
   ``nanobind::detail::type_caster<std::vector<int>>``) now resolves to *two
   different implementations* in the two compilation units. It is unclear how
   such a conflict should be resolved at the linking stage, and you should
   consider code using such constructions broken.

   To avoid this issue altogether, we recommend that you create a single
   include file (e.g., ``binding_core.h``) containing all of the nanobind
   include files (binding, type casters), your own custom type casters (if
   present), and :c:macro:`NB_MAKE_OPAQUE(T) <NB_MAKE_OPAQUE>` declarations.
   Include this header consistently in all binding compilation units. The
   construction shown in the example (mixing type casters and bindings for the
   same type) is not allowed, and cannot occur when following the
   recommendation.

How can I preserve the ``const``-ness of values in bindings?
------------------------------------------------------------

This is a limitation of nanobind, which casts away ``const`` in function
arguments and return values. This is in line with the Python language, which
has no concept of const values. Additional care is therefore needed to avoid
bugs that would be caught by the type checker in a traditional C++ program.

How can I reduce build time?
----------------------------

Large binding projects should be partitioned into multiple files, as shown in
the following example:

:file:`example.cpp`:

.. code-block:: cpp

    void init_ex1(nb::module_ &);
    void init_ex2(nb::module_ &);
    /* ... */

    NB_MODULE(my_ext, m) {
        init_ex1(m);
        init_ex2(m);
        /* ... */
    }

:file:`ex1.cpp`:

.. code-block:: cpp

    void init_ex1(nb::module_ &m) {
        m.def("add", [](int a, int b) { return a + b; });
    }

:file:`ex2.cpp`:

.. code-block:: cpp

    void init_ex2(nb::module_ &m) {
        m.def("sub", [](int a, int b) { return a - b; });
    }

:command:`python`:

.. code-block:: pycon

    >>> import example
    >>> example.add(1, 2)
    3
    >>> example.sub(1, 1)
    0

As shown above, the various ``init_ex`` functions should be contained in
separate files that can be compiled independently from one another, and then
linked together into the same final shared object.  Following this approach
will:

1. reduce memory requirements per compilation unit.

2. enable parallel builds (if desired).

3. allow for faster incremental builds. For instance, when a single class
   definition is changed, only a subset of the binding code will generally need
   to be recompiled.

Nanobind cannot pass instances of my type in a multi-library/extension project
------------------------------------------------------------------------------

Suppose that nanobind unexpectedly raises a ``TypeError`` when passing or
returning an instance of a bound type. There is usually a simple explanation:
the type (let's call it "``Foo``") is defined in a library compiled separately
from the main nanobind extension (let's call it ``libfoo``). The problem can
also arise when there are multiple extension libraries that all make use of
``Foo``.

The problem is that the runtime type information ("RTTI") describing ``Foo`` is
is not synchronized among these different libraries, at which point it appears
to nanobind that there are multiple identically named but distinct types called
``Foo``. The dynamic linker is normally responsible for merging the RTTI
records, but it can only do so when the shared library exports them correctly.

On Windows you must specify a DLL export/import annotation, and on other
platforms it suffices to raise the visibility of the associated symbols.

.. code-block:: cpp

   /* TODO: Change 'MYLIB' to the name of your project. It's probably best to put
      these into a common header file included by all parts of the project */
   #if defined(_WIN32)
   #  define MYLIB_EXPORT __declspec(dllexport)
   #  define MYLIB_IMPORT __declspec(dllimport)
   #else
   #  define MYLIB_EXPORT __attribute__ ((visibility("default")))
   #  define MYLIB_IMPORT __attribute__ ((visibility("default")))
   #endif

   #if defined(MYLIB_BUILD)
   #  define MYLIB_API MYLIB_EXPORT
   #else
   #  define MYLIB_API MYLIB_IMPORT
   #endif

   /// Important: annotate the Class declaration with MYLIB_API
   class MYLIB_API Foo {
       // ... Foo definitions ..
   };

In the CMake build system, you must furthermore specify the ``-DMYLIB_BUILD``
definition so that symbols are exported when building ``libfoo`` and imported
by consumers of ``libfoo``.

.. code-block:: cmake

   target_compile_definitions(libfoo PRIVATE MYLIB_BUILD)

.. _type-visibility:

How can I avoid conflicts with other projects using nanobind?
-------------------------------------------------------------

Suppose that a type binding in your project conflicts with another extension, for
example because both expose a common type (e.g., ``std::latch``). nanobind will
warn whenever it detects such a conflict:

.. code-block:: text

  RuntimeWarning: nanobind: type 'latch' was already registered!

In the worst case, this could actually break both packages (especially if the
bindings of the two packages expose an inconsistent/incompatible API).

The higher-level issue here is that nanobind will by default try to make type
bindings visible across extensions because this is helpful to partition large
binding projects into smaller parts. Such information exchange requires that
the extensions:

- use the same nanobind *ABI version* (see the :ref:`Changelog <changelog>` for details).
- use the same compiler (extensions built with GCC and Clang are isolated from each other).
- use ABI-compatible versions of the C++ library.
- use the stable ABI interface consistently (stable and unstable builds are isolated from each other).
- use debug/release mode consistently (debug and release builds are isolated from each other).

In addition, nanobind provides a feature to intentionally scope extensions to a
named domain to avoid conflicts with other extensions. To do so, specify the
``NB_DOMAIN`` parameter in CMake:

.. code-block:: cmake

   nanobind_add_module(my_ext
                       NB_DOMAIN my_project
                       my_ext.cpp)

In this case, inter-extension type visibility is furthermore restricted to
extensions in the ``"my_project"`` domain.

How to cite this project?
-------------------------

Please use the following BibTeX template to cite nanobind in scientific
discourse:

.. code-block:: bibtex

    @misc{nanobind,
       author = {Wenzel Jakob},
       year = {2022},
       note = {https://github.com/wjakob/nanobind},
       title = {nanobind---Seamless operability between C++17 and Python}
    }
