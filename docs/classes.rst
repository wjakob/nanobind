.. _classes:

.. cpp:namespace:: nanobind

Classes
=======

The material below builds on the section on :ref:`binding custom types
<binding_types>` and reviews advanced scenarios involving object-oriented code.

Frequently used
---------------
Click on the following :cpp:class:`nb::class_\<..\>::def_* <class_>` members for
examples on how to bind various different kinds of methods, fields, etc.

.. list-table::
  :widths: 40 60
  :header-rows: 1

  * - Type
    - method
  * - Methods & constructors
    - :cpp:func:`.def() <class_::def>`
  * - Fields
    - :cpp:func:`.def_ro() <class_::def_ro>`,
      :cpp:func:`.def_rw() <class_::def_rw>`
  * - Properties
    - :cpp:func:`.def_prop_ro() <class_::def_prop_ro>`,
      :cpp:func:`.def_prop_rw() <class_::def_prop_rw>`
  * - Static methods
    - :cpp:func:`.def_static() <class_::def_static>`
  * - Static fields
    - :cpp:func:`.def_ro_static() <class_::def_ro_static>`,
      :cpp:func:`.def_rw_static() <class_::def_rw_static>`
  * - Static properties
    - :cpp:func:`.def_prop_ro_static() <class_::def_prop_ro_static>`,
      :cpp:func:`.def_prop_rw_static() <class_::def_prop_rw_static>`

.. _inheritance:

Subclasses
----------

Consider the following two data structures with an inheritance relationship:

.. code-block:: cpp

   struct Pet {
       std::string name;
   };

   struct Dog : Pet {
       std::string bark() const { return name + ": woof!"; }
   };

To indicate the inheritance relationship to nanobind, specify the C++ base
class as an extra template parameter of :cpp:class:`nb::class_\<..\> <class_>`:

.. code-block:: cpp
   :emphasize-lines: 8

   #include <nanobind/stl/string.h>

   NB_MODULE(my_ext, m) {
       nb::class_<Pet>(m, "Pet")
          .def(nb::init<const std::string &>())
          .def_rw("name", &Pet::name);

       nb::class_<Dog, Pet /* <- C++ parent type */>(m, "Dog")
           .def(nb::init<const std::string &>())
           .def("bark", &Dog::bark);
   }

Alternatively, you can also pass the type object as an ordinary parameter.

.. code-block:: cpp
   :emphasize-lines: 5

   auto pet = nb::class_<Pet>(m, "Pet")
      .def(nb::init<const std::string &>())
      .def_rw("name", &Pet::name);

   nb::class_<Dog>(m, "Dog", pet /* <- Parent type object */)
       .def(nb::init<const std::string &>())
       .def("bark", &Dog::bark);

Instances expose fields and methods of both types as expected:

.. code-block:: pycon

    >>> d = my_ext.Dog("Molly")
    >>> d.name
    'Molly'
    >>> d.bark()
    'Molly: woof!'

.. _automatic_downcasting:

Automatic downcasting
---------------------

nanobind obeys type signature when returning regular non-polymorphic C++ objects:
building on the :ref:`previous example <inheritance>`, consider the following
function that returns a ``Dog`` object as a ``Pet`` base pointer.

.. code-block:: cpp

   m.def("pet_store", []() { return (Pet *) new Dog{"Molly"}; });

nanobind cannot safely determine that this is in fact an instance of the
``Dog`` subclass. Consequently, only fields and methods of the base type remain
accessible:

.. code-block:: pycon

   >>> p = my_ext.pet_store()
   >>> type(p)
   <class 'my_ext.Pet'>
   >>> p.bark()
   AttributeError: 'Pet' object has no attribute 'bark'

In C++, a type is only considered `polymorphic
<https://en.wikipedia.org/wiki/Dynamic_dispatch>`_ if it (or one of its base
classes) has at least one *virtual function*. Let's add a virtual default
destructor to make ``Pet`` and its subtypes polymorphic.

.. code-block:: cpp

   struct Pet {
       virtual ~Pet() = default;
       std::string name;
   };

With this change, nanobind is able to inspect the returned C++ instance's
`virtual table <https://en.wikipedia.org/wiki/Virtual_method_table>`_ and infer
that it can be represented by a more specialized Python object of type
``my_ext.Dog``.

.. code-block:: pycon

   >>> p = my_ext.pet_store()
   >>> type(p)
   <class 'my_ext.Dog'>
   >>> p.bark()
   'Molly: woof!'

.. note::

   Automatic downcasting of polymorphic instances is only supported when the
   subtype has been registered using :cpp:class:`nb::class_\<..\> <class_>`.
   Otherwise, the return type listed in the function signature takes
   precedence.

