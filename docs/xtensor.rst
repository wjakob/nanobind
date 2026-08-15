.. cpp:namespace:: nanobind

.. _xtensor:

The *xtensor* multi-dimensional array library
==============================================

`xtensor <https://github.com/xtensor-stack/xtensor>`__ is a header-only C++
library for multi-dimensional arrays and tensors with lazy evaluation
semantics. It provides NumPy-like syntax for element-wise operations,
broadcasting, and mathematical functions.

nanobind includes custom type casters that enable bidirectional conversion
between xtensor and NumPy arrays. These casters build on the :ref:`n-dimensional array <ndarray_class>` class.

Setup
-----

Add the following include directive to your binding code:

.. code-block:: cpp

   #include <nanobind/xtensor.h>

This umbrella header pulls in casters for all supported xtensor types. You can
also include individual headers if you only need a subset:

.. code-block:: cpp

   #include <nanobind/xtensor/xcontainer.h>  // xt::xarray, xt::xtensor
   #include <nanobind/xtensor/xview.h>       // nb::xarray_view, nb::xtensor_view
   #include <nanobind/xtensor/xexpression.h> // lazy xexpression return
   #include <nanobind/xtensor/xvectorize.h>  // nb::xvectorize

Owning containers
-----------------

Owning containers (``xt::xarray<T>`` and ``xt::xtensor<T, N>``) manage their
own memory. When converting between Python and C++, data is **always copied**.

Python → C++
^^^^^^^^^^^^^

When a Python NumPy array is passed to a C++ function expecting an owning
container, the data is **always copied** into the container. This means the
container owns its memory independently of the Python object.

.. code-block:: cpp

   // Data is copied into the xarray, safe to modify
   m.def("process", [](xt::xarray<double>& arr) {
       arr(0) = 999.0;  // does NOT affect the Python array
       return arr;
   });

The owning caster supports implicit conversion at two levels: if the NumPy
array has a different dtype (e.g., ``float32`` when ``double`` is expected),
nanobind will convert it automatically. For containers with a strict layout
(e.g., ``row_major``), a non-matching memory order is also reordered during
the copy. Both can be disabled with ``nb::arg().noconvert()``, which rejects
dtype or layout mismatches with a ``TypeError``.

C++ → Python
^^^^^^^^^^^^^

When a C++ function returns an owning container, the caster creates a NumPy
array. The behavior depends on the return value policy:

- **Return by value** (``rv_policy::move``): the container is move-constructed
  into a capsule that owns the data. The resulting NumPy array references this
  capsule with zero copy.

- **Return by reference**: the NumPy array references the original C++ data.
  Use ``rv_policy::reference_internal`` when the data is owned by a bound C++
  object to ensure correct lifetimes.

.. code-block:: cpp

   // Returns by value, data is moved, no copy
   m.def("make_array", []() {
       return xt::xarray<double>{1.0, 2.0, 3.0};
   });

   // Returns by reference from a method, reference_internal keeps self alive
   nb::class_<MyClass>(m, "MyClass")
       .def("get_data", &MyClass::get_data, nb::rv_policy::reference_internal);

.. warning::

   ``rv_policy::reference_internal`` must only be used with **methods** (where
   a ``self`` object exists). It works by preventing the Python wrapper of
   ``self`` from being garbage-collected while the returned array is alive.

   For **free functions** returning references to static or global data, use
   ``rv_policy::reference`` instead, there is no ``self`` to keep alive, and
   using ``reference_internal`` will cause a segmentation fault:

   .. code-block:: cpp

      static xt::xarray<double> global = {1.0, 2.0, 3.0};

      // WRONG, free function has no self → segfault
      m.def("bad", []() -> xt::xarray<double>& {
          return global;
      }, nb::rv_policy::reference_internal);

      // CORRECT, use reference for static/global data
      m.def("good", []() -> xt::xarray<double>& {
          return global;
      }, nb::rv_policy::reference);

Non-owning views
----------------

Non-owning views (``nb::xarray_view<T>`` and ``nb::xtensor_view<T, N>``)
provide **zero-copy** access to the underlying NumPy array's data. They do not
own the memory and instead wrap the Python buffer directly using
``xt::adapt()`` with ``xt::no_ownership()``.

.. code-block:: cpp

   #include <nanobind/xtensor.h>
   namespace nb = nanobind;

   // Zero-copy read access
   m.def("sum_view", [](const nb::xarray_view<double>& a) {
       return xt::sum(a)();
   });

   // Zero-copy write, modifies the Python array in-place
   m.def("mutate", [](nb::xarray_view<double>& a) {
       a(0) = 999.0;
   });

Because views wrap existing memory without copying, they **do not support
implicit type conversion**. If the NumPy array's dtype does not match the
expected scalar type exactly, nanobind will raise a ``TypeError``. This is
intentional: converting to a temporary buffer would create a copy that the view
would reference, leading to a dangling pointer when the temporary is destroyed.

