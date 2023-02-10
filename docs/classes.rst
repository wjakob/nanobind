.. _classes:

.. cpp:namespace:: nanobind

Object-oriented code
====================

.. note::

   This section is adapted from `pybind11
   <https://pybind11.readthedocs.io/en/stable/advanced/classes.html>`_ since it
   uses the same API.


Keyword and default arguments
-----------------------------

It is possible to specify keyword and default arguments as before. Refer to the
:ref:`previous discussion <keyword_and_default_args>`` for details.

Binding lambda functions
------------------------

Note how ``print(p)`` produced a rather useless summary of our data structure in the example above:

.. code-block:: pycon

    >>> print(p)
    <my_ext.Pet object at 0x10cd98060>

To address this, we could bind a utility function that returns a human-readable
summary to the special method slot named ``__repr__``. Unfortunately, there is no
suitable functionality in the ``Pet`` data structure, and it would be nice if
we did not have to change it. This can easily be accomplished by binding a
Lambda function instead:

.. code-block:: cpp

        nb::class_<Pet>(m, "Pet")
            .def(nb::init<const std::string &>())
            .def("set_name", &Pet::set_name)
            .def("name", &Pet::name)
            .def("__repr__",
                [](const Pet &a) {
                    return "<my_ext.Pet named '" + a.name + "'>";
                }
            );

Both stateless [#f1]_ and stateful lambda closures are supported by nanobind.
With the above change, the same Python code now produces the following output:

.. code-block:: pycon

    >>> print(p)
    <my_ext.Pet named 'Molly'>

.. [#f1] Stateless closures are those with an empty pair of brackets ``[]`` as the capture object.

.. _properties:

Instance and static fields
--------------------------

We can also directly expose the ``name`` field using the
:func:`class_::def_readwrite` method. A similar :func:`class_::def_readonly`
method also exists for ``const`` fields.

.. code-block:: cpp

        nb::class_<Pet>(m, "Pet")
            .def(nb::init<const std::string &>())
            .def_readwrite("name", &Pet::name)
            // ... remainder ...

This makes it possible to write

.. code-block:: pycon

    >>> p = my_ext.Pet("Molly")
    >>> p.name
    'Molly'
    >>> p.name = "Charly"
    >>> p.name
    'Charly'

Now suppose that ``Pet::m_name`` was a private internal variable
that can only be accessed via setters and getters.

.. code-block:: cpp

    class Pet {
    public:
        Pet(const std::string &name) : m_name(name) { }
        void set_name(const std::string &name) { m_name = name; }
        const std::string &get_name() const { return m_name; }

    private:
        std::string m_name;
    };

In this case, the method :func:`class_::def_property`
(:func:`class_::def_property_readonly` for read-only data) can be used to
provide a field-like interface within Python that will transparently call
the setter and getter functions:

.. code-block:: cpp

        nb::class_<Pet>(m, "Pet")
            .def(nb::init<const std::string &>())
            .def_property("name", &Pet::name, &Pet::set_name)
            // ... remainder ...

Write only properties can be defined by passing ``nullptr`` as the
input for the read function.

.. seealso::

    Similar functions :func:`class_::def_readwrite_static`,
    :func:`class_::def_readonly_static` :func:`class_::def_property_static`,
    and :func:`class_::def_property_readonly_static` are provided for binding
    static variables and properties. Please also see the section on
    :ref:`static_properties` in the advanced part of the documentation.

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
attributes are the ones explicitly defined using :func:`class_::def_readwrite`
or :func:`class_::def_property`.

.. code-block:: cpp

    nb::class_<Pet>(m, "Pet")
        .def(nb::init<>())
        .def_readwrite("name", &Pet::name);

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
        .def_readwrite("name", &Pet::name);

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

.. _inheritance:

Inheritance
-----------

Suppose now that the example consists of two data structures with an
inheritance relationship:

.. code-block:: cpp

    struct Pet {
        Pet(const std::string &name) : name(name) { }
        std::string name;
    };

    struct Dog : Pet {
        Dog(const std::string &name) : Pet(name) { }
        std::string bark() const { return "woof!"; }
    };

There are two different ways of indicating a hierarchical relationship to
nanobind: the first specifies the C++ base class as an extra template
parameter of the :class:`class_`:

.. code-block:: cpp

    nb::class_<Pet>(m, "Pet")
       .def(nb::init<const std::string &>())
       .def_readwrite("name", &Pet::name);

    // Method 1: template parameter:
    nb::class_<Dog, Pet /* <- specify C++ parent type */>(m, "Dog")
        .def(nb::init<const std::string &>())
        .def("bark", &Dog::bark);

Alternatively, we can also assign a name to the previously bound ``Pet``
:class:`class_` object and reference it when binding the ``Dog`` class:

.. code-block:: cpp

    nb::class_<Pet> pet(m, "Pet");
    pet.def(nb::init<const std::string &>())
       .def_readwrite("name", &Pet::name);

    // Method 2: pass parent class_ object:
    nb::class_<Dog>(m, "Dog", pet /* <- specify Python parent type */)
        .def(nb::init<const std::string &>())
        .def("bark", &Dog::bark);

Functionality-wise, both approaches are equivalent. Afterwards, instances will
expose fields and methods of both types:

.. code-block:: pycon

    >>> p = my_ext.Dog("Molly")
    >>> p.name
    'Molly'
    >>> p.bark()
    'woof!'

The C++ classes defined above are regular non-polymorphic types with an
inheritance relationship. This is reflected in Python:

.. code-block:: cpp

    // Return a base pointer to a derived instance
    m.def("pet_store", []() { return std::unique_ptr<Pet>(new Dog("Molly")); });

.. code-block:: pycon

    >>> p = my_ext.pet_store()
    >>> type(p)  # `Dog` instance behind `Pet` pointer
    Pet          # no pointer downcasting for regular non-polymorphic types
    >>> p.bark()
    AttributeError: 'Pet' object has no attribute 'bark'

The function returned a ``Dog`` instance, but because it's a non-polymorphic
type behind a base pointer, Python only sees a ``Pet``. In C++, a type is only
considered polymorphic if it has at least one virtual function and nanobind
will automatically recognize this:



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
       .def("set", static_cast<void (Pet::*)(int)>(&Pet::set), "Set the pet's age")
       .def("set", static_cast<void (Pet::*)(const std::string &)>(&Pet::set), "Set the pet's name");

The overload signatures are also visible in the method's docstring:

.. code-block:: pycon

    >>> help(my_ext.Pet)

    class Pet(__builtin__.object)
     |  Methods defined here:
     |
     |  __init__(...)
     |      Signature : (Pet, str, int) -> NoneType
     |
     |  set(...)
     |      1. Signature : (Pet, int) -> NoneType
     |
     |      Set the pet's age
     |
     |      2. Signature : (Pet, str) -> NoneType
     |
     |      Set the pet's name

If you have a C++14 compatible compiler [#cpp14]_, you can use an alternative
syntax to cast the overloaded function:

.. code-block:: cpp

    nb::class_<Pet>(m, "Pet")
        .def("set", nb::overload_cast<int>(&Pet::set), "Set the pet's age")
        .def("set", nb::overload_cast<const std::string &>(&Pet::set), "Set the pet's name");

Here, ``nb::overload_cast`` only requires the parameter types to be specified.
The return type and class are deduced. This avoids the additional noise of
``void (Pet::*)()`` as seen in the raw cast. If a function is overloaded based
on constness, the ``nb::const_`` tag should be used:

.. code-block:: cpp

    struct Widget {
        int foo(int x, float y);
        int foo(int x, float y) const;
    };

    nb::class_<Widget>(m, "Widget")
       .def("foo_mutable", nb::overload_cast<int, float>(&Widget::foo))
       .def("foo_const",   nb::overload_cast<int, float>(&Widget::foo, nb::const_));

If you prefer the ``nb::overload_cast`` syntax but have a C++11 compatible compiler only,
you can use ``nb::detail::overload_cast_impl`` with an additional set of parentheses:

.. code-block:: cpp

    template <typename... Args>
    using overload_cast_ = nanobind::detail::overload_cast_impl<Args...>;

    nb::class_<Pet>(m, "Pet")
        .def("set", overload_cast_<int>()(&Pet::set), "Set the pet's age")
        .def("set", overload_cast_<const std::string &>()(&Pet::set), "Set the pet's name");

.. [#cpp14] A compiler which supports the ``-std=c++14`` flag.

.. note::

    To define multiple overloaded constructors, simply declare one after the
    other using the ``.def(nb::init<...>())`` syntax. The existing machinery
    for specifying keyword and default arguments also works.

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
        .def_readwrite("name", &Pet::name)
        .def_readwrite("type", &Pet::type)
        .def_readwrite("attr", &Pet::attr);

    nb::enum_<Pet::Kind>(pet, "Kind")
        .value("Dog", Pet::Kind::Dog)
        .value("Cat", Pet::Kind::Cat)
        .export_values();

    nb::class_<Pet::Attributes>(pet, "Attributes")
        .def(nb::init<>())
        .def_readwrite("age", &Pet::Attributes::age);


To ensure that the nested types ``Kind`` and ``Attributes`` are created within the scope of ``Pet``, the
``pet`` :class:`class_` instance must be supplied to the :class:`enum_` and :class:`class_`
constructor. The :func:`enum_::export_values` function exports the enum entries
into the parent scope, which should be skipped for newer C++11-style strongly
typed enums.

.. code-block:: pycon

    >>> p = Pet("Lucy", Pet.Cat)
    >>> p.type
    Kind.Cat
    >>> int(p.type)
    1L

The entries defined by the enumeration type are exposed in the ``__members__`` property:

.. code-block:: pycon

    >>> Pet.Kind.__members__
    {'Dog': Kind.Dog, 'Cat': Kind.Cat}

The ``name`` property returns the name of the enum value as a unicode string.

.. note::

    It is also possible to use ``str(enum)``, however these accomplish different
    goals. The following shows how these two approaches differ.

    .. code-block:: pycon

        >>> p = Pet("Lucy", Pet.Cat)
        >>> pet_type = p.type
        >>> pet_type
        Pet.Cat
        >>> str(pet_type)
        'Pet.Cat'
        >>> pet_type.name
        'Cat'

.. note::

    When the special tag ``nb::arithmetic()`` is specified to the ``enum_``
    constructor, nanobind creates an enumeration that also supports rudimentary
    arithmetic and bit-level operations like comparisons, and, or, xor, negation,
    etc.

    .. code-block:: cpp

        nb::enum_<Pet::Kind>(pet, "Kind", nb::arithmetic())
           ...

    By default, these are omitted to conserve space.