.. _overloaded_methods:

Overloaded methods
------------------

Sometimes there are several overloaded C++ methods with the same name taking
different kinds of input arguments:

.. code-block:: cpp

    struct Pet {
        Pet(const std::string &name, int age) : name(name), age(age) { }

        void set(int age_) { age = age_; }
        void set(const std::string &name_) { name = name_; }

        std::string name;
        int age;
    };

Attempting to bind ``Pet::set`` will cause an error since the compiler does not
know which method the user intended to select. We can disambiguate by casting
them to function pointers. Binding multiple functions to the same Python name
automatically creates a chain of function overloads that will be tried in
sequence.

.. code-block:: cpp

   nb::class_<Pet>(m, "Pet")
      .def(nb::init<const std::string &, int>())
      .def("set", nb::overload_cast<int>(&Pet::set), "Set the pet's age")
      .def("set", nb::overload_cast<const std::string &>(&Pet::set), "Set the pet's name");

Here, :cpp:func:`nb::overload_cast <overload_cast>` only requires the parameter
types to be specified, and it deduces the return type.

.. note::

   In cases where a function overloads by ``const``-ness, an additional
   ``nb::const_`` parameter is needed to select the right overload, e.g.,
   ``nb::overload_cast<int>(&Pet::get, nb::const_)``.

   To define overloaded constructors, simply declare one after the other using
   the normal :cpp:class:`.def(nb::init\<...\>()) <init>` syntax.

The overload signatures are also visible in the method's docstring:

.. code-block:: pycon

   >>> help(my_ext.Pet)
   class Pet(builtins.object)
    |  Methods defined here:
    |
    |  __init__(...)
    |      __init__(self, arg0: str, arg1: int, /) -> None
    |
    |  set(...)
    |      set(self, arg: int, /) -> None
    |      set(self, arg: str, /) -> None
    |
    |      Overloaded function.
    |
    |      1. ``set(self, arg: int, /) -> None``
    |
    |      Set the pet's age
    |
    |      2. ``set(self, arg: str, /) -> None``
    |
    |      Set the pet's name

The format of the docstring with a leading overload list followed by a repeated
list with details is designed to be compatible with the `Sphinx
<https://www.sphinx-doc.org/en/master/>`_ documentation generator.

.. _enumerations_and_internal:

Enumerations and internal types
-------------------------------

Let's now suppose that the example class contains internal types like enumerations, e.g.:

.. code-block:: cpp

   struct Pet {
       enum Kind {
           Dog = 0,
           Cat
       };

       struct Attributes {
           float age = 0;
       };

       Pet(const std::string &name, Kind type) : name(name), type(type) { }

       std::string name;
       Kind type;
       Attributes attr;
   };

The binding code for this example looks as follows:

.. code-block:: cpp

   nb::class_<Pet> pet(m, "Pet");

   pet.def(nb::init<const std::string &, Pet::Kind>())
       .def_rw("name", &Pet::name)
       .def_rw("type", &Pet::type)
       .def_rw("attr", &Pet::attr);

   nb::enum_<Pet::Kind>(pet, "Kind")
       .value("Dog", Pet::Kind::Dog)
       .value("Cat", Pet::Kind::Cat)
       .export_values();

   nb::class_<Pet::Attributes>(pet, "Attributes")
       .def(nb::init<>())
       .def_rw("age", &Pet::Attributes::age);

To ensure that the nested types ``Kind`` and ``Attributes`` are created within
the scope of ``Pet``, the ``pet`` type object is passed as the ``scope``
argument of the subsequent :cpp:class:`nb::enum_\<T\> <enum_>` and
:cpp:class:`nb::class_\<T\> <class_>` binding declarations. The
:cpp:func:`.export_values() <enum_::export_values>` function exports the
enumeration entries into the parent scope, which should be skipped for newer
C++11-style strongly typed enumerations.

.. code-block:: pycon

   >>> from my_ext import Pet
   >>> p = Pet("Lucy", Pet.Cat)
   >>> p.attr.age = 3
   >>> p.type
   my_ext.Kind.Cat
   >>> p.type.__name__
   'Cat'
   >>> int(p.type)
   1

.. note::

   When the annotation :cpp:class:`nb::is_arithmetic() <is_arithmetic>` is
   passed to :cpp:class:`nb::enum_\<T\> <enum_>`, the resulting Python type
   will support arithmetic and bit-level operations like comparisons, and, or,
   xor, negation, etc.

   .. code-block:: cpp

       nb::enum_<Pet::Kind>(pet, "Kind", nb::is_arithmetic())
          ...

   By default, these are omitted.

