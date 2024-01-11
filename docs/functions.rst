.. _functions:

.. cpp:namespace:: nanobind

Functions
=========

Binding annotations
-------------------

Besides :ref:`keyword and default arguments <keyword_and_default_args>`,
:ref:`docstrings <docstrings>`, and :ref:`return value policies <rvp>`, other
function binding annotations can be specified to achieve different goals as
described below.

Default arguments revisited
---------------------------

A noteworthy point about the previously discussed way of specifying
:ref:`default arguments <keyword_and_default_args>` is that nanobind
immediately converts them into Python objects. Consider the following example:

.. code-block:: cpp

   nb::class_<MyClass>(m, "MyClass")
       .def("f", &MyClass::f, "value"_a = SomeType(123));

nanobind must be set up to deal with values of the type ``SomeType`` (via a
prior instantiation of ``nb::class_<SomeType>``), or an exception will be
thrown.

The "preview" of the default argument in the function signature is generated
using the object's ``__repr__`` method. If not available, the signature may not
be very helpful, e.g.:

.. code-block:: pycon

   >> help(my_ext.MyClass)

   class MyClass(builtins.object)
    |  Methods defined here:
    ....
    |  f(...)
    |      f(self, value: my_ext.SomeType = <my_ext.SomeType object at 0x1004d7230>) -> None


.. _noconvert:

Implicit conversions, and how to suppress them
----------------------------------------------

Consider the following function taking a floating point value as input:

.. code-block:: cpp

   m.def("double", [](float x) { return 2.f * x; });

We can call this function using a Python ``float``, but an ``int`` works just
as well:

.. code-block:: pycon

   >>> my_ext.double(2)
   4.0

nanobind performed a so-called *implicit conversion* for convenience. The same
mechanism generalizes to custom types defining a
:cpp:class:`nb::init_implicit\<T\>() <init_implicit>`-style constructor:

.. code-block:: cpp

   nb::class_<A>(m, "A")
       // Following this line, nanobind will automatically convert 'B' -> 'A' if needed
       .def(nb::init_implicit<B>());

This behavior is not always desirable---sometimes, it is better to give up or
try another function overload. To achieve this behavior, use the
:cpp:func:`.noconvert() <arg::noconvert>` method of the :cpp:class:`nb::arg
<arg>` annotation to mark the argument as *non-converting*. An example:

.. code-block:: cpp

   m.def("double", [](float x) { return 2.f * x; }, nb::arg("x").noconvert());

The same experiment now fails with a ``TypeError``:

.. code-block:: pycon

   >>> my_ext.double(2)
   TypeError: double(): incompatible function arguments. The following ↵
   argument types are supported:
       1. double(x: float) -> float

   Invoked with types: int

You may, of course, combine this with the ``_a`` shorthand notation (see the
section on :ref:`keyword arguments <keyword_and_default_args>`) or use specify
*unnamed* non-converting arguments using :cpp:func:`nb::arg().noconvert()
<arg::noconvert>`.

.. note::

   The number of :cpp:class:`nb::arg <arg>` annotations must match the argument
   count of the function. To enable no-convert behaviour for just one of
   several arguments, you will need to specify :cpp:func:`nb::arg().noconvert()
   <arg::noconvert>` for that argument, and :cpp:class:`nb::arg() <arg>` for
   the remaining ones.

.. _none_arguments:

None arguments
--------------

A common design pattern in C/C++ entails passing ``nullptr`` to pointer-typed
arguments to indicate a missing value. Since nanobind cannot know whether a
function uses such a convention, it refuses conversions from ``None`` to
``nullptr`` by default. For example, consider the following binding code:

.. code-block:: cpp

   struct Dog { };
   const char *bark(Dog *dog) {
       return dog != nullptr ? "woof!" : "(no dog)";
   }

   NB_MODULE(my_ext, m) {
       nb::class_<Dog>(m, "Dog")
           .def(nb::init<>());
       m.def("bark", &bark);
   }

Calling the function with ``None`` raises an exception:

.. code-block:: pycon

   >>> my_ext.bark(my_ext.Dog())
   'woof!'
   >>> my_ext.bark(None)
   TypeError: bark(): incompatible function arguments. The following ↵
   argument types are supported:
       1. bark(arg: my_ext.Dog, /) -> str

To switch to a more permissive behavior, call the :cpp:func:`.none()
<arg::none>` method of the :cpp:class:`nb::arg <arg>` annotation:

