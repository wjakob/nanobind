.. cpp:namespace:: nanobind

.. _ndarray_class:

The ``nb::ndarray<..>`` class
=============================

nanobind can exchange n-dimensional arrays (henceforth "**nd-arrays**") with
popular array programming frameworks including `NumPy <https://numpy.org>`__,
`PyTorch <https://pytorch.org>`__, `TensorFlow <https://www.tensorflow.org>`__,
`JAX <https://jax.readthedocs.io>`__, and `CuPy <https://cupy.dev>`_. It
supports *zero-copy* exchange using two protocols:

-  The classic `buffer
   protocol <https://docs.python.org/3/c-api/buffer.html>`__.

-  `DLPack <https://github.com/dmlc/dlpack>`__, a
   GPU-compatible generalization of the buffer protocol.

nanobind knows how to talk to each framework and takes care of all the
nitty-gritty details.

To use this feature, you must add the include directive

.. code-block:: cpp

   #include <nanobind/ndarray.h>

to your code. Following this, you can bind functions with
:cpp:class:`nb::ndarray\<...\> <ndarray>`-typed parameters and return values.

Array input arguments
---------------------

A function that accepts an :cpp:class:`nb::ndarray\<\> <ndarray>`-typed parameter
(i.e., *without* template parameters) can be called with *any* writable array
from any framework regardless of the device on which it is stored. The
following example binding declaration uses this functionality to inspect the
properties of an arbitrary input array:

.. code-block:: cpp

   m.def("inspect", [](const nb::ndarray<>& a) {
       printf("Array data pointer : %p\n", a.data());
       printf("Array dimension : %zu\n", a.ndim());
       for (size_t i = 0; i < a.ndim(); ++i) {
           printf("Array dimension [%zu] : %zu\n", i, a.shape(i));
           printf("Array stride    [%zu] : %zd\n", i, a.stride(i));
       }
       printf("Device ID = %u (cpu=%i, cuda=%i)\n", a.device_id(),
           int(a.device_type() == nb::device::cpu::value),
           int(a.device_type() == nb::device::cuda::value)
       );
       printf("Array dtype: int16=%i, uint32=%i, float32=%i\n",
           a.dtype() == nb::dtype<int16_t>(),
           a.dtype() == nb::dtype<uint32_t>(),
           a.dtype() == nb::dtype<float>()
       );
   });

Below is an example of what this function does when called with a NumPy
array:

.. code-block:: pycon

   >>> my_module.inspect(np.array([[1,2,3], [3,4,5]], dtype=np.float32))
   Array data pointer : 0x1c30f60
   Array dimension : 2
   Array dimension [0] : 2
   Array stride    [0] : 3
   Array dimension [1] : 3
   Array stride    [1] : 1
   Device ID = 0 (cpu=1, cuda=0)
   Array dtype: int16=0, uint32=0, float32=1

Array constraints
-----------------

In practice, it can often be useful to *constrain* what kinds of arrays
constitute valid inputs to a function. For example, a function expecting CPU
storage would likely crash if given a pointer to GPU memory, and nanobind
should therefore prevent such undefined behavior. The
:cpp:class:`nb::ndarray\<...\> <ndarray>` class accepts template arguments to
specify such constraints. For example the binding below guarantees that the
implementation can only be called with CPU-resident arrays with shape (·,·,3)
containing 8-bit unsigned integers.

.. code-block:: cpp

   using RGBImage = nb::ndarray<uint8_t, nb::shape<-1, -1, 3>, nb::device::cpu>;

   m.def("process", [](RGBImage data) {
       // Double brightness of the MxNx3 RGB image
       for (size_t y = 0; y < data.shape(0); ++y)
           for (size_t x = 0; x < data.shape(1); ++x)
               for (size_t ch = 0; ch < 3; ++ch)
                   data(y, x, ch) = (uint8_t) std::min(255, data(y, x, ch) * 2);
   });

The above example also demonstrates the use of :cpp:func:`operator()
<ndarray::operator()>`, which provides direct read/write access to the array
contents assuming that they are reachable through the CPU's
virtual address space.