Owning vs. non-owning: when to use which
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+-------------------+---------------------------+---------------------------+
|                   | Owning                    | Non-owning (view)         |
|                   | (``xt::xarray``,          | (``nb::xarray_view``,     |
|                   | ``xt::xtensor``)          | ``nb::xtensor_view``)     |
+===================+===========================+===========================+
| Memory            | Copies data               | Wraps existing buffer     |
+-------------------+---------------------------+---------------------------+
| Type conversion   | Implicit (converts dtype) | Strict (exact dtype only) |
+-------------------+---------------------------+---------------------------+
| Mutability        | Modifications are local   | Modifies Python array     |
+-------------------+---------------------------+---------------------------+
| Use when          | You need an independent   | You need fast, zero-copy  |
|                   | copy or will return data  | access to Python data     |
+-------------------+---------------------------+---------------------------+

Fixed-rank tensors
------------------

Both owning and view types have fixed-rank variants that enforce a specific
number of dimensions at binding time:

.. code-block:: cpp

   // Accepts only 2D arrays, raises TypeError for other ranks
   m.def("matrix_op", [](const xt::xtensor<double, 2>& a) {
       return xt::sin(a);
   });

   // Fixed-rank view
   m.def("matrix_view", [](const nb::xtensor_view<double, 2>& a) {
       return xt::sum(a)();
   });

If a NumPy array with the wrong number of dimensions is passed, the caster
reports failure and nanobind raises a ``TypeError``.

Layout and allocator support
----------------------------

Owning containers
^^^^^^^^^^^^^^^^^

The owning type casters match at the ``xt::xarray_container`` /
``xt::xtensor_container`` class template level, which means they automatically
support non-default layout types and custom allocators:

.. code-block:: cpp

   // Column-major layout
   m.def("column_major", [](const xt::xarray<double, xt::layout_type::column_major>& a) {
       return a * 2.0;
   });

   // Custom allocator
   m.def("custom_alloc", [](const xt::xarray<double, xt::layout_type::row_major,
                                              std::allocator<double>>& a) {
       return a * 2.0;
   });

Because owning types copy data and xtensor's assignment operator handles
stride reordering, a column-major C++ function can accept both C-contiguous
and Fortran-contiguous NumPy arrays, the data is reordered during the copy.

Non-owning views
^^^^^^^^^^^^^^^^

View types accept an optional layout template parameter that defaults to
``xt::layout_type::dynamic``. The layout determines how xtensor iterates over
the data:

.. code-block:: cpp

   // Default (dynamic), accepts any array including non-contiguous slices.
   // Detects the actual memory layout to enable optimizations when contiguous.
   m.def("any_sum", [](const nb::xarray_view<double>& a) {
       return xt::sum(a)();
   });

   // Row-major, only accepts C-contiguous arrays. Enables fast flat-pointer
   // iteration since the layout is known at compile time.
   m.def("fast_sum", [](const nb::xarray_view<double, xt::layout_type::row_major>& a) {
       return xt::sum(a)();
   });

   // Column-major, only accepts Fortran-contiguous arrays.
   m.def("fortran_sum", [](const nb::xarray_view<double, xt::layout_type::column_major>& a) {
       return xt::sum(a)();
   });

The same applies to ``nb::xtensor_view``:

.. code-block:: cpp

   // Default (dynamic), accepts any 2D array layout
   m.def("any_matrix", [](const nb::xtensor_view<double, 2>& a) {
       return xt::sin(a);
   });

   // Row-major, only accepts C-contiguous 2D arrays
   m.def("matrix_op", [](const nb::xtensor_view<double, 2, xt::layout_type::row_major>& a) {
       return xt::sin(a);
   });

   // Column-major, only accepts Fortran-contiguous 2D arrays
   m.def("fortran_matrix", [](const nb::xtensor_view<double, 2, xt::layout_type::column_major>& a) {
       return xt::sin(a);
   });

.. note::

   For 1-dimensional arrays, ``row_major`` and ``column_major`` are
   equivalent (both have a single stride of 1), so a 1D Fortran-contiguous
   array is accepted by a ``row_major`` view and vice versa. Non-contiguous
   1D arrays (e.g., ``a[::2]``) are still rejected by strict-layout views.

The default ``dynamic`` layout accepts any array without restrictions and
detects the input memory order to enable optimizations when the data is
contiguous. Use ``row_major`` or ``column_major`` when you want to enforce
a specific memory order in the function signature and benefit from
compile-time layout optimizations.

.. _xtensor_lazy_expressions:

Returning lazy expressions
--------------------------

xtensor uses *expression templates* for lazy evaluation. Arithmetic operations
like ``a + b`` or ``xt::sin(a) * s + t`` do not compute results immediately,
they return lightweight expression objects that capture **references** to their
operands and evaluate on demand.