.. _dynamic_attributes:

Dynamic attributes
------------------

Native Python classes can pick up new attributes dynamically:

.. code-block:: pycon

   >>> class Pet:
   ...     name = "Molly"
   ...
   >>> p = Pet()
   >>> p.name = "Charly"  # overwrite existing
   >>> p.age = 2  # dynamically add a new attribute

By default, classes exported from C++ do not support this and the only writable
attributes are the ones explicitly defined using :func:`class_::def_rw`
or :func:`class_::def_prop_rw`.

.. code-block:: cpp

   nb::class_<Pet>(m, "Pet")
       .def(nb::init<>())
       .def_rw("name", &Pet::name);

Trying to set any other attribute results in an error:

.. code-block:: pycon

   >>> p = my_ext.Pet()
   >>> p.name = "Charly"  # OK, attribute defined in C++
   >>> p.age = 2  # fail
   AttributeError: 'Pet' object has no attribute 'age'

To enable dynamic attributes for C++ classes, the :class:`nb::dynamic_attr` tag
must be added to the :class:`nb::class_` constructor:

.. code-block:: cpp

   nb::class_<Pet>(m, "Pet", nb::dynamic_attr())
       .def(nb::init<>())
       .def_rw("name", &Pet::name);

Now everything works as expected:

.. code-block:: pycon

   >>> p = my_ext.Pet()
   >>> p.name = "Charly"  # OK, overwrite value in C++
   >>> p.age = 2  # OK, dynamically add a new attribute

Note that there is a small runtime cost for a class with dynamic attributes.
Not only because of the addition of an instance dictionary, but also because of
more expensive garbage collection tracking which must be activated to resolve
possible circular references. Native Python classes incur this same cost by
default, so this is not anything to worry about. By default, nanobind classes
are more efficient than native Python classes. Enabling dynamic attributes just
brings them on par.

.. _inheriting_in_python:

Extending C++ classes in Python
-------------------------------

Bound C++ types can be extended within Python, which is helpful to dynamically
extend compiled code with further fields and other functionality. Bind classes
with the :cpp:class:`is_final` annotation to forbid subclassing.

Consider the following example bindings of a ``Dog`` and ``DogHouse`` class.

.. code-block:: cpp

   #include <nanobind/stl/string.h>

   namespace nb = nanobind;

   struct Dog {
       std::string name;
       std::string bark() const { return name + ": woof!"; }
   };

   struct DogHouse {
       Dog dog;
   };

   NB_MODULE(my_ext, m) {
       nb::class_<Dog>(m, "Dog")
          .def(nb::init<const std::string &>())
          .def("bark", &Dog::bark)
          .def_rw("name", &Dog::name);

       nb::class_<DogHouse>(m, "DogHouse")
          .def(nb::init<Dog>())
          .def_rw("dog", &DogHouse::dog);
   }

The following Python snippet creates a new ``GuardDog`` type that extends
``Dog`` with an ``.alarm()`` method.

.. code-block:: pycon

   >>> import my_ext
   >>> class GuardDog(my_ext.Dog):
   ...     def alarm(self, count = 3):
   ...         for i in range(count):
   ...             print(self.bark())
   ...
   >>> gd = GuardDog("Max")
   >>> gd.alarm()
   Max: woof!
   Max: woof!
   Max: woof!

This Python subclass is best thought of as a "rich wrapper" around an existing
C++ base object. By default, that wrapper will disappear when nanobind makes a
copy or transfers ownership to C++.

.. code-block:: pycon

   >>> d = my_ext.DogHouse()
   >>> d.dog = gd
   >>> d.dog.alarm()
   AttributeError: 'Dog' object has no attribute 'alarm'

To preserve it, adopt a shared ownership model using :ref:`shared pointers
<shared_ptr>` or :ref:`intrusive reference counting <intrusive_intro>`. For
example, updating the code as follows fixes the problem:

.. code-block:: cpp

   #include <nanobind/stl/shared_ptr.h>

   struct DogHouse {
       std::share_ptr<Dog> dog;
   };

.. code-block:: pycon

   >>> d = my_ext.DogHouse()
   >>> d.dog = gd
   >>> d.dog.alarm()
   Max: woof!
   Max: woof!
   Max: woof!

.. _trampolines:

Overriding virtual functions in Python
--------------------------------------

Building on the previous example on :ref:`inheriting C++ types in Python
<inheriting_in_python>`, let's investigate how a C++ *virtual function* can be
overridden in Python. In the code below, the virtual method ``bark()`` is
called by a global ``alarm()`` function (now written in C++).

