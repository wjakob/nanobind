.. _refleaks:

.. cpp:namespace:: nanobind

Reference leaks
===============

When the Python interpreter shuts down, nanobind may generate warnings similar
to the following:

.. code-block:: text

   nanobind: leaked 1 instances!
    - leaked instance 0x102123728 of type "my_ext.MyClass"
   nanobind: leaked 1 types!
    - leaked type "my_ext.MyClass"
   nanobind: leaked 1 functions!
    - leaked function "__init__"

   nanobind: this is likely caused by a reference counting issue in the binding code.
   See https://nanobind.readthedocs.io/en/latest/refleaks.html

Reference leaks are the most frequently reported issue in this project—please
read this page carefully before opening a bug report.

If you are a user of an extension experiencing this issue (e.g., ``my_ext`` in the
example above), **do not open a nanobind issue**. Instead, inform the extension
maintainers and direct them to this page.

Why are these warnings generated?
---------------------------------

nanobind registers a callback that runs once Python has fully shut down. If any
nanobind-created instances, functions, or types still exist at this point,
something has gone wrong—they should have been automatically deleted by
Python's garbage collector.

As Python objects can reference significant amounts of memory (e.g.,
large CPU or GPU tensors), an inability to delete them is potentially very bad.

Although leaks aren't always a serious problem, the decision was made to have
nanobind complain noisily about their presence to to encourage early detection
and resolution. Other binding tools do not report leaks, allowing them to
accumulate unnoticed until they cause serious problems.

Disabling leak warnings
-----------------------

Ignorance is bliss. If you prefer to simply not see these messages, you can
easily disable them by calling :cpp:func:`nb::set_leak_warnings()
<set_leak_warnings>` in your binding code:

.. code-block:: cpp

   NB_MODULE(my_ext, m) {
       nb::set_leak_warnings(false);
       // ...
    }

Note that is a *global flag* shared by all nanobind extension libraries in the
same ABI domain. When changing global flags, please isolate your extension from
others by passing the ``NB_DOMAIN`` parameter to the
:cmake:command:`nanobind_add_module()` CMake command:

.. code-block:: cmake

   nanobind_add_module(
     my_module
     NB_DOMAIN my_abi_domain
     extension_file.cpp)

Some projects choose to activate leak warnings for internal builds but disable
them for wheels uploaded to PyPI, as they can be confusing to end users.

Reference counting fundamentals
-------------------------------

Let's begin with some background material to understand the cause of these warnings.

Each Python object tracks its *reference count*—the number of places it is
used. When this count reaches zero, the object is automatically deallocated.

This mechanism is simple and efficient but it fails in the presence of
reference cycles, where objects indirectly references themselves (e.g., A → B → C →
A). In this case, the reference count never reaches zero, and reference
counting alone does not suffice to free the cycle (even if ``A``, ``B``, and
``C`` are not used anywhere else).

In such cases, Python's cyclic *garbage collector* must step in. The garbage
collector periodically sweeps through all Python objects to find and break up
any cycles that are not referenced by other objects. To do its job, it must
know how objects are connected to each other.

For pure Python code, this works seamlessly. Consider the following snippet:

.. code-block:: python

   l = []       # 'l' ref. count = 1
   l.append(l)  # 'l' ref. count = 2
   del l        # 'l' ref. count = 1

Following the last line, the reference count of ``l`` remains ``1`` due to the
self-reference. Python's garbage collector will eventually visit the list and
its elements, identify the cycle, and delete it.

Sources of reference leaks
--------------------------

Under-defined types impede Python's ability to detect cycles, which can causes
leaks. However, user-defined types alone aren't enough---a specific mixture of
ingredients is needed to cause leaks. The following subsections review several
troublesome constructions.

Class members
^^^^^^^^^^^^^

Consider this nanobind extension:

.. code-block:: cpp

   #include <nanobind/nanobind.h>

   namespace nb = nanobind;

   struct Wrapper { nb::object value; };

   NB_MODULE(my_ext, m) {
       nb::class_<Wrapper>(m, "Wrapper")
           .def(nb::init<>())
           .def_rw("value", &Wrapper::value);
   }

