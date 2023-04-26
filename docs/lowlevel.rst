.. _lowlevel:

.. cpp:namespace:: nanobind

Low-level interface
===================

nanobind exposes a low-level interface to provide fine-grained control over
the sequence of steps that instantiates a Python object wrapping a C++
instance. This is useful when writing generic binding code that manipulates
nanobind-based objects of various types.

Given a previous :cpp:class:`nb::class_\<...\> <class_>` binding declaration,
this interface can be used to look up the Python type object associated with
a C++ class named ``MyClass``.

.. code-block:: cpp

   nb::handle py_type = nb::type<MyClass>();

In the case of failure, this line will return a ``nullptr`` pointer, which
can be checked via ``py_type.is_valid()``. We can verify that the type
lookup succeeded, and that the returned instance indeed represents a
nanobind-owned type (via :cpp:func:`nb::type_check() <type_check>`, which is
redundant in this case):

.. code-block:: cpp

   assert(py_type.is_valid() && nb::type_check(py_type));

nanobind knows the size, alignment, and C++ RTTI ``std::type_info``
record of all bound types. They can be queried on the fly in situations
where this is useful.

.. code-block:: cpp

   assert(nb::type_size(py_type) == sizeof(MyClass) &&
          nb::type_align(py_type) == alignof(MyClass) &&
          nb::type_info(py_type) == typeid(MyClass));

Given a type object representing a C++ type, we can create an uninitialized
instance via :cpp:func:`nb::inst_alloc() <inst_alloc>`. This is an ordinary
Python object that can, however, not (yet) be passed to bound C++ functions
to prevent undefined behavior. It must first be initialized.

.. code-block:: cpp

   nb::object py_inst = nb::inst_alloc(py_type);

We can confirm that this newly created instance is managed by nanobind,
that it has the correct type in Python, and that it is not ``ready``
(i.e. uninitialized):

.. code-block:: cpp

   assert(nb::inst_check(py_inst) &&
          py_inst.type().is(py_type) &&
          !nb::inst_ready(py_inst));

For simple *plain old data* (POD) types, the :cpp:func:`nb::inst_zero()
<inst_zero>` function can be used to zero-initialize the object and mark it
as ready.

.. code-block:: cpp

   nb::inst_zero(py_inst);
   assert(nb::inst_ready(py_inst));

We can destruct this default instance and convert it back to non-ready
status. This memory region can then be reinitialized once more.

.. code-block:: cpp

   nb::inst_destruct(py_inst);
   assert(!nb::inst_ready(py_inst));

What follows is a more interesting example, where we use a lesser-known feature
of C++ (the "`placement new <https://en.wikipedia.org/wiki/Placement_syntax>`_"
operator) to construct an instance *in-place* into the memory region allocated
by nanobind.

.. code-block:: cpp

   // Get a C++ pointer to the uninitialized instance data
   MyClass *ptr = nb::inst_ptr<MyClass>(py_inst);

   // Perform an in-place construction of the C++ object at address 'ptr'
   new (ptr) MyClass(/* constructor arguments go here */);

Following this constructor call, we must inform nanobind that the instance
object is now fully constructed. When its reference count reaches zero,
nanobind will automatically call the in-place destructor
(``MyClass::~MyClass``).

.. code-block:: cpp

   nb::inst_mark_ready(py_inst);
   assert(nb::inst_ready(py_inst));

Let’s destroy this instance once more manually (which will, again, call
the C++ destructor and mark the Python object as non-ready).

.. code-block:: cpp

   nb::inst_destruct(py_inst);

Another useful feature is that nanobind can copy- or move-construct ``py_inst``
from another instance of the same type. This calls the C++ copy or move
constructor and transitions ``py_inst`` back to ``ready`` status. Note that
this is equivalent to calling an in-place version of these constructors above
but compiles to more compact code (the :cpp:class:`nb::class_\<MyClass\>
<class_>` declaration had already created bindings for both constructors, and
this simply calls those bindings).

.. code-block:: cpp

   if (copy_instance)
       nb::inst_copy(/* dst = */ py_inst, /* src = */ some_other_instance);
   else
       nb::inst_move(/* dst = */ py_inst, /* src = */ some_other_instance);

Note that these functions are all *unsafe* in the sense that they do not
verify that their input arguments are valid. This is done for
performance reasons, and such checks (if needed) are therefore the
responsibility of the caller. Functions labeled ``nb::type_*`` should
only be called with nanobind type objects, and functions labeled
``nb::inst_*`` should only be called with nanobind instance objects.

The functions :cpp:func:`nb::type_check() <type_check>` and
:cpp:func:`nb::inst_check() <inst_check>` are exceptions to this rule: they
accept any Python object and test whether something is a nanobind type or
instance object.

Even lower-level interface
--------------------------

Every nanobind object has two important flags that control its behavior:

1. ``ready``: is the object fully constructed? If set to ``false``,
   nanobind will raise an exception when the object is passed to a bound
   C++ function.

2. ``destruct``: Should nanobind call the C++ destructor when the
   instance is garbage collected?

The functions :cpp:func:`nb::inst_zero() <inst_zero>`,
:cpp:func:`nb::inst_mark_ready() <inst_mark_ready>`, :cpp:func:`nb::inst_move()
<inst_move>`, and :cpp:func:`nb::inst_copy() <inst_copy>` set both of these
flags to ``true``, and :cpp:func:`nb::inst_destruct() <inst_destruct>` sets
both of them to ``false``.

In rare situations, the destructor should *not* be invoked when the
instance is garbage collected, for example when working with a nanobind
instance representing a field of a parent instance created using the
:cpp:enumerator:`nb::rv_policy::reference_internal
<rv_policy::reference_internal>` return value policy. The library therefore
exposes two more functions that can be used to read/write these two flags
individually.