.. _ndarray-constraints-1:

Overview
^^^^^^^^

Overall, the following kinds of constraints are available:

- **Data type**: a type annotation like ``float``, ``uint8_t``, etc., constrain the
  numerical representation of the nd-array. Complex arrays (i.e.,
  ``std::complex<float>`` or ``std::complex<double>``) are also supported.

- **Constant arrays**: further annotating the data type with ``const`` makes it possible to call
  the function with constant arrays that do not permit write access. Without
  the annotation, calling the binding would fail with a ``TypeError``.

  You can alternatively accept constant arrays of *any type* by not specifying
  a data type at all and instead passing the :cpp:class:`nb::ro <ro>` annotation.

- **Shape**: The :cpp:class:`nb::shape <shape>` annotation (as in ``nb::shape<-1, 3>``)
  simultaneously constrains the number of array dimensions and the size per
  dimension. A value of ``-1`` leaves the size of the associated dimension
  unconstrained.

  :cpp:class:`nb::ndim\<N\> <ndim>` is shorter when only the dimension
  should be constrained. For example, ``nb::ndim<3>`` is equivalent to
  ``nb::shape<-1, -1, -1>``.

- **Device tags**: annotations like :cpp:class:`nb::device::cpu <device::cpu>`
  or :cpp:class:`nb::device::cuda <device::cuda>` constrain the source device
  and address space.

- **Memory order**: two ordering tags :cpp:class:`nb::c_contig <c_contig>` and
  :cpp:class:`nb::f_contig <f_contig>` enforce contiguous storage in either
  C or Fortran style.

  In the case of matrices, C-contiguous implies row-major and F-contiguous
  implies column-major storage. Without this tag, arbitrary non-contiguous
  representations (e.g. produced by slicing operations) and other unusual
  layouts are permitted.

  This tag is mainly useful when your code directly accesses the array contents
  via :cpp:func:`nb::ndarray\<...\>::data() <ndarray::data>`, while assuming a
  particular layout.

  A third order tag named :cpp:class:`nb::any_contig <any_contig>` accepts
  *both* ``F``- and ``C``-contiguous arrays while rejecting non-contiguous
  ones.

Type signatures
^^^^^^^^^^^^^^^

nanobind displays array constraints in docstrings and error messages. For
example, suppose that we now call the ``process()`` function with an invalid
input. This produces the following error message:

.. code-block:: pycon

   >>> my_module.process(np.zeros(1))

   TypeError: process(): incompatible function arguments. The following argument types are supported:
   1. process(arg: ndarray[dtype=uint8, shape=(*, *, 3), device='cpu'], /) -> None

   Invoked with types: numpy.ndarray

Note that these type annotations are intended for humans–they will not
currently work with automatic type checking tools like `MyPy
<https://mypy.readthedocs.io/en/stable/>`__ (which at least for the time being
don’t provide a portable or sufficiently flexible annotation of n-dimensional
arrays).

Overload resolution
^^^^^^^^^^^^^^^^^^^

A function binding can declare multiple overloads with different nd-array
constraints (e.g., a CPU and a GPU implementation), in which case nanobind will
call the first matching overload. When no perfect match can be found, nanobind
will try each overload once more while performing basic implicit conversions:
it will convert strided arrays into C- or F-contiguous arrays (if requested)
and perform type conversion. This, e.g., makes it possible to call a function
expecting a ``float32`` array with ``float64`` data. Implicit conversions
create temporary nd-arrays containing a copy of the data, which can be
undesirable. To suppress them, add an
:cpp:func:`nb::arg("my_array_arg").noconvert() <arg::noconvert>` or
:cpp:func:`"my_array_arg"_a.noconvert() <arg::noconvert>` argument annotation.

Passing arrays within C++ code
------------------------------