Now, run the following Python code.

.. code-block:: pycon

   >>> import my_ext
   >>> w = my_ext.Wrapper()
   >>> w.value = w

This triggers a leak warning:

.. code-block:: text

   nanobind: leaked 1 instances!
    - leaked instance 0x104d63728 of type "my_ext.Wrapper"
   nanobind: leaked 1 types!
    - leaked type "my_ext.Wrapper"
   nanobind: leaked 3 functions!
    - leaked function ""
    - leaked function ""
    - leaked function "__init__"

This resembles the previous example with a self-referential list,
except that a user-defined ``Wrapper`` type is now used instead.

The first message ("*leaked instance*") warns that a Python object of type
``Wrapper`` was not freed during the Python interpreter shutdown. This instance
in turn references other objects, which also become part of the leak:

- ``w`` implicitly references the underlying type object ``my_ext.Wrapper``.

- ``my_ext.Wrapper`` references several methods: ``__init__``, and anonymous
  setter/getter functions.

The root of the problem here is that Python lacks the ability to peek inside
the C++ ``Wrapper`` class to examine its connectivity. Therefore, it cannot
detect and free the cycle.

The fact that we are storing a ``nb::object`` in the C++ instance is
irrelevant---the same issue would have occurred when using
``std::shared_ptr<Wrapper>`` or an intrusively reference-counted object.


Function objects
^^^^^^^^^^^^^^^^

Functions are often a source of reference cycles. Let's reuse the earlier
example but instead assign a local function ``g`` to ``w.value``.

.. code-block:: pycon

   >>> def f():
   ...     w = my_ext.Wrapper()
   ...     def g():
   ...         return w
   ...     w.value = g
   ...
   >>> f()

This code behaves very badly: every call to ``f()`` will leak an uncollectable cycle.

The local function ``g()`` is a `function closure
<https://en.wikipedia.org/wiki/Closure_(computer_programming)>`_. That is to
say, besides being a function, it additionally captures variable state, in this
case the variable ``w``. This creates an inter-language ``Wrapper`` →
``function`` → ``Wrapper`` cycle.

Here is another tricky case: let's move the code back to the top level and
create a dummy function that doesn't reference anything.

.. code-block:: python

   >>> def f():
   ...     pass
   ...
   >>> w = my_ext.Wrapper()
   >>> w.value = f

Given that the function is now empty, we may be tempted to assume that this
should fix the leak. However, this intuition is incorrect:

.. code-block:: text

   nanobind: leaked 1 instances!
    - leaked instance 0x104d63728 of type "my_ext.Wrapper"
   nanobind: leaked 1 types!
    - leaked type "my_ext.Wrapper"
   nanobind: leaked 3 functions!
    - leaked function ""
    - leaked function ""
    - leaked function "__init__"

The reference cycle consists of:

- ``w`` (``Wrapper`` instance) → ``f`` (Python function object).
- ``f`` (Python function object) → ``globals()``.
- ``globals()`` → ``w`` (``Wrapper`` instance).

Functions *implicitly* depend on the global module namespace, which in turn
associates the name ``w`` with the instance. Reference leaks involving globals
can be especially noisy because they can pull in thousands of other objects
that dangle from the uncollectable cycle.

Modifying ``Wrapper`` so that it uses an STL function object does not help.

.. code-block:: cpp

   #include <nanobind/stl/functional.h>

   struct Wrapper {
       std::function<void()> value;
   };

This produces same cycle, just with more layers of indirection:

- ``w`` → ``std::function<void()>`` instance
- ``std::function<void()>`` instance → nanobind function dispatch object
- nanobind function dispatch object → ``f``.
- ``f`` → ``globals()``.
- ``globals()`` → ``w``.

It is easy to encounter such cycles when binding C++ classes with callbacks
that invoke Python functions. An example would be a button class in a GUI
framework that allows the user to assign a button press handler.

Default arguments
^^^^^^^^^^^^^^^^^

