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

Inheritance
-----------

Consider the following two data structures with an inheritance relationship:

.. code-block:: cpp

    struct Pet {
        std::string name;
    };

    struct Dog : Pet {
        std::string bark() const { return "woof!"; }
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
    'woof!'

nanobind obeys type signature when returning C++ objects: consider the
following function that returns a ``Dog`` object as a ``Pet`` base pointer.

.. code-block:: cpp

    m.def("pet_store", []() { return (Pet *) new Dog{"Molly"}; });

Only fields and methods of the base type remain accessible.

.. code-block:: pycon

    >>> p = my_ext.pet_store()
    >>> type(p)
    <class 'my_ext.Pet'>
    >>> p.bark()
    AttributeError: 'Pet' object has no attribute 'bark'

This is the case even when the returned instance is *polymorphic* (a deviation
from pybind11).

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

.. note::

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
    >>> p.__dict__  # just like a native Python class
    {'age': 2}

Note that there is a small runtime cost for a class with dynamic attributes.
Not only because of the addition of a ``__dict__``, but also because of more
expensive garbage collection tracking which must be activated to resolve
possible circular references. Native Python classes incur this same cost by
default, so this is not anything to worry about. By default, nanobind classes
are more efficient than native Python classes. Enabling dynamic attributes
just brings them on par.