nanobind includes an ``xexpression`` caster that materializes lazy
expressions into a NumPy array before returning them to Python. The caster
matches any type satisfying ``xt::is_xexpression<T>``, which covers
expression templates (``a + b``, ``xt::sin(a)``, etc.), strided views
(``xt::strided_view(...)``), and adaptors (``xt::adapt(...)``):

.. code-block:: cpp

   m.def("compute", [](const xt::xarray<double>& a, const double& s, const double& t) {
       return xt::sin(a) * s + t;  // returns a lazy xexpression
   });

The expression is evaluated and wrapped in a NumPy array automatically.

.. warning::

   **Function parameters must be accepted as references when returning lazy
   expressions.** This is critical for correctness.

   Because expression templates hold **references** to their operands (not
   copies), the operands must remain alive until the expression is evaluated.
   nanobind's type casters store converted values on the dispatch stack,
   which remains alive long enough for the expression to be evaluated and
   materialized by the ``xexpression`` caster. But if a parameter is accepted
   by value, the lambda receives a *local copy* and that copy is destroyed
   when the lambda returns, **before** the expression is evaluated.

   .. code-block:: cpp

      // WRONG, undefined behavior! Parameters are by-value copies that are
      // destroyed before the lazy expression is evaluated.
      m.def("bad", [](xt::xarray<double> a, double s) {
          return a * s;  // xexpression references destroyed locals
      });

      // CORRECT, parameters are references to caster-managed data that
      // remain alive until the expression is materialized.
      m.def("good", [](const xt::xarray<double>& a, const double& s) {
          return a * s;
      });

   This applies equally to owning types (``xt::xarray``, ``xt::xtensor``),
   views (``nb::xarray_view``, ``nb::xtensor_view``), and scalar parameters.
   **Always use references** (``const T&`` or ``T&``) for parameters when the
   return type is a lazy expression.

   If you prefer to avoid this pitfall entirely, you can force eager evaluation
   by specifying a concrete return type:

   .. code-block:: cpp

      // Also correct, explicit return type forces evaluation inside the lambda
      m.def("also_good", [](xt::xarray<double> a, double s) -> xt::xarray<double> {
          return a * s;  // evaluated immediately into the return container
      });

Vectorization
-------------

``nb::xvectorize`` wraps a scalar C++ function to operate element-wise on
xtensor arrays, similar to NumPy's ``vectorize``:

.. code-block:: cpp

   double scalar_add(double a, double b) { return a + b; }

   m.def("vectorized_add", nb::xvectorize(scalar_add));

This also works with lambdas:

.. code-block:: cpp

   m.def("vectorized_sin", nb::xvectorize([](double x) {
       return std::sin(x);
   }));

The vectorized function accepts ``nb::xarray_view`` parameters (zero-copy) and
returns a lazy expression that is automatically materialized by the
``xexpression`` caster. Broadcasting follows NumPy semantics.

Supported scalar types
----------------------

The xtensor casters support all scalar types that nanobind's ``nb::ndarray``
supports. Under the hood, nanobind uses the `DLPack
<https://github.com/dmlc/dlpack>`__ protocol for dtype representation, so any
type recognized by DLPack works transparently.

The supported C++ types and their NumPy equivalents are:

+----------------------------+----------------------------+
| C++ type                   | NumPy dtype                |
+============================+============================+
| ``bool``                   | ``numpy.bool_``            |
+----------------------------+----------------------------+
| ``int8_t``                 | ``numpy.int8``             |
+----------------------------+----------------------------+
| ``int16_t``                | ``numpy.int16``            |
+----------------------------+----------------------------+
| ``int32_t``                | ``numpy.int32``            |
+----------------------------+----------------------------+
| ``int64_t``                | ``numpy.int64``            |
+----------------------------+----------------------------+
| ``uint8_t``                | ``numpy.uint8``            |
+----------------------------+----------------------------+
| ``uint16_t``               | ``numpy.uint16``           |
+----------------------------+----------------------------+
| ``uint32_t``               | ``numpy.uint32``           |
+----------------------------+----------------------------+
| ``uint64_t``               | ``numpy.uint64``           |
+----------------------------+----------------------------+
| ``float``                  | ``numpy.float32``          |
+----------------------------+----------------------------+
| ``double``                 | ``numpy.float64``          |
+----------------------------+----------------------------+
| ``std::complex<float>``    | ``numpy.complex64``        |
+----------------------------+----------------------------+
| ``std::complex<double>``   | ``numpy.complex128``       |
+----------------------------+----------------------------+

Example with complex numbers:

.. code-block:: cpp

   m.def("complex_op", [](const xt::xarray<std::complex<double>>& a) {
       return a * std::complex<double>(0.0, 1.0);
   });
