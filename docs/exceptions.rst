.. cpp:namespace:: nanobind

.. _exceptions:

Exceptions
==========

.. _exception_conversion:

Automatic conversion of C++ exceptions
--------------------------------------

When Python calls a C++ function, that function might raise an exception
instead of returning a result. In such a case, nanobind will capture the C++
exception and then raise an equivalent exception within Python. This automatic
conversion supports ``std::exception``, common subclasses, and several classes
that convert to specific Python exceptions as shown below:

.. list-table::
  :widths: 40 60
  :header-rows: 1

  * - Exception thrown by C++
    - Translated to Python exception type
  * - ``std::exception``
    - ``RuntimeError``
  * - ``std::bad_alloc``
    - ``MemoryError``
  * - ``std::domain_error``
    - ``ValueError``
  * - ``std::invalid_argument``
    - ``ValueError``
  * - ``std::length_error``
    - ``ValueError``
  * - ``std::out_of_range``
    - ``IndexError``
  * - ``std::range_error``
    - ``ValueError``
  * - ``std::overflow_error``
    - ``OverflowError``
  * - :cpp:class:`nb::stop_iteration <stop_iteration>`
    - ``StopIteration`` (used to implement custom iterator) 
  * - :cpp:class:`nb::index_error <index_error>`
    - ``IndexError`` (used to indicate out of bounds access in ``__getitem__``,
      ``__setitem__``, etc.)
  * - :cpp:class:`nb::key_error <index_error>`
    - ``KeyError`` (used to indicate an invalid access in ``__getitem__``,
      ``__setitem__``, etc.)
  * - :cpp:class:`nb::value_error <value_error>`
    - ``ValueError`` (used to indicate an invalid value in operations like
      ``container.remove(...)``)
  * - :cpp:class:`nb::type_error <type_error>`
    - ``TypeError``
  * - :cpp:class:`nb::buffer_error <type_error>`
    - ``BufferError``
  * - :cpp:class:`nb::import_error <import_error>`
    - ``ImportError``
  * - :cpp:class:`nb::attribute_error <attribute_error>`
    - ``AttributeError``
  * - Any other exception
    - ``SystemError``

Exception translation is not bidirectional. A C++ ``catch
(nb::key_error)`` block will not catch a Python ``KeyError``. Use
:cpp:class:`nb::python_error <python_error>` for this purpose (see the :ref:`example
below <handling_python_exceptions_cpp>` for details).

The is also a special exception :cpp:class:`nb::cast_error <cast_error>` that may
be raised
by the call operator :cpp:func:`nb::handle::operator()
<detail::api::operator()>` and :cpp:func:`nb::cast() <cast>` when argument(s)
cannot be converted to Python objects.

.. _custom_exceptions:

Handling custom exceptions
--------------------------

nanobind can also expose custom exception types. The
:cpp:class:`nb::exception\<T\> <exception>` helper resembles
:cpp:class:`nb::class_\<T\> <class_>` and registers a new exception type within
the provided scope.

.. code-block:: cpp

   NB_MODULE(my_ext, m) {
       nb::exception<CppExp>(m, "PyExp");
   }

Here, it creates ``my_ext.PyExp``. Subsequently, any C++ exception of type
``CppExp`` crossing the language barrier will automatically convert to
``my_ext.PyExp``.

A Python exception base class can optionally be specified. For example, the
snippet below causes ``PyExp`` to inherit from ``RuntimeError`` (the default is
``Exception``). The built-in Python exception classes are listed `here
<https://docs.python.org/3/c-api/exceptions.html#standard-exceptions>`__.

.. code-block:: cpp

    nb::exception<CppExp>(module, "PyExp", PyExc_RuntimeError);

In more complex cases, :cpp:func:`nb::register_exception_translator()
<register_exception_translator>` can be called to register a custom exception
translation routine. It takes a stateless callable (e.g. a function pointer or
a lambda function without captured variables) with the call signature
``void(const std::exception_ptr &, void*)`` and an optional payload pointer
value that will be passed to the second parameter of the callable.

