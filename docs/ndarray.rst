.. cpp:namespace:: nanobind

.. _ndarray_class:

The ``nb::ndarray<..>`` class
=============================

nanobind can exchange n-dimensional arrays (henceforth "**ndarrays**") with
popular array programming frameworks including `NumPy <https://numpy.org>`_,
`PyTorch <https://pytorch.org>`_, `TensorFlow <https://www.tensorflow.org>`_,
and `JAX <https://jax.readthedocs.io>`_. It supports *zero-copy* exchange using
two protocols:

-  The classic `buffer
   protocol <https://docs.python.org/3/c-api/buffer.html>`_.

-  `DLPack <https://github.com/dmlc/dlpack>`_, a
   GPU-compatible generalization of the buffer protocol.

nanobind knows how to talk to each framework and takes care
of all the nitty-gritty details.

To use this feature, you must add the include directive

.. code-block:: cpp

   #include <nanobind/ndarray.h>

to your code. Following this, you can bind functions with
:cpp:class:`nb::ndarray\<...\> <ndarray>`-typed parameters and return values.

Binding functions that take arrays as input
-------------------------------------------

A function that accepts a :cpp:class:`nb::ndarray\<\> <ndarray>`-typed parameter
(i.e., *without* template parameters) can be called with *any* array
from any framework regardless of the device on which it is stored. The
following example binding declaration uses this functionality to inspect the
properties of an arbitrary input array:

.. code-block:: cpp

   m.def("inspect", [](nb::ndarray<> a) {
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
should therefore prevent such undefined behavior.
:cpp:class:`nb::ndarray\<...\> <ndarray>` accepts template arguments to
specify such constraints. For example the function interface below
guarantees that the implementation is only invoked when it is provided with
a ``MxNx3`` array of 8-bit unsigned integers that is furthermore stored
contiguously in CPU memory using a C-style array ordering convention.

.. code-block:: cpp

   m.def("process", [](nb::ndarray<uint8_t, nb::shape<nb::any, nb::any, 3>,
                                   nb::c_contig, nb::device::cpu> data) {
       // Double brightness of the MxNx3 RGB image
       for (size_t y = 0; y < data.shape(0); ++y)
           for (size_t x = 0; x < data.shape(1); ++x)
               for (size_t ch = 0; ch < 3; ++ch)
                   data(y, x, ch) = (uint8_t) std::min(255, data(y, x, ch) * 2);

   });

The above example also demonstrates the use of
:cpp:func:`nb::ndarray\<...\>::operator() <ndarray::operator()>`, which
provides direct (i.e., high-performance) read/write access to the array
data. Note that this function is only available when the underlying data
type and ndarray rank are specified. It should only be used when the
array storage is reachable via CPU’s virtual memory address space.

.. _ndarray-constraints-1:

Constraint types
~~~~~~~~~~~~~~~~

The following constraints are available

- A scalar type (``float``, ``uint8_t``, etc.) constrains the representation
  of the ndarray.

- The :cpp:class:`nb::shape <shape>` annotation simultaneously constrains the
  number of array dimensions and the size per dimension. A :cpp:var:`nb::any
  <any>` entry leaves the corresponding dimension unconstrained.

- Device tags like :cpp:class:`nb::device::cpu <device::cpu>` or
  :cpp:class:`nb::device::cuda <device::cuda>` constrain the source device
  and address space.

- Two ordering tags :cpp:class:`nb::c_contig <c_contig>` and
  :cpp:class:`nb::f_contig <f_contig>` enforce contiguous storage in either
  C or Fortran style. In the case of matrices, C-contiguous corresponds to
  row-major storage, and F-contiguous corresponds to column-major storage.
  Without this tag, non-contiguous representations (e.g. produced by slicing
  operations) and other unusual layouts are permitted.

Passing arrays in C++ code
--------------------------

:cpp:class:`nb::ndarray\<...\> <ndarray>` behaves like a shared pointer with
builtin reference counting: it can be moved or copied within C++ code.
Copies will point to the same underlying buffer and increase the reference
count until they go out of scope. It is legal call
:cpp:class:`nb::ndarray\<...\> <ndarray>` members from multithreaded code even
when the `GIL <https://wiki.python.org/moin/GlobalInterpreterLock>`_ is not
held.

Constraints in type signatures
------------------------------

nanobind displays array constraints in docstrings and error messages. For
example, suppose that we now call the ``process()`` function with an invalid
input. This produces the following error message:

.. code-block:: pycon

   >>> my_module.process(ndarray=np.zeros(1))

   TypeError: process(): incompatible function arguments. The following argument types are supported:
   1. process(arg: ndarray[dtype=uint8, shape=(*, *, 3), order='C', device='cpu'], /) -> None

   Invoked with types: numpy.ndarray

Note that these type annotations are intended for humans–they will not
currently work with automatic type checking tools like `MyPy
<https://mypy.readthedocs.io/en/stable/>`_ (which at least for the time being
don’t provide a portable or sufficiently flexible annotation of n-dimensional
arrays).

Arrays and function overloads
-----------------------------

A bound function taking a ndarray argument can declare multiple overloads
with different constraints (e.g. a CPU and GPU implementation), in which
case the first first matching overload will be called. When no perfect
match could be found, nanobind will try each overload once more while
performing basic implicit conversions: it will convert strided arrays
into C- or F-contiguous arrays (if requested) and perform type
conversion. This, e.g., makes possible to call a function expecting a
``float32`` array with ``float64`` data. Implicit conversions create
temporary ndarrays containing a copy of the data, which can be
undesirable. To suppress then, add a
:cpp:func:`nb::arg("ndarray").noconvert() <arg::noconvert>`
:cpp:func:`"ndarray"_a.noconvert() <arg::noconvert>` or
function binding annotation.

Binding functions that return arrays
------------------------------------

To return a ndarray from C++ code, you must indicate its type, shape, a pointer
to CPU/GPU memory, and what framework (NumPy/..) should be used to encapsulate
the data.

The following simple binding declaration shows how to return a ``2x4``
NumPy floating point matrix.

.. code-block:: cpp

   const float data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

   m.def("ret_numpy", []() {
       size_t shape[2] = { 2, 4 };
       return nb::ndarray<nb::numpy, float, nb::shape<2, nb::any>>(
           data, /* ndim = */ 2, shape);
   });

The auto-generated docstring of this function is:

.. code-block:: python

   ret_pytorch() -> np.ndarray[float32, shape=(2, *)]

Calling it in Python yields

.. code-block:: python

   array([[1., 2., 3., 4.],
          [5., 6., 7., 8.]], dtype=float32)

The following additional ndarray declarations are possible for return
values:

-  :cpp:class:`nb::numpy <numpy>`. Returns the ndarray as a ``numpy.ndarray``.
-  :cpp:class:`nb::pytorch <pytorch>`. Returns the ndarray as a ``torch.Tensor``.
-  :cpp:class:`nb::tensorflow <tensorflow>`. Returns the ndarray as a
   ``tensorflow.python.framework.ops.EagerTensor``.
-  :cpp:class:`nb::jax <jax>`. Returns the ndarray as a
   ``jaxlib.xla_extension.DeviceArray``.
-  No framework annotation. In this case, nanobind will return a raw
   Python ``dltensor``
   `capsule <https://docs.python.org/3/c-api/capsule.html>`_
   representing the `DLPack <https://github.com/dmlc/dlpack>`_
   metadata.

Note that shape and order annotations like :cpp:class:`nb::shape <shape>` and
:cpp:class:`nb::c_contig <c_contig>` enter into the docstring, but nanobind
won’t spend time on additional checks. It trusts that your method returns what
it declares. Furthermore, non-CPU ndarrays must be explicitly indicate the
device type and device ID using special parameters of the :cpp:func:`ndarray()
<ndarray::ndarray()>` constructor shown below. Device types indicated via
template arguments, e.g., ``nb::ndarray<..., nb::device::cuda>``, are only used
for decorative purposes to generate an informative function docstring.

The full signature of the ndarray constructor is:

.. code-block:: cpp

   ndarray(void *value,
           size_t ndim,
           const size_t *shape,
           handle owner = nb::handle(),
           const int64_t *strides = nullptr,
           dlpack::dtype dtype = nb::dtype<Scalar>(),
           int32_t device_type = nb::device::cpu::value,
           int32_t device_id = 0) { .. }

If no ``strides`` parameter is provided, the implementation will assume
a C-style ordering. Both ``strides`` and ``shape`` will be copied by the
constructor, hence the targets of these pointers don’t need to remain
valid following the call.

The ``owner`` parameter can be used to keep another Python object alive
while the ndarray data is referenced by a consumer. This mechanism can be
used to implement a data destructor as follows:

.. code-block:: cpp

   m.def("ret_pytorch", []() {
       // Dynamically allocate 'data'
       float *data = new float[8] { 1, 2, 3, 4, 5, 6, 7, 8 };
       size_t shape[2] = { 2, 4 };

       // Delete 'data' when the 'owner' capsule expires
       nb::capsule owner(data, [](void *p) noexcept {
          delete[] (float *) p;
       });

       return nb::ndarray<nb::pytorch, float>(data, 2, shape, owner);
   });

In other situations, it may be helpful to have the capsule manage the lifetime
of a custom data structure that contains one or multiple containers. The same
capsule can be referenced from multiple ndarrays and will call the deleter
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

       size_t shape_1[1] = { temp->vec_1.size() };
       size_t shape_2[1] = { temp->vec_2.size() };

       return std::make_pair(
           nb::ndarray<nb::pytorch, float>(temp->vec_1.data(), 1, shape_1, deleter),
           nb::ndarray<nb::pytorch, float>(temp->vec_2.data(), 1, shape_2, deleter)
       );
   });