.. code-block:: cpp

   m.def("bark", &bark, nb::arg("dog").none());

With this change, the function accepts ``None``, and its signature also changes
to reflect this fact.

.. code-block:: pycon

   >>> my_ext.bark(None)
   '(no dog)'

   >>> my_ext.bark.__doc__
   'bark(dog: Optional[my_ext.Dog]) -> str'

You may also specify a ``None`` default argument value, in which case the
annotation can be omitted:

.. code-block:: cpp

   m.def("bark", &bark, nb::arg("dog") = nb::none());

Note that passing values *by pointer* (including null pointers) is only
supported for :ref:`bound <bindings>` types. :ref:`Type casters <type_casters>`
and :ref:`wrappers <wrappers>` cannot be used in such cases and will produce
compile-time errors.

Alternatively, you can also use ``std::optional<T>`` to pass an optional
argument *by value*. To use it, you must include the header file associated
needed by its type caster:

.. code-block:: cpp

   #include <nanobind/stl/optional.h>

   NB_MODULE(my_ext, m) {
       m.def("bark", [](std::optional<Dog> d) { ... }, nb::arg("dog") = nb::none());
   }


.. _overload_resolution:

Overload resolution order
-------------------------

nanobind relies on a two-pass scheme to determine the right implementation when
a bound function or method with multiple overloads is called from Python.

The first pass attempts to call each overload while disabling implicit argument
conversion---it's as if every argument had a matching
:cpp:func:`nb::arg().noconvert() <arg::noconvert>` annotation as described
:ref:`above <noconvert>`. The process terminates successfully when nanobind
finds an overload that is compatible with the provided arguments.

If the first pass fails, a second pass retries all overloads while enabling
implicit argument conversion. If the second pass also fails, the function
dispatcher raises a ``TypeError``.

Within each pass, nanobind tries overloads in the order in which they were
registered. Consequently, it prefers an overload that does not require implicit
conversion to one that does, but otherwise prefers earlier-defined overloads to
later-defined ones. Within the second pass, the precise number of implicit
conversions needed does not influence the order.

The special exception :cpp:class:`nb::next_overload <next_overload>` can also
influence overload resolution. Raising this exception from an overloaded
function causes it to be skipped, and overload resolution resumes. This can be
helpful in complex situations where the value of a parameter must be inspected
to see if a particular overload is eligible.

.. _args_kwargs_1:

Accepting \*args and \*\*kwargs
-------------------------------

Python supports functions that accept an arbitrary number of positional and
keyword arguments:

.. code-block:: python

   def generic(*args, **kwargs):
       ...  # do something with args and kwargs

Such functions can also be created using nanobind:

.. code-block:: cpp

   void generic(nb::args args, nb::kwargs kwargs) {
       for (auto v: args)
           nb::print(nb::str("Positional: {}").format(v));
       for (auto kv: kwargs)
           nb::print(nb::str("Keyword: {} -> {}").format(kv.first, kv.second));
   }

   // Binding code
   m.def("generic", &generic);

The class :cpp:class:`nb::args <args>` derives from :cpp:class:`nb::tuple
<tuple>` and :cpp:class:`nb::kwargs <kwargs>` derives from :cpp:class:`nb::dict
<dict>`.

You may also use them individually or even combine them with ordinary
arguments. Note, however, that :cpp:class:`nb::args <args>` and
:cpp:class:`nb::kwargs <kwargs>` must always be the last arguments of the
function, and in that order if both are specified. This is a restriction
compared to pybind11, which allowed more general arrangements. nanobind also
lacks the ``kw_only`` and ``pos_only`` annotations available in pybind11.

.. _args_kwargs_2:

Expanding \*args and \*\*kwargs
-------------------------------

Conversely, nanobind can also expand standard containers to add positional and
keyword arguments to a Python call. The example below shows how to do this
using the wrapper types :cpp:class:`nb::object <object>`,
:cpp:class:`nb::callable <callable>`, :cpp:class:`nb::list <list>`,
:cpp:class:`nb::dict <dict>`

.. code-block:: cpp

   nb::object my_call(nb::callable callable) {
       nb::list list;
       nb::dict dict;

       list.append("positional");
       dict["keyword"] = "value";

       return callable(1, *list, **dict);
   }

   NB_MODULE(my_ext, m) {
       m.def("my_call", &my_call);
   }

