.. _functions:

.. cpp:namespace:: nanobind

Functions
#########

Binding annotations
===================

In addition to :ref:`keyword and default arguments
<keyword_and_default_args>`, :ref:`docstrings <docstrings>`, and
:ref:`return value policies <rvp>`, other kinds function binding annotations
can be specified to accomplish different goals as described below.

Lifetime annotation
-------------------

The :cpp:class:`nb::keep_alive\<Nurse, Patient\> <keep_alive>` annotation
indicates that the argument with index ``Patient`` should be kept alive at least
until the argument with index ``Nurse`` is freed by the garbage collector.

The example below applies it to operation that appends an object to a
hypothetical C++ container ``List``.

.. code-block:: cpp

    nb::class_<List>(m, "List")
        .def("append",
             [](List &l, Item *item) { ... },
             nb::keep_alive<1, 2>());

Argument indices start at ``1``, while ``0`` refers to the return value. For
methods and constructors, index ``1`` refers to the implicit ``this``
pointer, while regular arguments begin at index ``2``. When a ``Nurse`` or
``Patient`` with value ``None`` is detected at runtime, the annotation does
nothing. When the nurse object is neither a nanobind-registered type nor
weak-referenceable, an exception will be thrown.

Call guards
-----------

The :cpp:class:`nb::call_guard\<T\>() <call_guard>` annotation allows any scope
guard ``T`` to be placed around the function call. For example, this
definition:

.. code-block:: cpp

    m.def("foo", foo, nb::call_guard<T>());

is equivalent to the following pseudocode:

.. code-block:: cpp

    m.def("foo", [](args...) {
        T scope_guard;
        return foo(args...); // forwarded arguments
    });

The only requirement is that ``T`` is default-constructible, but otherwise
any scope guard will work. This feature is often combined with
:cpp:class:`nb::gil_scoped_release <gil_scoped_release>` to release the
Python *global interpreter lock* (GIL) during a long-running C++ routine
to permit parallel execution.

Multiple guards should be specified as :cpp:class:`nb::call_guard\<T1, T2,
T3...\> <call_guard>`. Construction occurs left to right, while destruction
occurs in reverse.

Accepting \*args and \*\*kwargs
===============================

Python supports functions that accept an arbitrary number of arguments and
keyword arguments:

.. code-block:: python

   def generic(*args, **kwargs):
       ...  # do something with args and kwargs

Such functions can also be created using nanobind:

.. code-block:: cpp

   void generic(nb::args args, nb::kwargs kwargs) {
       /// .. do something with args
       if (kwargs.is_valid())
           /// .. do something with kwargs
   }

   /// Binding code
   m.def("generic", &generic);

The class :cpp:class:`nb::args <args>` derives from :cpp:class:`nb::tuple
<tuple>` and :cpp:class:`nb::kwargs <kwargs>` derives from :cpp:class:`nb::dict
<dict>`.

You may also use just one or the other, and may combine these with other
arguments.  Note, however, that :cpp:class:`nb::kwargs <kwargs>` must always be
the last argument of the function, and :cpp:class:`nb::args <args>` implies
that any further arguments are keyword-only.

.. note::

    When combining \*args or \*\*kwargs with :ref:`keyword arguments
    <keyword_and_default_args>` you should *not* include :cpp:class:`nb::arg
    <arg>` or ``""_a`` tag for the :cpp:class:`nb::args <args>` and `nb::kwargs
    <kwargs>` arguments.

Default arguments revisited
===========================

The section on :ref:`keyword arguments <keyword_and_default_args>` 
previously discussed basic usage of default
arguments using nanobind. One noteworthy aspect of their implementation is that
default arguments are converted to Python objects right at declaration time.
Consider the following example:

.. code-block:: cpp

    nb::class_<MyClass>("MyClass")
        .def("my_function", nb::arg("arg") = SomeType(123));

In this case, nanobind must already be set up to deal with values of the type
``SomeType`` (via a prior instantiation of ``nb::class_<SomeType>``), or an
exception will be thrown.

Another aspect worth highlighting is that the "preview" of the default argument
in the function signature is generated using the object's ``__repr__`` method.
If not available, the signature may not be very helpful, e.g.:

.. code-block:: pycon

    FUNCTIONS
    ...
    |  myFunction(...)
    |      Signature : (MyClass, arg : SomeType = <SomeType object at 0x101b7b080>) -> NoneType
    ...


.. _nonconverting_arguments:

Non-converting arguments
========================

Certain argument types may support conversion from one type to another.  Some
examples of conversions are:

* :ref:`Implicit conversions <implicit_conversions>` declared using
  :cpp:class:`nb::implicitly_convertible\<A, B\>() <implicitly_convertible>`
* Calling a method expecting a floating point argument with an integer.
* Calling a function taking a :cpp:class:`nb::tensor\<..\> <tensor>` or
  :ref:`Eigen array <eigen>` with a NumPy array of the wrong layout or data type.

This behaviour is sometimes undesirable, and the binding code may prefer to
raise an error. To achieve this behavior, call the :cpp:func:`.noconvert()
<arg::noconvert>` method of the :cpp:class:`nb::arg <arg>` argument annotation, for example:

.. code-block:: cpp

    m.def("floats_only", [](double f) { return 0.5 * f; }, nb::arg("f").noconvert());
    m.def("floats_preferred", [](double f) { return 0.5 * f; }, nb::arg("f"));

Attempting the call the second function (the one without
:cpp:func:`.noconvert() <arg::noconvert>`) with an integer will succeed, but
attempting to call the :cpp:func:`.noconvert() <arg::noconvert>` version will
fail with a ``TypeError``:

.. code-block:: pycon

    >>> floats_preferred(4)
    2.0
    >>> floats_only(4)
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    TypeError: floats_only(): incompatible function arguments. The following argument types are supported:
        1. (f: float) -> float

    Invoked with: 4

You may, of course, combine this with the ``_a`` shorthand notation (see
the section on :ref:`keyword arguments <keyword_and_default_args>`).
It is also permitted to omit
the argument name by using the :cpp:class:`nb::arg() <arg>` constructor without an argument
name, i.e. by specifying :cpp:func:`nb::arg().noconvert() <arg::noconvert>`.

.. note::

   The number of :cpp:class:`nb::arg <arg>` annotations must match the argument
   count of the function. To enable no-convert behaviour for just one of
   several arguments, you will need to specify :cpp:func:`nb::arg().noconvert()
   <arg::noconvert>` for that argument, and :cpp:class:`nb::arg() <arg>` for
   the remaining ones.

.. _none_arguments:

Allow/Prohibiting None arguments
================================

When a C++ type registered with :class:`nb::class_` is passed as an argument to
a function taking the instance as pointer or shared holder (e.g. ``shared_ptr``
or a custom, copyable holder as described in :ref:`smart_pointers`), pybind
allows ``None`` to be passed from Python which results in calling the C++
function with ``nullptr`` (or an empty holder) for the argument.

To explicitly enable or disable this behaviour, using the
``.none`` method of the :class:`nb::arg` object:

.. code-block:: cpp

    nb::class_<Dog>(m, "Dog").def(nb::init<>());
    nb::class_<Cat>(m, "Cat").def(nb::init<>());
    m.def("bark", [](Dog *dog) -> std::string {
        if (dog) return "woof!"; /* Called with a Dog instance */
        else return "(no dog)"; /* Called with None, dog == nullptr */
    }, nb::arg("dog").none(true));
    m.def("meow", [](Cat *cat) -> std::string {
        // Can't be called with None argument
        return "meow";
    }, nb::arg("cat").none(false));

With the above, the Python call ``bark(None)`` will return the string ``"(no
dog)"``, while attempting to call ``meow(None)`` will raise a ``TypeError``:

.. code-block:: pycon

    >>> from animals import Dog, Cat, bark, meow
    >>> bark(Dog())
    'woof!'
    >>> meow(Cat())
    'meow'
    >>> bark(None)
    '(no dog)'
    >>> meow(None)
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    TypeError: meow(): incompatible function arguments. The following argument types are supported:
        1. (cat: animals.Cat) -> str

    Invoked with: None

The default behaviour when the tag is unspecified is to allow ``None``.

.. note::

    Even when ``.none(true)`` is specified for an argument, ``None`` will be converted to a
    ``nullptr`` *only* for custom and :ref:`opaque <opaque>` types. Pointers to built-in types
    (``double *``, ``int *``, ...) and STL types (``std::vector<T> *``, ...; if ``nanobind/stl.h``
    is included) are copied when converted to C++ (see :doc:`/advanced/cast/overview`) and will
    not allow ``None`` as argument.  To pass optional argument of these copied types consider
    using ``std::optional<T>``

.. _overload_resolution:

Overload resolution order
=========================

When a function or method with multiple overloads is called from Python,
nanobind determines which overload to call in two passes.  The first pass
attempts to call each overload without allowing argument conversion (as if
every argument had been specified as ``nb::arg().noconvert()`` as described
above).

If no overload succeeds in the no-conversion first pass, a second pass is
attempted in which argument conversion is allowed (except where prohibited via
an explicit ``nb::arg().noconvert()`` attribute in the function definition).

If the second pass also fails a ``TypeError`` is raised.

Within each pass, overloads are tried in the order they were registered with
nanobind. What this means in practice is that nanobind will prefer any overload
that does not require conversion of arguments to an overload that does, but
otherwise prefers earlier-defined overloads to later-defined ones.

.. note::

    nanobind does *not* further prioritize based on the number/pattern of
    overloaded arguments.  That is, nanobind does not prioritize a function
    requiring one conversion over one requiring three, but only prioritizes
    overloads requiring no conversion at all to overloads that require
    conversion of at least one argument.


Function templates
==================

Consider the following function signature with a *template parameter*:

.. code-block:: cpp

    template <typename T> void process(T t);

A template must be instantiated with concrete types to be usable, which is a
compile-time operation. The generic version version therefore cannot be used
in bindings:

.. code-block:: cpp

    m.def("process", &process); // <-- this will not compile

You must bind each instantiation separately, either as a single function
with overloads, or as separately named functions.

.. code-block:: cpp

    // Option 1:
    m.def("process", &process<int>);
    m.def("process", &process<std::string>);

    // Option 2:
    m.def("process_int", &process<int>);
    m.def("process_string", &process<std::string>);