You can think of the :cpp:class:`nb::ndarray <ndarray>` class as a
reference-counted pointer resembling ``std::shared_ptr<T>`` that can be freely
moved or copied. This means that there isn't a big difference between a
function taking
``ndarray`` by value versus taking a constant reference ``const ndarray &``
(i.e., the former does not create an additional copy of the underlying data).

Copies of the :cpp:class:`nb::ndarray <ndarray>` wrapper will point to the same
underlying buffer and increase the reference count until they go out of scope.
You may call freely call :cpp:class:`nb::ndarray\<...\> <ndarray>` methods from
multithreaded code even when the `GIL
<https://wiki.python.org/moin/GlobalInterpreterLock>`__ is not held, for
example to examine the layout of an array and access the underlying storage.

There are two exceptions to this: creating a *new* nd-array object from C++
(discussed :ref:`later <returning-ndarrays>`) and casting it to Python via
the :cpp:func:`ndarray::cast` function both involve Python API calls that require
that the GIL is held.

.. _returning-ndarrays:

Returning arrays from C++ to Python
-----------------------------------

Passing an nd-array across the C++ → Python language barrier is a two-step
process:

1. Creating an :cpp:class:`nb::ndarray\<...\> <ndarray>` instance, which
   only stores *metadata*, e.g.:

   - Where is the data located in memory? (pointer address and device)
   - What is its type and shape?
   - Who owns this data?

   An actual Python object is not yet constructed at this stage.

2. Converting the :cpp:class:`nb::ndarray\<...\> <ndarray>` into a
   Python object of the desired type (e.g. ``numpy.ndarray``).

Normally, step 1 is your responsibility, while step 2 is taken care of by the
binding layer. To understand this separation, let's look at an example. The
``.view()`` function binding below creates a 4×4 column-major NumPy array view
into a ``Matrix4f`` instance.

.. _matrix4f-example:

.. code-block:: cpp

   struct Matrix4f { float m[4][4] { }; };

   using Array = nb::ndarray<float, nb::numpy, nb::shape<4, 4>, nb::f_contig>;

   nb::class_<Matrix4f>(m, "Matrix4f")
       .def(nb::init<>())
       .def("view",
            [](Matrix4f &m){ return Array(data); },
            nb::rv_policy::reference_internal);

In this case:

- step 1 is the ``Array(data)`` call in the lambda function.

- step 2 occurs outside of the lambda function when the nd-array
  :cpp:class:`nb::ndarray\<...\> <ndarray>` :ref:`type caster <type_casters>`
  constructs a NumPy array from the metadata.

Data *ownership* is an important aspect of this two-step process: because the
NumPy array points directly into the storage of another object, nanobind must
keep the ``Matrix4f`` instance alive as long as the NumPy array exists, which
the :cpp:enumerator:`reference_internal <rv_policy::reference_internal>` return
value policy signals to nanobind. More generally, wrapping an existing memory
region without copying requires that that this memory region remains valid
throughout the lifetime of the created array (more on this point :ref:`shortly
<ndarray-ownership>`).

Recall the discussion of the :ref:`nd-array constraint <ndarray-constraints-1>`
template parameters. For the return path, you will generally want to add a
*framework* template parameter to the nd-array parameters that indicates the
desired Python type.

- :cpp:class:`nb::numpy <numpy>`: create a ``numpy.ndarray``.
- :cpp:class:`nb::pytorch <pytorch>`: create a ``torch.Tensor``.
- :cpp:class:`nb::tensorflow <tensorflow>`: create a ``tensorflow.python.framework.ops.EagerTensor``.
- :cpp:class:`nb::jax <jax>`: create a ``jaxlib.xla_extension.DeviceArray``.
- :cpp:class:`nb::cupy <cupy>`: create a ``cupy.ndarray``.
- No framework annotation. In this case, nanobind will create a raw Python
  ``dltensor`` `capsule <https://docs.python.org/3/c-api/capsule.html>`__
  representing the `DLPack <https://github.com/dmlc/dlpack>`__ metadata.

This annotation also affects the auto-generated docstring of the function,
which in this case becomes:

.. code-block:: python

   view(self) -> numpy.ndarray[float32, shape=(4, 4), order='F']

Note that the framework annotation only plays a role when passing arrays from
C++ to Python. It does not constrain the reverse direction (for example, a
PyTorch array would still be accepted by a function taking the ``Array`` alias
defined above as input. For this reason, you may want to add a
:cpp:class:`nb::device::cpu <device::cpu>` device annotation).

Dynamic array configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The previous example was rather simple because all the array configuration was fully known
at compile time and specified via the :cpp:class:`nb::ndarray\<...\> <ndarray>`
template parameters. In general, there are often dynamic aspects of the
configuration that must be explicitly passed to the constructor. Its signature
(with some simplifications) is given below. See the
:ref:`ndarray::ndarray() <ndarray_dynamic_constructor>` documentation for a more detailed specification
and another variant of the constructor.

.. code-block:: cpp

   ndarray(void *data,
           std::initializer_list<size_t> shape = { },
           handle owner = { },
           std::initializer_list<int64_t> strides = { },
           dlpack::dtype dtype = ...,
           int device_type = ...,
           int device_id = 0,
           char order = ...) { .. }

The parameters have the following role:

- ``data``: CPU/GPU/.. memory address of the data.
- ``shape``: number of dimensions and size along each axis.
- ``owner``: a Python object owning the storage, which must be
  kept alive while the array object exists.
- ``strides``: specifies the data layout in memory. You only need to specify
  this parameter if it has a non-standard ``order`` (e.g., if it is non-contiguous).
  Note that the ``strides`` count elements, not bytes.
- ``dtype`` data type (floating point, signed/unsigned integer), bit depth.
- ``device_type`` and ``device_id``: device type and number, e.g., for multi-GPU setups.
- ``order``: coefficient memory order. Default: ``'C'`` (C-style) ordering,
  specify ``'F'`` for Fortran-style ordering.

The parameters generally have inferred defaults based on the array's
compile-time template parameters. Passing them explicitly overrides these
defaults with information available at runtime.

.. _ndarray-ownership:

Data ownership
^^^^^^^^^^^^^^

Let's look at a fancier example that uses the constructor arguments explained
above to return a dynamically sized 2D array. This example also shows another
mechanism to express *data ownership*:

.. code-block:: cpp

   m.def("create_2d",
         [](size_t rows, size_t cols) {
             // Allocate a memory region an initialize it
             float *data = new float[rows * cols];
             for (size_t i = 0; i < rows * cols; ++i)
                 data[i] = (float) i;

             // Delete 'data' when the 'owner' capsule expires
             nb::capsule owner(data, [](void *p) noexcept {
                delete[] (float *) p;
             });

             return nb::ndarray<nb::numpy, float, nb::ndim<2>>(
                 /* data = */ data,
                 /* shape = */ { rows, cols },
                 /* owner = */ owner
             );
   });

The ``owner`` parameter should specify a Python object, whose continued
existence keeps the underlying memory region alive. Nanobind will temporarily
increase the ``owner`` reference count in the  :cpp:func:`ndarray::ndarray()`
constructor and then decrease it again when the created NumPy array expires.

The above example binding returns a *new* memory region that should be deleted
when it is no longer in use. This is done by creating a
:cpp:class:`nb::capsule`, an opaque pointer with a destructor callback that
runs at this point and takes care of cleaning things up.

If there is already an existing Python object, whose existence guarantees that
it is safe to access the provided storage region, then you may alternatively
pass this object as the ``owner``---nanobind will make sure that this object
isn't deleted as long as the created array exists. If the owner is a C++ object
with an associated Python instance, you may use :cpp:func:`nb::find() <find>`
to look up the associated Python object. When binding methods, you can use the
:cpp:enumerator:`reference_internal <rv_policy::reference_internal>` return
value policy to specify the implicit ``self`` argument as the ``owner`` upon
return, which was done in the earlier ``Matrix4f`` :ref:`example
<matrix4f-example>`.