Here is an example use of the above extension in Python:

.. code-block:: pycon

   >>> def x(*args, **kwargs):
   ...     print(args)
   ...     print(kwargs)
   ...
   >>> import my_ext
   >>> my_ext.my_call(x)
   (1, 'positional')
   {'keyword': 'value'}

.. _function_templates:

Function templates
------------------

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

.. _lifetime_annotations:

Lifetime annotations
--------------------

The :cpp:class:`nb::keep_alive\<Nurse, Patient\>() <keep_alive>` annotation
indicates that the argument with index ``Patient`` should be kept alive at least
until the argument with index ``Nurse`` is freed by the garbage collector.

The example below applies the annotation to a hypothetical operation that
appends an entry to a log data structure.

.. code-block:: cpp

    nb::class_<Log>(m, "Log")
        .def("append",
             [](Log &log, Entry *entry) -> void { ... },
             nb::keep_alive<1, 2>());

Here, ``Nurse = 1`` refers to the ``log`` argument, while ``Patient = 2``
refers to ``entry``. Setting ``Nurse/Patient = 0`` would select the function
return value (here, the function doesn't return anything, so ``0`` is not a
valid choice).

The example uses the annotation to tie the lifetime of the ``entry`` to that of
``log``. Without it, Python could potentially delete ``entry`` *before*
``log``, which would be problematic if the ``log.append()`` operation causes
``log`` to reference ``entry`` through a pointer address instead of making a
copy. Whether or not this is a good design is another question (for example,
shared ownership via ``std::shared_ptr<T>`` or intrusive reference counting
would avoid the problem altogether).

See the definition of :cpp:class:`nb::keep_alive <keep_alive>` for further
discussion and limitations of this method.

.. _call_guards:

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

.. _higher_order_adv:

Higher-order functions
----------------------

The C++11 standard introduced lambda functions and the generic polymorphic
function wrapper ``std::function<>``, which enable powerful new ways of working
with functions. Lambda functions come in two flavors: stateless lambda function
resemble classic function pointers that link to an anonymous piece of code,
while stateful lambda functions additionally depend on captured variables that
are stored in an anonymous *lambda closure object*.

Here is a simple example of a C++ function that takes an arbitrary function
(stateful or stateless) with signature ``int -> int`` as an argument and runs
it with the value 10.

.. code-block:: cpp

   int func_arg(const std::function<int(int)> &f) {
       return f(10);
   }

The example below is more involved: it takes a function of signature ``int -> int``
and returns another function of the same kind. The return value is a stateful
lambda function, which stores the value ``f`` in the capture object and adds 1 to
its return value upon execution.

.. code-block:: cpp

   std::function<int(int)> func_ret(const std::function<int(int)> &f) {
       return [f](int i) {
           return f(i) + 1;
       };
   }

This example demonstrates using python named parameters in C++ callbacks which
requires use of the :cpp:func:`nb::cpp_function <cpp_function>` conversion
function. Usage is similar to defining methods of classes:

.. code-block:: cpp

   nb::object func_cpp() {
       return nb::cpp_function([](int i) { return i+1; },
          nb::arg("number"));
   }

After including the extra header file :file:`nanobind/stl/function.h`, it is almost
trivial to generate binding code for all of these functions.

.. code-block:: cpp

   #include <nanobind/stl/function.h>

   NB_MODULE(my_ext, m) {
       m.def("func_arg", &func_arg);
       m.def("func_ret", &func_ret);
       m.def("func_cpp", &func_cpp);
   }

The following interactive session shows how to call them from Python.

.. code-block:: pycon

   Python 3.11.1 (main, Dec 23 2022, 09:28:24) [Clang 14.0.0 (clang-1400.0.29.202)] on darwin
   Type "help", "copyright", "credits" or "license" for more information.
   >>> import my_ext
   >>> def square(i):
   ...     return i*i
   ...
   >>> my_ext.func_arg(square)
   100
   >>> square_plus_1 = my_ext.func_ret(square)
   >>> square_plus_1(4)
   17
   >>> plus_1 = my_ext.func_cpp()
   >>> plus_1.__doc__
   '<anonymous>(number: int) -> int'
   >>> plus_1(number=43)
   44

.. note::

   This functionality is very useful when generating bindings for callbacks in
   C++ libraries (e.g. GUI libraries, asynchronous networking libraries,
   etc.).