Here is another subtle case, where the ``Wrapper`` constructor was modified
to set a default argument.

.. code-block:: cpp

   struct Wrapper { nb::object value; };

   NB_MODULE(my_ext, m) {
       nb::class_<Wrapper>(m, "Wrapper")
           .def(nb::init<Wrapper>() = Wrapper());
   }

Now, we *don't even need to use* the ``Wrapper`` type.

.. code-block:: python

   import my_ext

Its mere presence produces a leak:

.. code-block:: text

   nanobind: leaked 1 instances!
    - leaked instance 0x1035fbb68 of type "my_ext.Wrapper"
   nanobind: leaked 1 types!
    - leaked type "my_ext.Wrapper"
   nanobind: leaked 1 functions!
    - leaked function "__init__"

The reference cycle here is as follows:

- ``my_ext.Wrapper`` type → ``my_ext.Wrapper.__init__`` function
- ``my_ext.Wrapper.__init__`` function → ``my_ext.Wrapper`` instance (the constructed default argument)
- ``my_ext.Wrapper`` instance → ``my_ext.Wrapper`` type (instances implictly reference their type)

Default arguments in general are harmless. However, default arguments that
introduce cycles between instance and type objects can cause uncollectable cycles.

.. _fixing_refleaks:

Fixing reference leaks
----------------------

As the above examples hopefully demonstrate, this can be quite the
minefield---and these were "easy" cycles with only only a few hops. In
practice, leaks can be significantly more complex.

For this reason, it is recommended that you *immediately* investigate and
squash leaks when they occur, especially while things are still under control
(i.e., when there is only a single source of leaks). Start by building your
extension in debug mode, in which case Dr.Jit will exhaustively print warnings
about all leaked instances/type.

Look at the listed types and think about what objects they reference directly
or indirectly. C++ code that stores Python functions (i.e., callbacks) is
especially suspect, since functions can implicitly depend on globals and other
state through theyr closure object. Does a simple ``import`` statement suffice to
cause leaks? This might implicate default function arguments.

Once you have identified a type binding as likely culprit, you must tell Python
how to traverse instances of this type to break cycles. nanobind provides no
abstractions for this at the moment. You must drop down to the CPython API
level and declare two callbacks (referred to as *type slots*):

- ``tp_traverse``: Python's GC will call this function to discover references
  of user-defined types.

- ``tp_clear``: Python's GC will call this function to break collectable cycles.

In particular, *all* types in the cycle must implement the ``tp_traverse``
*type slot*, and *at least one* of them must implement the ``tp_clear`` type
slot.

Here is an example of the required code for a ``Wrapper`` type:

.. code-block:: cpp

   struct Wrapper { std::shared_ptr<Wrapper> value; };

   int wrapper_tp_traverse(PyObject *self, visitproc visit, void *arg) {
       // On Python 3.9+, we must traverse the implicit dependency
       // of an object on its associated type object.
       #if PY_VERSION_HEX >= 0x03090000
           Py_VISIT(Py_TYPE(self));
       #endif

       // The tp_traverse method may be called after __new__ but before or during
       // __init__, before the C++ constructor has been completed. We must not
       // inspect the C++ state if the constructor has not yet completed.
       if (!nb::inst_ready(self)) {
          return 0;
       }

       // Get the C++ object associated with 'self' (this always succeeds)
       Wrapper *w = nb::inst_ptr<Wrapper>(self);

       // If w->value has an associated Python object, return it.
       // If not, value.ptr() will equal NULL, which is also fine.
       nb::handle value = nb::find(w->value);

       // Inform the Python GC about the instance
       Py_VISIT(value.ptr());

       return 0;
   }

   int wrapper_tp_clear(PyObject *self) {
       // Get the C++ object associated with 'self' (this always succeeds)
       Wrapper *w = nb::inst_ptr<Wrapper>(self);

       // Break the reference cycle!
       w->value = {};

       return 0;
   }

   // Table of custom type slots we want to install
   PyType_Slot wrapper_slots[] = {
       { Py_tp_traverse, (void *) wrapper_tp_traverse },
       { Py_tp_clear, (void *) wrapper_tp_clear },
       { 0, 0 }
   };