.. warning::

   If you do not specify an owner and use a return value policy like
   :cpp:enumerator:`rv_policy::reference` (see also the the section on
   :ref:`nd-array return value policies <ndarray_rvp>`), nanobind will assume
   that the array storage **remains valid forever**.

   This is one of the most frequent issues reported on the nanobind GitHub
   repository: users forget to think about data ownership and run into data
   corruption.

   If there isn't anything keeping the array storage alive, it will likely be
   released and reused at some point, while stale arrays still point to the
   associated memory region (i.e., a classic "use-after-free" bug).

In more advanced situations, it may be helpful to have a capsule that manages
the lifetime of data structures containing *multiple* storage regions. The same
capsule can be referenced from different nd-arrays and will call the deleter
when all of them have expired:

.. code-block:: cpp

   m.def("return_multiple", []() {
       struct Temp {
           std::vector<float> vec_1;
           std::vector<float> vec_2;
       };

       Temp *temp = new Temp();
       temp->vec_1 = std::move(...);
       temp->vec_2 = std::move(...);

       nb::capsule deleter(temp, [](void *p) noexcept {
           delete (Temp *) p;
       });

       size_t size_1 = temp->vec_1.size();
       size_t size_2 = temp->vec_2.size();

       return std::make_pair(
           nb::ndarray<nb::pytorch, float>(temp->vec_1.data(), { size_1 }, deleter),
           nb::ndarray<nb::pytorch, float>(temp->vec_2.data(), { size_2 }, deleter)
       );
   });

.. _ndarray_rvp:

Return value policies
^^^^^^^^^^^^^^^^^^^^^

Function bindings that return nd-arrays can specify return value policy
annotations to determine whether or not a copy should be made. They are
interpreted as follows:

- The default :cpp:enumerator:`rv_policy::automatic` and
  :cpp:enumerator:`rv_policy::automatic_reference` policies cause the array to
  be copied when it has no owner and when it is not already associated with a
  Python object.

- The policy :cpp:enumerator:`rv_policy::reference` references an existing
  memory region and never copies.

- :cpp:enumerator:`rv_policy::copy` always copies.

- :cpp:enumerator:`rv_policy::none` refuses the cast unless the array is
  already associated with an existing Python object (e.g. a NumPy array), in
  which case that object is returned.

- :cpp:enumerator:`rv_policy::reference_internal` retroactively sets the
  nd-array's ``owner`` field to a method's ``self`` argument. It fails with an
  error if there is already a different owner.

- :cpp:enumerator:`rv_policy::move` is unsupported and demoted to
  :cpp:enumerator:`rv_policy::copy`.

.. _ndarray-temporaries:

Returning temporaries
^^^^^^^^^^^^^^^^^^^^^

Returning nd-arrays from temporaries (e.g. stack-allocated memory) requires
extra precautions.

.. code-block:: cpp
   :emphasize-lines: 4,5

   using Vector3f = nb::ndarray<float, nb::numpy, nb::shape<3>>;
   m.def("return_vec3", []{
       float data[] { 1, 2, 3 };
       // !!! BAD don't do this !!!
       return Vector3f(data);
   });

Recall the discussion at the :ref:`beginning <returning-ndarrays>` of this
subsection. The :cpp:class:`nb::ndarray\<...\> <ndarray>` constructor only
creates *metadata* describing this array, with the actual array creation
happening *after* of the function call. That isn't safe in this case because
``data`` is a temporary on the stack that is no longer valid once the function
has returned. To fix this, we could use the :cpp:func:`nb::cast() <cast>`
method to *force* the array creation in the body of the function:

.. code-block:: cpp
   :emphasize-lines: 4,5

   using Vector3f = nb::ndarray<float, nb::numpy, nb::shape<3>>;
   m.def("return_vec3", []{
       float data[] { 1, 2, 3 };
       // OK.
       return nb::cast(Vector3f(data));
   });

While safe, one unfortunate aspect of this change is that the function now has
a rather non-informative docstring ``return_vec3() -> object``, which is a
consequence of :cpp:func:`nb::cast() <cast>` returning a generic
:cpp:class:`nb::object <object>`.

