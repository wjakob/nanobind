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
   Traceback (most recent call last):
     File "<stdin>", line 1, in <module>
   ImportError: dynamic module does not define module export function (PyInit_my_ext)

Why are mutable reference arguments not being updated?
------------------------------------------------------

Functions like the following example can be exposed in Python, but they won't
propagate updates to their arguments.

.. code-block:: stl

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

   If your extension consistes of multiple source code files that involve
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

How can I reduce the build time?
--------------------------------

It's good practice to split binding code over multiple files, as in the
following example:

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
