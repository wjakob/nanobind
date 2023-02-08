.. _intrusive:

.. cpp:namespace:: nanobind

Intrusive reference counting
============================

Nanobind provides a custom intrusive reference counting solution that is
both efficient and also completely solves the issue of shared C++/Python
object ownership. It addresses :ref:`annoying corner cases
<enable_shared_from_this>` that exist with shared pointers.

The main limitation is that it requires adapting the base class of an object
hierarchy according to the needs of nanobind, which may not always be possible.

Motivation
^^^^^^^^^^

Consider the following simple class with intrusive reference counting:

.. code-block:: cpp

   class Object {
   public:
       void inc_ref() const noexcept { ++m_ref_count; }

       void dec_ref() const noexcept {
           if (--m_ref_count == 0)
               delete this;
       }

   private:
       mutable std::atomic<size_t> m_ref_count { 0 };
   };

It contains an atomic counter that stores the number of references. When the
counter reaches zero, the object deallocates itself. Easy and efficient.

The advantage of over ``std::shared_ptr<T>`` is that no separate control block
must be allocated. Technical band-aids like ``std::enable_shared_from_this<T>``
can also be avoided, since the reference count is always found in the object
itself.

However, one issue that tends to arise when a type like ``Object`` is
wrapped using nanobind is that there are now *two* separate reference counts
referring to the same object: one in Python’s ``PyObject``, and one in
``Object``. This can lead to a problematic reference cycle:

- Python’s ``PyObject`` needs to keep the ``Object`` instance alive so that it
  can be safely passed to C++ functions.

- The C++ ``Object`` may in turn need to keep the ``PyObject`` alive. This
  is the case when a subclass uses *tramponlines* (:c:macro:`NB_TRAMPOLINE`,
  :c:macro:`NB_OVERRIDE`) to catch C++ virtual function calls and
  potentially dispatch them to an overridden implementation in Python. In
  this case, the C++ instance needs to be able to perform a function call on
  its own Python object identity, which requires a reference.

The source of the problem is that there are *two* separate counters that try
to reason about the reference count of *one* instance, which leads to an
uncollectable inter-language reference cycle.

The solution
^^^^^^^^^^^^
We can solve the problem by using just one counter:

- if an instance lives purely on the C++ side, the ``m_ref_count``
  field is used to reason about the number of references.

- The first time that an instance is exposed to Python (by being
  created from Python, or by being returned from a bound C++ function),
  lifetime management switches over to Python.

The files `tests/object.h
<https://github.com/wjakob/nanobind/blob/master/tests/object.h>`_ and
`tests/object.cpp
<https://github.com/wjakob/nanobind/blob/master/tests/object.cpp>`_ contain an
example implementation of a suitable base class named ``Object``. It contains
an extra optimization to use a single field of type ``std::atomic<uintptr_t>``
(8 bytes) to store *either* a reference counter or a pointer to a
``PyObject*``. The example class is designed to work even when used in a
context where Python is not available.

The main change in the bindings is that the base class must specify a
:cpp:class:`nb::intrusive_ptr <intrusive_ptr>` annotation to inform an instance
that lifetime management has been taken over by Python. This annotation is
automatically inherited by all subclasses. In the linked example, this is done
via the ``Object::set_self_py()`` method that we can now call from the class
binding annotation.

.. code-block:: cpp

   nb::class_<Object>(
       m, "Object",
       nb::intrusive_ptr<Object>(
           [](Object *o, PyObject *po) noexcept { o->set_self_py(po); }));

That's it. If you use this approach, any potential issues involving shared
pointers, return value policies, reference leaks with trampolines, etc., can
be avoided from the beginning.