.. code-block:: cpp
   :emphasize-lines: 6

   #include <iostream>

   struct Dog {
       std::string name;
       Dog(const std::string &name) : name(name) { }
       virtual std::string bark() const { return name + ": woof!"; }
   };

   void alarm(Dog *dog, size_t count = 3) {
       for (size_t i = 0; i < count; ++i)
           std::cout << dog->bark() << std::endl;
   }

Normally, the binding code would look as follows:

.. code-block:: cpp

   #include <nanobind/stl/string.h>

   namespace nb = nanobind;
   using namespace nb::literals;

   NB_MODULE(my_ext, m) {
       nb::class_<Dog>(m, "Dog")
          .def(nb::init<const std::string &>())
          .def("bark", &Dog::bark)
          .def_rw("name", &Dog::name);

       m.def("alarm", &alarm, "dog"_a, "count"_a = 3);
   }

However, this don't work as expected. We can subclass and override without
problems, but virtual function calls originating from C++ aren't being
propagated to Python:

.. code-block:: pycon

   >>> class ShihTzu(my_ext.Dog):
   ...     def bark(self):
   ...         return self.name + ": yip!"
   ...

   >>> dog = ShihTzu("Mr. Fluffles")

   >>> dog.bark()
   Mr. Fluffles: yip!

   >>> my_ext.alarm(dog)
   Mr. Fluffles: woof!     # <-- oops, alarm() is calling the base implementation
   Mr. Fluffles: woof!
   Mr. Fluffles: woof!

To fix this behavior, you must implement a *trampoline class*. A trampoline has
the sole purpose of capturing virtual function calls in C++ and forwarding them
to Python.

.. code-block:: cpp

   #include <nanobind/trampoline.h>

   struct PyDog : Dog {
       NB_TRAMPOLINE(Dog, 1);

       std::string bark() const override {
           NB_OVERRIDE(bark);
       }
   };

This involves an additional include directive and the line
:c:macro:`NB_TRAMPOLINE(Dog, 1) <NB_TRAMPOLINE>` to mark the class as a
trampoline for the ``Dog`` base type. The count (``1``) denotes to the total
number of virtual method slots that can be overridden within Python.

.. note::

   The number of virtual method slots is used to preallocate memory.
   Trampoline declarations with an insufficient size may eventually trigger a
   Python ``RuntimeError`` exception with a descriptive label, e.g.:

   .. code-block:: text

      nanobind::detail::get_trampoline('PyDog::bark()'): the trampoline ran out of
      slots (you will need to increase the value provided to the NB_TRAMPOLINE() macro)

The macro :c:macro:`NB_OVERRIDE(bark) <NB_OVERRIDE>` intercepts the virtual
function call, checks if a Python override exists, and forwards the call in
that case. If no override was found, it falls back to the base class
implementation.

The macro accepts an variable argument list to pass additional parameters. For
example, suppose that the virtual function ``bark()`` had an additional ``int
volume`` parameter---in that case, the syntax would need to be adapted as follows:

.. code-block:: cpp

       std::string bark(int volume) const override {
           NB_OVERRIDE(bark, volume);
       }


The macro :c:macro:`NB_OVERRIDE_PURE() <NB_OVERRIDE_PURE>` should be used for
pure virtual functions, and :c:macro:`NB_OVERRIDE() <NB_OVERRIDE>` should be
used for functions which have a default implementation.  There are also two
alternate macros :c:macro:`NB_OVERRIDE_PURE_NAME() <NB_OVERRIDE_PURE_NAME>` and
:c:macro:`NB_OVERRIDE_NAME() <NB_OVERRIDE_NAME>` which take a string as first
argument to specify the name of function in Python. This is useful when the C++
and Python versions of the function have different names (e.g., ``operator+``
vs ``__add__``).

The binding code needs a tiny adaptation (highlighted) to inform nanobind of
the trampoline that will be used whenever Python code extends the C++ class.

.. code-block:: cpp

   nb::class_<Dog, PyDog /* <-- trampoline */>(m, "Dog")

If the :cpp:class:`nb::class_\<..\> <class_>` declaration also specifies a base
class, you may specify it and the trampoline in either order. Also, note that
binding declarations should be made against the actual class, not the
trampoline:

.. code-block:: cpp

    nb::class_<Dog, PyDog>(m, "Dog")
       .def(nb::init<const std::string &>())
       .def("bark", &PyDog::bark); /* <--- THIS IS WRONG, use &Dog::bark */

With the trampoline in place, our example works as expected:

.. code-block:: pycon

   >>> my_ext.alarm(dog)
   Mr. Fluffles: yip!
   Mr. Fluffles: yip!
   Mr. Fluffles: yip!
