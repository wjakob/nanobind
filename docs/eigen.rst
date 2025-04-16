.. cpp:namespace:: nanobind

.. _eigen:

The *Eigen* linear algebra library
==================================

`Eigen <http://eigen.tuxfamily.org>`__ is a header-only C++ library for linear
algebra that offers dense and sparse matrix types along with a host of
algorithms that operate on them. Owing to its widespread use in many scientific
projects, nanobind includes custom type casters that enable bidirectional
conversion between Eigen and Python array programming libraries.

These casters build on the previously discussed :ref:`n-dimensional array
<ndarray_class>` class. You can therefore think of this section as an easier
interface to the same features that is preferable if your project uses Eigen.

Dense matrices and vectors
--------------------------

Add the following include directive to your binding code to exchange dense
Eigen types:

.. code-block:: cpp

   #include <nanobind/eigen/dense.h>

Following this, you should be able to bind functions that accept and return
values of type ``Eigen::Matrix<..>``, ``Eigen::Array<..>``,
``Eigen::Vector<..>``, ``Eigen::Ref<..>``, ``Eigen::Map<..>``, and their
various specializations.  Unevaluated expression templates are also supported.

nanobind may need to evaluate or copy the matrix/vector contents during type
casting, which is sometimes undesirable. The following cases explain when
copying is needed, and how it can be avoided.

C++ → Python
^^^^^^^^^^^^

Consider the following C++ function returning a dense Eigen type
(``Eigen::MatrixXf`` in this example). The bound Python version of ``f()``
returns this data in the form of a ``numpy.ndarray``.

.. code-block:: cpp

   Eigen::MatrixXf f() { ... }

If the C++ function returns *by value*, and when the Eigen type represents an
evaluated expression, nanobind will capture and wrap it in a NumPy array
without making a copy. All other cases (returning by reference, returning an
unevaluated expression template) either evaluate or copy the array.

.. warning::

   It can be tempting to bind functions that directly return Eigen expressions,
   such as such the innocent-looking vector sum below:

   .. code-block:: cpp

      m.def("sum", [](Eigen::Vector3f a, Eigen::Vector3d b) { return a + b; });

   However, note that this example triggers undefined behavior. The problem is
   that the sum ``a + b`` is an *expression template*, which provides the means
   to evaluate the expression at some later point. The expression references
   variables on the stack that no longer exist when when the expression is
   evaluated by the caller. The issue is not related to nanobind (i.e., this is
   also a bug in pure Eigen code).

   To fix this you can

   1. Specify a return type, e.g.,

      .. code-block:: cpp

         m.def("sum", [](Eigen::Vector3f a, Eigen::Vector3d b) -> Eigen::Vector3d { return a + b; });

      This forces an evaluation of the expression into a container that *owns*
      the underlying storage.

   2. Invoke ``Eigen::DenseBase::eval()``, which is equivalent and potentially
      more compact and flexible.

      .. code-block:: cpp

         m.def("sum", [](Eigen::Vector3f a, Eigen::Vector3d b) { return (a + b).eval(); });

   3. If the expression to be returned only references function arguments,
      then you can turn the arguments themselves into references:

      .. code-block:: cpp

         m.def("sum", [](const Eigen::Vector3f &a, const Eigen::Vector3d &b) { return a + b; });

      This is safe, because the nanobind type casters keep the referenced
      objects alive until the expression has been evaluated.

Python → C++
^^^^^^^^^^^^

The reverse direction is more tricky. Consider the following 3
functions taking variations of a dense ``Eigen::MatrixXf``:

.. code-block:: cpp

   void f1(const Eigen::MatrixXf &x) { ... }
   void f2(const Eigen::Ref<Eigen::MatrixXf> &x) { ... }
   void f3(const nb::DRef<Eigen::MatrixXf> &x) { ... }

The Python bindings of these three functions can be called using any of a
number of different CPU-resident 2D array types (NumPy arrays,
PyTorch/Tensorflow/JAX tensors, etc.). However, the following limitations
apply:

- ``f1()`` will always perform a copy of the array contents when called from
  Python. This is because ``Eigen::MatrixXf`` is designed to *own* the
  underlying storage, which is sadly incompatible with the idea of creating a
  view of an existing Python array.

- ``f2()`` very likely copies as well! This may seem non-intuitive, since
  ``Eigen::Ref<..>`` exists to avoid this exact problem.

  The problem is that Eigen normally expects a very specific memory layout
  (Fortran/column-major layout), while Python array frameworks actually use the
  *opposite* by default (C/row-major layout). Array slices are even more
  problematic and always require a copy.

- ``f3()`` uses :cpp:type:`nb::DRef <DRef>` to support *any* memory layout
  (row-major, column-major, slices) without copying. It may still perform an
  implicit conversion when called with the *wrong data type*---for example, the
  function expects a single precision array, but NumPy matrices often use
  double precision.

  If that is undesirable, you may bind the function as follows, in which case
  nanobind will report a ``TypeError`` if an implicit conversion would be
  needed.

  .. code-block:: cpp

     m.def("f1", &f1, nb::arg("x").noconvert());

  This parameter passing convention can also be used to mutate function
  parameters, e.g.:

  .. code-block:: cpp

     void f4(nb::DRef<Eigen::MatrixXf> x) { x *= 2; }

Maps
----

Besides ``Eigen::Ref<...>``, nanobind also supports binding functions that take
and return ``Eigen::Map<...>``. The underlying map type caster strictly
prevents conversion of incompatible inputs into an ``Eigen::Map<...>`` when
this would require implicit layout or type conversion. This restriction exists
because the primary purpose of this interface is to efficiently access existing
memory without conversion overhead. When binding functions that return
``Eigen::Map<...>``, you must ensure that the mapped memory remains valid
throughout the map's lifetime. This typically requires appropriate lifetime
annotations (such as :cpp:enumerator:`rv_policy::reference_internal` or
:cpp:struct:`keep_alive`) to prevent access to memory that has been deallocated
on the C++ side.

Sparse matrices
---------------

Add the following include directive to your binding code to exchange sparse
Eigen types:

.. code-block:: cpp

   #include <nanobind/eigen/sparse.h>

The ``Eigen::SparseMatrix<..>`` and ``Eigen::Map<Eigen::SparseMatrix<..>>``
types map to either ``scipy.sparse.csr_matrix`` or ``scipy.sparse.csc_matrix``
depending on whether row- or column-major storage is used. The previously
mentioned precautions related to returning dense maps also apply in the sparse
case.

There is no support for Eigen sparse vectors because an equivalent type does
not exist as part of ``scipy.sparse``.