To fix this, you can use the nd-array :cpp:func:`.cast() <ndarray::cast>`
method, which is like :cpp:func:`nb::cast() <cast>` except that it preserves
the type signature:

.. code-block:: cpp
   :emphasize-lines: 4,5

   using Vector3f = nb::ndarray<float, nb::numpy, nb::shape<3>>;
   m.def("return_vec3", []{
       float data[] { 1, 2, 3 };
       // Perfect.
       return Vector3f(data).cast();
   });

.. _ndarray-nonstandard:

Nonstandard arithmetic types
----------------------------

Low or extended-precision arithmetic types (e.g., ``int128``, ``float16``,
``bfloat16``) are sometimes used but don't have standardized C++ equivalents.
If you wish to exchange arrays based on such types, you must register a partial
overload of ``nanobind::detail::dtype_traits`` to inform nanobind about it.

You are expressively allowed to create partial overloads of this class despite
it being in the ``nanobind::detail`` namespace.

For example, the following snippet makes ``__fp16`` (half-precision type on
``aarch64``) available by  providing

1. ``value``, a DLPack ``nanobind::dlpack::dtype`` type descriptor, and
2. ``name``, a type name for use in docstrings and error messages.

.. code-block:: cpp

   namespace nanobind::detail {
       template <> struct dtype_traits<__fp16> {
           static constexpr dlpack::dtype value {
               (uint8_t) dlpack::dtype_code::Float, // type code
               16, // size in bits
               1   // lanes (simd), usually set to 1
           };
           static constexpr auto name = const_name("float16");
       };
   }

.. _ndarray-views:

Fast array views
----------------

The following advice applies to performance-sensitive CPU code that reads and
writes arrays using loops that invoke :cpp:func:`nb::ndarray\<...\>::operator()
<ndarray::operator()>`. It does not apply to GPU arrays because they are
usually not accessed in this way.

Consider the following snippet, which fills a 2D array with data:

.. code-block:: cpp

   void fill(nb::ndarray<float, nb::ndim<2>, nb::c_contig, nb::device::cpu> arg) {
       for (size_t i = 0; i < arg.shape(0); ++i)
           for (size_t j = 0; j < arg.shape(1); ++j)
               arg(i, j) = /* ... */;
   }

While functional, this code is not perfect. The problem is that to compute the
address of an entry, ``operator()`` accesses the DLPack array descriptor. This
indirection can break certain compiler optimizations.

nanobind provides the method :cpp:func:`ndarray\<...\>::view() <ndarray::view>`
to fix this. It creates a tiny data structure that provides all information
needed to access the array contents, and which can be held within CPU
registers. All relevant compile-time information (:cpp:class:`nb::ndim <ndim>`,
:cpp:class:`nb::shape <shape>`, :cpp:class:`nb::c_contig <c_contig>`,
:cpp:class:`nb::f_contig <f_contig>`) is materialized in this view, which
enables constant propagation, auto-vectorization, and loop unrolling.

An improved version of the example using such a view is shown below:

.. code-block:: cpp

   void fill(nb::ndarray<float, nb::ndim<2>, nb::c_contig, nb::device::cpu> arg) {
       auto v = arg.view(); // <-- new!

       for (size_t i = 0; i < v.shape(0); ++i) // Important; use 'v' instead of 'arg' everywhere in loop
           for (size_t j = 0; j < v.shape(1); ++j)
               v(i, j) = /* ... */;
   }

Note that the view performs no reference counting. You may not store it in a way
that exceeds the lifetime of the original array.

When using OpenMP to parallelize expensive array operations, pass the
``firstprivate(view_1, view_2, ...)`` so that each worker thread can copy the
view into its register file.

.. code-block:: cpp

   auto v = arg.view();
   #pragma omp parallel for schedule(static) firstprivate(v)
   for (...) { /* parallel loop */ }

.. _ndarray-runtime-specialization:

Specializing views at runtime
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As mentioned earlier, element access via ``operator()`` only works when both
the array's scalar type and its dimension are specified within the type (i.e.,
when they are known at compile time); the same is also true for array views.
However, sometimes, it is useful that a function can be called with different
array types.

You may use the :cpp:func:`ndarray\<...\>::view() <ndarray::view>` method to
create *specialized* views if a run-time check determines that it is safe to
do so. For example, the function below accepts contiguous CPU arrays and
performs a loop over a specialized 2D ``float`` view when the array is of
this type.

.. code-block:: cpp

   void fill(nb::ndarray<nb::c_contig, nb::device::cpu> arg) {
       if (arg.dtype() == nb::dtype<float>() && arg.ndim() == 2) {
           auto v = arg.view<float, nb::ndim<2>>(); // <-- new!

           for (size_t i = 0; i < v.shape(0); ++i) {
               for (size_t j = 0; j < v.shape(1); ++j) {
                   v(i, j) = /* ... */;
               }
           }
        } else { /* ... */ }
   }

Array libraries
---------------

The Python `array API standard <https://data-apis.org/array-api/latest/purpose_and_scope.html>`__
defines a common interface and interchange protocol for nd-array libraries. In particular, to
support inter-framework data exchange, custom array types should implement the

- `__dlpack__ <https://data-apis.org/array-api/latest/API_specification/generated/array_api.array.__dlpack__.html#array_api.array.__dlpack__>`__ and
- `__dlpack_device__ <https://data-apis.org/array-api/latest/API_specification/generated/array_api.array.__dlpack_device__.html#array_api.array.__dlpack_device__>`__

methods. This is easy thanks to the nd-array integration in nanobind. An example is shown below:

.. code-block:: cpp

   nb::class_<MyArray>(m, "MyArray")
      // ...
      .def("__dlpack__", [](nb::kwargs kwargs) {
          return nb::ndarray<>( /* ... */);
      })
      .def("__dlpack_device__", []() {
          return std::make_pair(nb::device::cpu::value, 0);
      });

Returning a raw :cpp:class:`nb::ndarray <ndarray>` without framework annotation
will produce a DLPack capsule, which is what the interface expects.

The ``kwargs`` argument can be used to provide additional parameters (for
example to request a copy), please see the DLPack documentation for details.
Note that nanobind does not yet implement the versioned DLPack protocol. The
version number should be ignored for now.

Frequently asked questions
--------------------------

Why does my returned nd-array contain corrupt data?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If your nd-array bindings lead to undefined behavior (data corruption or
crashes), then this is usually an ownership issue. Please review the section on
:ref:`data ownership <ndarray-ownership>` for details.

Why does nanobind not accept my NumPy array?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When binding a function that takes an ``nb::ndarray<T, ...>`` as input, nanobind
will by default require that array to be writable. This means that the function
cannot be called using NumPy arrays that are marked as constant.

If you wish your function to be callable with constant input, either change the
parameter to ``nb::ndarray<const T, ...>`` (if the array is parameterized by
type), or write ``nb::ndarray<nb::ro>`` to accept a read-only array of any
type.

Limitations related to ``dtypes``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _dtype_restrictions:

Libraries like `NumPy <https://numpy.org>`__ support arrays with flexible
internal representations (*dtypes*), including

- Floating point and integer arrays with various bit depths

- Null-terminated strings

- Arbitrary Python objects

- Heterogeneous data structures composed of multiple fields

nanobind's :cpp:class:`nb::ndarray\<...\> <ndarray>` is based on the `DLPack
<https://github.com/dmlc/dlpack>`__ array exchange protocol, which causes it to
be more restrictive. Presently supported dtypes include signed/unsigned
integers, floating point values, complex numbers, and boolean values. Some
:ref:`nonstandard arithmetic types <ndarray-nonstandard>` can be supported as
well.

Nanobind can receive and return *read-only* arrays via the buffer protocol when
exhanging data with NumPy. The DLPack interface currently ignores this
annotation.
