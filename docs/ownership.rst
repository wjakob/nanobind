.. _ownership:

.. cpp:namespace:: nanobind

Object ownership
================

When nanobind instantiates a C++ type within Python, the resulting instance
is stored *within* the created Python object (henceforth "``PyObject``").
Alternatively, when an already existing C++ instance is transferred to
Python via a function return value and
:cpp:enumerator:`rv_policy::reference`,
:cpp:enumerator:`rv_policy::reference_internal`, or
:cpp:enumerator:`rv_policy::take_ownership`, nanobind creates a smaller
``PyObject`` that only stores a pointer to the instance data.

This is *very different* from pybind11, where the instance ``PyObject``
contained a *holder type* (typically ``std::unique_ptr<T>``) storing a pointer
to the instance data. Dealing with holders caused inefficiencies and introduced
complexity; they were therefore removed in nanobind. This has implications on
object ownership, shared ownership, and interactions with C++ shared/unique
pointers.

Intrusive reference counting
----------------------------

Like pybind11, nanobind provides a way of binding classes with builtin
(“intrusive”) reference counting. This is the most general and cheapest way
of handling shared ownership between C++ and Python, but it requires that
the base class of an object hierarchy is adapted according to the needs of
nanobind. See the :ref:`separate section on intrusive reference counting
<intrusive>` for details.


Shared pointers
---------------

nanobind supports functions that receive and return
``std::shared_ptr<T>``. You must add the include directive

.. code-block:: cpp

   #include <nanobind/stl/shared_ptr.h>

to your binding code in this case.

When calling a C++ function with a ``std::shared_ptr<T>`` argument
from Python, ownership must be shared between Python and C++.
nanobind does this by increasing the reference count of the
``PyObject`` and then creating a ``std::shared_ptr<T>`` with a new
control block containing a custom deleter that will in turn reduce
the Python reference count upon destruction of the shared pointer.

When a C++ function returns a ``std::shared_ptr<T>``, nanobind
checks if the instance already has a ``PyObject`` counterpart
(nothing needs to be done in this case). Otherwise, it indicates
shared ownership by creating a temporary ``std::shared_ptr<T>`` on
the heap that will be destructed when the ``PyObject`` is garbage
collected.

Shared pointers therefore remain usable *despite the lack of
holders*. The approach in nanobind was chosen following on
discussions with `Ralf Grosse-Kunstleve <https://github.com/rwgk>`_;
it is unusual in that multiple ``shared_ptr`` control blocks are
potentially allocated for the same object, which means that
``std::shared_ptr<T>::use_count()`` generally won’t show the true
global reference count.

.. _enable_shared_from_this:

Limitations
^^^^^^^^^^^

nanobind refuses conversion of classes that derive from
``std::enable_shared_from_this<T>``. This is a fundamental limitation:
nanobind instances do not create a base shared pointer that declares
ownership of an object. Other parts of a C++ codebase might then incorrectly
assume ownership and eventually try to ``delete`` a nanobind instance
allocated using ``pymalloc`` (which is undefined behavior). A compile-time
assertion catches this and warns about the problem.

Replacing shared pointers with :ref:`intrusive reference counting
<intrusive>` fixes this limitations.

.. _unique_ptr:

Unique pointers
---------------

nanobind supports functions that receive and return
``std::unique_ptr<T, Deleter>``. You must add the include directive

.. code-block:: cpp

   #include <nanobind/stl/unique_ptr.h>

to your binding code in this case.

Whereas ``std::shared_ptr<T>`` could abstract over details concerning
storage and the deletion mechanism, this is not possible in simpler
``std::unique_ptr``, which means that some of those details leak into
the type signature.

When calling a C++ function with a ``std::unique_ptr<T, Deleter>``
argument from Python, there is an ownership transfer from Python to
C++ that must be handled.

- When ``Deleter`` is ``std::default_delete<T>`` (i.e., the default
  when no ``Deleter`` is specified), this ownership transfer is only
  possible when the instance was originally created by a *new expression*
  within C++ and nanobind has taken over ownership (i.e., it was created by
  a function returning a raw pointer ``T *value`` with
  ``rv_policy::take_ownership``, or a function returning a
  ``std::unique_ptr<T>``). This limitation exists because the ``Deleter``
  will execute the statement ``delete value`` when the unique pointer
  expires, causing undefined behavior when the object was allocated within
  Python. nanobind detects this and refuses such unsafe conversions with a
  warning.

- To enable ownership transfer under all conditions, nanobind
  provides a custom ``Deleter`` named :cpp:class:`nb::deleter\<T\>
  <deleter>` that uses reference counting to keep the underlying
  ``PyObject`` alive during the lifetime of the unique pointer. Following
  this route requires changing function signatures so that they use
  ``std::unique_ptr<T, nb::deleter<T>>`` instead of ``std::unique_ptr<T>``.
  This custom deleter supports ownership by both C++ and Python and can be
  used in all situations.

In both cases, a Python object may continue to exist after ownership was
transferred to C++ side. nanobind marks this object as *invalid*: any
operations involving it will fail with a ``TypeError``. Reverse ownership
transfer at a later point will make it usable again.

Binding functions that return a ``std::unique_ptr<T, Deleter>`` always
works: nanobind will then (re-)acquire ownership of the object.

Deleters other than ``std::default_delete<T>`` or ``nb::deleter<T>`` are
*not supported*.