When a C++ exception is captured by nanobind, all registered exception
translators are tried in reverse order of registration (i.e. the last
registered translator has the first chance of handling the exception). 

Inside the translator, call ``std::rethrow_exception()`` within a
``try``-``catch`` block to re-throw the exception and capture supported
exception types. The ``catch`` block should call ``PyErr_SetString`` or
``PyErr_Format`` (`1
<https://docs.python.org/3/c-api/exceptions.html#c.PyErr_SetString>`__, `2
<https://docs.python.org/3/c-api/exceptions.html#c.PyErr_Format>`__) to
set a suitable Python error status. The following example demonstrates this
pattern to convert ``MyCustomException`` into a Python ``IndexError``.

.. code-block:: cpp

    nb::register_exception_translator(
        [](const std::exception_ptr &p, void * /* unused */) {
            try {
                std::rethrow_exception(p);
            } catch (const MyCustomException &e) {
                PyErr_SetString(PyExc_IndexError, e.what());
            }
        });

Multiple exceptions can be handled by a single translator. nanobind captures
unhandled exceptions and forwards them to the preceding translator. If none of
the exception translators succeeds, it will convert according to the previously
discussed default rules.

.. note::

    When the exception translator returns normally, it must have set a Python
    error status. Otherwise, Python will crash with the message ``SystemError:
    error return without exception set``.

    Unsupported exception types should not be caught, or may be explicitly
    (re-)thrown to delegate them to the other exception translators.

.. _handling_python_exceptions_cpp:

Capturing Python exceptions within C++
--------------------------------------

When nanobind-based C++ code calls a Python function that raises an exception,
it will automatically convert into a :class:`nb::python_error <python_error>`
raised on the C++ side. This exception type can be caught and handled in C++ or
propagate back into Python, where it will undergo reverse conversion.

.. list-table::
  :widths: 40 60
  :header-rows: 1

  * - Exception raised in Python
    - Translated to C++ exception type
  * - Any Python ``Exception``
    - :cpp:class:`nb::python_error <python_error>`

The class exposes various members to obtain further information about the
exception. The :cpp:func:`.type() <python_error::type>` and :cpp:func:`.value()
<python_error::value>` methods provide information about the exception type and
value, while :cpp:func:`.what() <python_error::what>` generates a
human-readable representation including a backtrace.

A use of the :cpp:func:`.matches() <python_error::matches>` method to
distinguish different exception types is shown below:

.. code-block:: cpp

    try {
        nb::object file = nb::module_::import_("io").attr("open")("file.txt", "r");
        nb::object text = file.attr("read")();
        file.attr("close")();
    } catch (const nb::python_error &e) {
        if (e.matches(PyExc_FileNotFoundError)) {
            nb::print("file.txt not found");
        } else if (e.matches(PyExc_PermissionError)) {
            nb::print("file.txt found but not accessible");
        } else {
            throw;
        }
    }

Note that the previously discussed :ref:`automatic conversion
<exception_conversion>` of C++ exception does not apply here. Errors raised
from Python *always* convert to :cpp:class:`nb::python_error <python_error>`.

Handling errors from the Python C API
-------------------------------------

Whenever possible, use :ref:`nanobind wrappers <wrappers>` instead of calling
the Python C API directly. Otherwise, you must carefully manage reference
counts and adhere to the nanobind error protocol outlined below.

When a Python C API call fails with an error status, you must immediately
``throw nb::python_error();`` to capture the error and handle it using
appropriate C++ mechanisms. This includes calls to error setting functions such
as ``PyErr_SetString`` (:ref:`custom exception translators <custom_exceptions>`
are excluded from this rule).

.. code-block:: cpp

    PyErr_SetString(PyExc_TypeError, "C API type error demo");
    throw nb::python_error();

    // But it would be easier to simply...
    throw nb::type_error("nanobind wrapper type error");

Alternately, to ignore the error, call `PyErr_Clear()
<https://docs.python.org/3/c-api/exceptions.html#c.PyErr_Clear>`__. Any
Python error must be thrown or cleared, or nanobind will be left in an
invalid state.
