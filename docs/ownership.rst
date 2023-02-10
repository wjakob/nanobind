.. _ownership:

.. cpp:namespace:: nanobind

Object ownership
================

Python and C++ don't manage the lifetime and storage of objects in the same
way. Consequently, two questions arise whenever an object crosses the language
barrier:

- Who actually *owns* this object? C++? Python? Both?!

- Can we safely determine when the object is no longer needed?

This is important: we *must* exclude the possibility that Python destroys an
object that is still being used by C++ (or vice versa).

The :ref:`previous section <exchange>` introduced three ways of exchanging
information between C++ and Python: :ref:`type casters <type_casters>`,
:ref:`bindings <bindings>`, and :ref:`wrappers <wrappers>`. It is specifically
:ref:`bindings <bindings>` for which these two questions must be answered.

.. _ownership_problem:

A problematic example
---------------------

Consider the following example to see what can go wrong:

.. code-block:: cpp

   #include <nanobind/nanobind.h>
   namespace nb = nanobind;

   struct Data { };
   Data data; // Data global variable & function returning a pointer to it
   Data *get_data() { return &data; }

   NB_MODULE(my_ext, m) {
       nb::class_<Data>(m, "Data");

       // KABOOM, calling this function will crash the Python interpreter
       m.def("get_data", &get_data());
   }

The bound function ``my_ext.get_data()`` returns a Python object of type
``my_ext.Data`` that wraps the pointer ``&data`` and takes ownership of it.

When Python eventually garbage collects the object, nanobind will try to free
the (non-heap-allocated) C++ instance via ``operator delete``, causing a
segmentation fault.

To avoid this problem, we can

1. **Provide more information**: the problem was that nanobind *incorrectly*
   transferred ownership of a C++ instance to the Python side. To fix this, we
   can add add a :ref:`return value policy <rvp>` annotation that clarifies
   what to do with the return value.

2. **Make ownership transfer explicit**: C++ types passed via :ref:`unique
   pointers <unique_ptr>` (``std::unique_ptr<T>``) make the ownership transfer
   explicit in the type system, which would have revealed the problem in this
   example.

3. **Switch to shared ownership**: C++ types passed via :ref:`shared pointers
   <shared_ptr>` (``std::shared_ptr<T>``), or which use *intrusive reference
   counting* can be shared by C++ and Python. The whole issue disappears
   because ownership transfer is no longer needed.

The remainder of this section goes through each of these options.

.. _rvp:

Return value policies
---------------------

nanobind provides a several *return value policy* annotations that can be
passed to :func:`module_::def`, :func:`class_::def`, and :func:`cpp_function`.
The default policy is :cpp:enumerator:`rv_policy::automatic`.

In the :ref:`previous example <ownership_problem>`, the policy
:cpp:enumerator:`rv_policy::reference` should have been specified so that the
global data instance is only *referenced* without any implied transfer of
ownership, i.e.:

.. code-block:: cpp

    m.def("get_data", &get_data, py::rv_policy::reference);

On the other hand, this is not the right policy for many other situations,
where ignoring ownership could lead to resource leaks. As a developer using
this library, it is important that you familiarize yourself with the different
options below. In particular, the following policies are available:

- :cpp:enumerator:`rv_policy::take_ownership`:
  Create a thin Python object wrapper around the returned C++ instance without
  making a copy and transfer ownership to Python. When the
  Python wrapper is eventually garbage collected, nanobind will call the C++
  ``delete`` operator to free the C++ instance.

  In the example below, a function uses this policy to return a heap-allocated
  instance and transfer ownership to Python:

  .. code-block:: cpp

     m.def("make_data",
           []() -> Data* { return new Data(); },
           nb::rv_policy::take_ownership)

  The return value policy declaration could actually be omitted here because
  :cpp:enumerator:`take_ownership <rv_policy::take_ownership>` is the default
  for pointer return values (see :cpp:enumerator:`automatic
  <rv_policy::automatic>`).

- :cpp:enumerator:`rv_policy::copy`:
  Copy-construct a new Python object from the C++ instance. The copy will be
  owned by Python, while C++ retains ownership of the original.

  In the example below, a function uses this policy to return a reference to a
  C++ instance. The owner and lifetime of such a reference may not be clear, so
  the safest route is to make a copy.

  .. code-block:: cpp

     struct A {
        B &b() {
            // .. unknown code ..
        }
     };

     m.def("get_b", [](A &a) { return a.b(); }, nb::rv_policy::copy)

  The return value policy declaration could actually be omitted here because
  :cpp:enumerator:`copy <rv_policy::copy>` is the default for lvalue reference
  return values (see :cpp:enumerator:`automatic <rv_policy::automatic>`).