The types ``visitproc``, ``PyType_Slot``, and macro ``Py_VISIT()`` are part of
the Python C API.

The expression :cpp:func:`nb::inst_ptr\<Wrapper\>(self) <inst_ptr>` efficiently
returns the C++ instance associated with a Python object and is explained in
the documentation about nanobind's :cpp:ref:`low level interface <lowlevel>`.

Note the use of the :cpp:func:`nb::find() <find>` function, which behaves like
:cpp:func:`nb::cast() <cast>` by returning the Python object associated with a
C++ instance. The main difference is that :cpp:func:`nb::cast() <cast>` will
create the Python object if it doesn't exist, while :cpp:func:`nb::find()
<find>` returns a ``nullptr`` object in that case. When given a
``std::function<>`` instance, :cpp:func:`nb::find() <find>` retrieves the
associated Python ``function`` object (if present), which means that the
``wrapper_tp_traverse()`` works for all of the examples shown in this
documentation section.

To activate this machinery, the ``Wrapper`` type bindings must be made aware of
these extra type slots via :cpp:class:`nb::type_slots <type_slots>`:

.. code-block:: cpp

   nb::class_<Wrapper>(m, "Wrapper", nb::type_slots(slots))

With this change, the cycle can be garbage-collected, and the leak warnings
disappear.

.. note::

   When targeting free-threaded Python, it is important that the ``tp_traverse``
   callback does not hold additional references to the objects being traversed.

   A previous version of this documentation page suggested the following

   .. code-block:: cpp

      nb::object value = nb::find(w->value);
      Py_VISIT(value.ptr());

   However, these now have to change to

   .. code-block:: cpp

      nb::handle value = nb::find(w->value);
      Py_VISIT(value.ptr());


Additional sources of leaks
---------------------------

In most of cases, leaks are caused by cycles, and the text above explains
how deal with them. For completeness, let's consider some other possibilities.

- **Reference counting bugs**.  If you write raw Python C API code or use the
  nanobind wrappers including functions like ``Py_[X]INCREF()``,
  ``Py_[X]DECREF()``, :cpp:func:`nb::steal() <steal>`, :cpp:func:`nb::borrow()
  <borrow>`, :cpp:func:`.dec_ref() <detail::api::dec_ref>`,
  :cpp:func:`.inc_ref() <detail::api::inc_ref>`
  , etc., then incorrect
  use of such calls can cause a reference to leak that prevents the associated
  object from being deleted.

- **Interactions with other tools that leak references**. Python extension
  libraries---especially *huge* ones with C library components like PyTorch,
  Tensorflow, etc., have been observed to leak references to nanobind
  objects.

  Some of these frameworks cache JIT-compiled functions based on the arguments
  with which they were called, and such caching schemes could leak references
  to nanobind types if they aren't cleaned up by the responsible extensions
  (this is a hypothesis). In this case, the leak would be benign---even so, it
  should be fixed in the responsible framework so that leak warnings aren't
  cluttered with flukes and can be more broadly useful.

- **Older Python versions**: Very old Python versions (e.g., 3.8) don't
  do a good job cleaning up global references when the interpreter shuts down.
  The following code may leak a reference if it is a top-level statement in a
  Python file or the REPL.

  .. code-block:: python

     a = my_ext.MyObject()

  Such a warning is benign and does not indicate an actual leak. It simply
  highlights a flaws in the interpreter shutdown logic of old Python versions.
  Wrap your code into a function to address this issue even on such versions:

  .. code-block:: python

     def run():
         a = my_ext.MyObject()
         # ...

     if __name__ == '__main__':
         run()

- **Exceptions**. Some exceptions such as ``AttributeError`` have been observed
  to hold references, e.g. to the object which lacked the desired attribute. If
  the last exception raised by the program references a nanobind instance, then
  this may be reported as a leak since Python finalization appears not to
  release the exception object. See `issue #376
  <https://github.com/wjakob/nanobind/issues/376>`__ for a discussion.