.. code-block:: cpp

   void inst_set_state(handle h, bool ready, bool destruct);
   std::pair<bool, bool> inst_state(handle h);

Referencing existing instances
------------------------------

The above examples used the function :cpp:func:`nb::inst_alloc() <inst_alloc>`
to allocate a Python object along with space to hold a C++ instance associated
with the binding ``py_type``.

.. code-block:: cpp

   nb::object py_inst = nb::inst_alloc(py_type);

   // Next, perform a C++ in-place construction into the
   // address given by nb::inst_ptr<MyClass>(py_inst)
   ... omitted, see the previous examples ...

What if the C++ instance already exists? nanobind also supports this case via
the :cpp:func:`nb::inst_wrap() <inst_wrap>` function—in this case, the Python
object references the existing memory region, which is potentially (slightly)
less efficient due to the need for an extra indirection.

.. code-block:: cpp

   MyClass *inst = new MyClass();
   nb::object py_inst = nb::inst_wrap(py_type, inst);

   // Mark as ready, garbage-collecting 'py_inst' will cause 'inst' to be
   // deleted as well. Call nb::inst_set_state (documented above) for more
   // fine-grained control.
   nb::inst_mark_ready(py_inst);

.. _supplement:

Supplemental type data
----------------------

nanobind can stash supplemental data *inside* the type object of bound types.
This involves the :cpp:class:`nb::supplement\<T\>() <supplement>` class binding
annotation to reserve space and :cpp:func:`nb::type_supplement\<T\>()
<type_supplement>` to access the reserved memory region.

An example use of this fairly advanced feature are libraries that register
large numbers of different types (e.g. flavors of tensors). A single
generically implemented function can then query the supplemental data block to
handle each tensor type slightly differently.

Here is what this might look like in an implementation:

.. code-block:: cpp

  struct MyTensorMetadata {
      bool stored_on_gpu;
      // ..
      // should be a POD (plain old data) type
  };

  // Register a new type MyTensor, and reserve space for sizeof(MyTensorMedadata)
  nb::class_<MyTensor> cls(m, "MyTensor", nb::supplement<MyTensorMedadata>(), nb::is_final())

  /// Mutable reference to 'MyTensorMedadata' portion in Python type object
  MyTensorMedadata &supplement = nb::type_supplement<MyTensorMedadata>(cls);
  supplement.stored_on_gpu = true;

The :cpp:class:`nb::supplement\<T\>() <supplement>` annotation implicitly
also passes :cpp:class:`nb::is_final() <is_final>` to ensure that type
objects with supplemental data cannot be subclassed in Python.

.. _instance_supplement:

Supplemental instance data
--------------------------

It is also possible to store supplemental data inside each *instance* of
a bound type. For performance reasons, an instance supplement must be
trivially copyable and must be no larger and no more aligned than a pointer.

To reserve space for an instance supplement, pass the
:cpp:class:`nb::instance_supplement() <instance_supplement>`
annotation when binding a class; you can then obtain a reference to
the supplemental data (interpreted as an object of type ``T``) by
calling :cpp:func:`nb::inst_supplement\<T\>() <inst_supplement>` on an
instance of that class.  The instance supplement is zero-initialized
whenever a new Python instance is constructed, either due to an object
being created from Python or due to wrapping a pointer returned from
C++. The instance supplement is always stored within the Python object
instance, even if the C++ object is not.

For example, this feature can be used to cache the hash value of an
immutable instance:

.. code-block:: cpp

  struct ExpensiveObject { size_t compute_hash() const; /* ... */ };
  nb::class_<ExpensiveObject> cls(m, "ExpensiveObject", nb::instance_supplement());
  cls.def("__hash__", [](nb::handle_t<ExpensiveObject> self) {
      auto& cached_hash = nb::inst_supplement<size_t>(self);
      if (cached_hash == 0) {
          if (!nb::inst_ready(self)) {
              throw nb::type_error("Instance is not initialized!");
          }
          cached_hash = nb::inst_ptr<ExpensiveObject>(self)->compute_hash();
          if (cached_hash == 0) {
              cached_hash = 1;
          }
      }
      return static_cast<Py_ssize_t>(cached_hash) - 1;
  });

Or to cache a string associated with the instance:

.. code-block:: cpp

  struct ExpensiveObject { std::string stringify() const; /* ... */ };
  nb::class_<ExpensiveObject> cls(m, "ExpensiveObject", nb::instance_supplement());
  cls.def("__str__", [](nb::handle_t<ExpensiveObject> self) {
      auto& cached_result = nb::inst_supplement<nb::handle>(self);
      if (!cached_result.is_valid()) {
          if (!nb::inst_ready(self)) {
              throw nb::type_error("Instance is not initialized!");
          }
          nb::object result = nb::cast(
                  nb::inst_ptr<ExpensiveObject>(self)->stringify());
          cached_result = result;
          return result;
      } else {
          return nb::borrow(cached_result);
      }
  }, nb::keep_alive<1, 0>());

In the latter example, note that the instance supplement stores a non-owning
:cpp:class:`nb::handle <handle>`; this is required because nanobind doesn't
run any destructor for the instance supplement when the instance is destroyed.
The :cpp:class:`nb::keep_alive <keep_alive>` function annotation is used to
ensure that the string stays alive for as long as the ``ExpensiveObject`` does.

.. warning:: If an instance supplement is used to store a Python
             object reference in this way, it will not be visible to
             the garbage collector, so if it participates in a
             reference cycle (by indirectly referring back to the instance
             itself) you will get a reference leak. If you really need to
             cache a Python object, stick to simple types
             such as ``str`` that can't participate in cycles.