- :cpp:enumerator:`rv_policy::move`:
  Move-construct a new Python object from the C++ instance. The new object will
  be owned by Python, while C++ retains ownership of the original (whose
  contents were likely invalidated by the move operation).

  In the example below, a function uses this policy to return a C++ instance by
  value. The :cpp:enumerator:`copy <rv_policy::copy>` operation mentioned above
  would also be safe to use, but move construction has the potential of being
  significantly more efficient.

  .. code-block:: cpp

     struct A {
        B b() {
            return B{ ... };
        }
     };

     m.def("get_b", [](A &a) { return a.b(); }, nb::rv_policy::move)

  The return value policy declaration could actually be omitted here because
  :cpp:enumerator:`move <rv_policy::move>` is the default for functions that
  return by value (see :cpp:enumerator:`automatic <rv_policy::automatic>`).

- :cpp:enumerator:`rv_policy::reference`:
  Create a thin Python object wrapper around the returned C++ instance without
  making a copy but *do not* transfer ownership to Python. nanobind will never
  call C++ ``delete`` operator, even when the Python wrapper is garbage
  collected. The C++ side is responsible for destructing the C++ instance.

  This return value policy is *dangerous and should be used cautiously*.
  Undefined behavior will ensue when the C++ side deletes the instance while it
  is still being used by Python. If need to use this policy, combine it with a
  :cpp:struct:`keep_alive` function binding annotation to manage the lifetime.
  Or use the simple and safe :cpp:enumerator:`reference_internal
  <rv_policy::reference_internal>` alternative described next.

  Below is an example use of this return value policy to reference a
  global variable that does not need ownership and lifetime management.

  .. code-block:: cpp

     Data data; // This is a global variable

     m.def("get_data", []() { return &data; }, nb::rv_policy::reference)

- :cpp:enumerator:`rv_policy::reference_internal`: A safe extension of the
  :cpp:enumerator:`reference <rv_policy::reference>` policy for methods that
  implement some form of attribute access.

  It creates a thin Python object wrapper around the returned C++ instance
  without making a copy and *does not* transfer ownership to Python.
  Additionally, it adjusts reference counts to keeps the method's implicit
  ``self`` argument alive until the newly created object has been garbage
  collected.

  The example below uses this policy to implement a *getter* function that
  returns a reference to an internal field. Wrapping this getter using
  :cpp:enumerator:`reference_internal <rv_policy::reference_internal>` permits
  mutable access to the field. nanobind ensures that the parent ``A`` instance
  is kept alive until the child ``B`` field reference expires.

  .. code-block:: cpp

      struct A {
          B b;
          B &get_b() { return b; }
      };

      nb::class_<A>(m, "A")
         .def("get_b", &A::get_b, nb::rv_policy::reference_internal);

  More advanced variations of this scheme are also possible using combinations
  of :cpp:enumerator:`reference <rv_policy::reference>` and the
  :cpp:struct:`keep_alive` function binding annotation.

- :cpp:enumerator:`rv_policy::none`: This is the most conservative policy: it
  simply refuses the cast unless the C++ instance already has a corresponding
  Python object, in which case the question of ownership becomes moot.

- :cpp:enumerator:`rv_policy::automatic`: This is the default return value
  policy, which falls back to :cpp:enumerator:`take_ownership
  <rv_policy::automatic>` when the return value is a pointer,
  :cpp:enumerator:`move <rv_policy::move>` when it is a rvalue reference, and
  :cpp:enumerator:`copy <rv_policy::copy>` when it is a lvalue reference.

- :cpp:enumerator:`rv_policy::automatic_reference`: This policy matches
  :cpp:enumerator:`automatic <rv_policy::automatic>` but falls back to
  :cpp:enumerator:`reference <rv_policy::refernece>` when the return value is a
  pointer. It is the default for function arguments when calling Python
  functions from C++ code via :cpp:func:`detail::api::operator()`. You probably
  won't need to use this policy in your own code.

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

.. _shared_ptr:

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
