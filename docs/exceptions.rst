.. cpp:namespace:: nanobind

.. _exceptions:

Exceptions
==========

Automatic conversion of C++ exceptions
--------------------------------------

When Python calls a C++ function, it is conceivable that the function might
raise an exception instead of returning a result. In such a case, nanobind will
capture the C++ exception and then raise an equivalent exception within Python.
nanobind includes translations for ``std::exception``, common subclasses, and
several special classes that translate to specific Python exceptions. The
mapping is shown below:

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

Here, it creates ``my_ext.PyExp``. Furthermore, any C++ exception of type
``CppExp`` crossing the language barrier will subsequently convert into
``my_ext.PyExp``.

A Python exception base class can optionally be specified. For example, the
snippet below causes ``PyExp`` to inherit from ``RuntimeError`` (the default is
``Exception``).

.. code-block:: cpp

    nb::exception<CppExp>(module, "PyExp", PyExc_RuntimeError);

The class objects of the built-in Python exceptions are listed in the Python
documentation on `Standard Exceptions
<https://docs.python.org/3/c-api/exceptions.html#standard-exceptions>`_.

In more complex cases, :cpp:func:`nb::register_exception_translator()
<register_exception_translator>` can be called to register a custom exception
translation routine. It takes a stateless callable (e.g. a function pointer or
a lambda function without captured variables) with the call signature
``void(const std::exception_ptr &, void*)`` and an optional payload pointer
value that will be passed to the second parameter of the callable.

When a C++ exception is captured by nanobind, all registered exception
translators are tried in reverse order of registration (i.e. the last
registered translator has the first chance of handling the exception). 

Inside the translator, ``std::rethrow_exception`` should be used within
a try block to re-throw the exception.  One or more catch clauses to catch
the appropriate exceptions should then be used with each clause using
``PyErr_SetString`` to set a Python exception or ``ex(string)`` to set
the python exception to a custom exception type (see below).

The following example demonstrates this to convert
``MyCustomException`` into a Python ``IndexError``.

.. code-block:: cpp

    nb::register_exception_translator(
        [](const std::exception_ptr &p, void * /* unused */) {
            try {
                std::rethrow_exception(p);
            } catch (const MyCustomException &e) {
                PyErr_SetString(PyExc_IndexError, e.what());
            }
        });

Multiple exceptions can be handled by a single translator. Unhandled exceptions
propagate to the caller and are be handled by the preceding translator. If no
registered exception translator handles the exception, it will be converted
according to the previously discussed default rules.

.. note::

    Call either ``PyErr_SetString`` or a custom exception's call
    operator (``exc(string)``) for every exception caught in a custom exception
    translator.  Failure to do so will cause Python to crash with ``SystemError:
    error return without exception set``.

    Exceptions that you do not plan to handle should simply not be caught, or
    may be explicitly (re-)thrown to delegate it to the other exception
    translators.

.. _handling_python_exceptions_cpp:

Handling exceptions from Python in C++
--------------------------------------

When C++ calls Python functions, such as in a callback function or when
manipulating Python objects, and Python raises an ``Exception``, nanobind
converts the Python exception into a C++ exception of type
:class:`nb::python_error <python_error>` whose payload contains a C++ string
textual summary and the actual Python exception. :cpp:class:`nb::python_error
<python_error>` is used to propagate Python exception back to Python (or
possibly, handle them in C++).

.. list-table::
  :widths: 40 60
  :header-rows: 1

  * - Exception raised in Python
    - Translated to C++ exception type
  * - Any Python ``Exception``
    - :cpp:class:`nb::python_error <python_error>`

For example:

.. code-block:: cpp

    try {
        // open("missing.txt", "r")
        auto file = nb::module_::import("io").attr("open")("missing.txt", "r");
        auto text = file.attr("read")();
        file.attr("close")();
    } catch (nb::python_error &e) {
        if (e.matches(PyExc_FileNotFoundError)) {
            nb::print("missing.txt not found");
        } else if (e.matches(PyExc_PermissionError)) {
            nb::print("missing.txt found but not accessible");
        } else {
            throw;
        }
    }

Note that C++ to Python exception translation does not apply here, since that is
a method for translating C++ exceptions to Python, not vice versa. The error raised
from Python is *always* :cpp:class:`nb::python_error <python_error>`.

Handling errors from the Python C API
-------------------------------------

Where possible, use :ref:`nanobind wrappers <wrappers>` instead of calling
the Python C API directly. When calling the Python C API directly, in
addition to manually managing reference counts, one must follow the nanobind
error protocol, which is outlined here.

After calling the Python C API, if Python returns an error,
``throw nb::python_error();``, which allows nanobind to deal with the
exception and pass it back to the Python interpreter. This includes calls to
the error setting functions such as ``PyErr_SetString``.

.. code-block:: cpp

    PyErr_SetString(PyExc_TypeError, "C API type error demo");
    throw nb::python_error();

    // But it would be easier to simply...
    throw nb::type_error("nanobind wrapper type error");

Alternately, to ignore the error, call `PyErr_Clear
<https://docs.python.org/3/c-api/exceptions.html#c.PyErr_Clear>`_.

Any Python error must be thrown or cleared, or Python/nanobind will be left in
an invalid state.
