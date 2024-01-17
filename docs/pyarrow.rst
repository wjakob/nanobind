.. _pyarrow:

PyArrow Bindings
================

nanobind can exchange ``pyarrow`` objects via a ``std::shared_ptr<..>``. To get started you have to

.. code-block:: cpp

   #include <nanobind/pyarrow/pyarrow_import.h>

and make sure to call the following `pyarrow initialization <https://arrow.apache.org/docs/python/integration/extending.html#_CPPv4N5arrow14import_pyarrowEv>`__ on top of your module definition

.. code-block:: cpp

    NB_MODULE(test_pyarrow_ext, m) {
        static nanobind::detail::pyarrow::ImportPyarrow module;
        // ...
    }

The type caster headers are structured in a similar form than the headers in ``pyarrow`` (``array_primitive.h``, ``array_binary.h``, etc) itself:

.. list-table::
  :widths: 42 48
  :header-rows: 1

  * - Types
    - Type caster header
  * - ``Array``, ``DoubleArray``, ``Int64Array``, ...
    - ``#include <nanobind/pyarrow/array_primitive.h>``
  * - ``BinaryArray``, ``LargeBinaryArray``, ``StringArray``, ``LargeStringArray``, ``FixedSizeBinaryArray``
    - ``#include <nanobind/pyarrow/array_binary.h>``
  * - ``ListArray``, ``LargeListArray``, ``MapArray``, ``FixedSizeListArray``, ``StructArray``, ``UnionArray``, ``SparseUnionArray``, ``DenseUnionArray``
    - ``#include <nanobind/pyarrow/array_nested.h>``
  * - ``ChunkedArray``
    - ``#include <nanobind/pyarrow/chunked_array.h>``
  * - ``Table``
    - ``#include <nanobind/pyarrow/table.h>``
  * - ``RecordBatch``
    - ``#include <nanobind/pyarrow/record_batch.h>``
  * - ``Field``, ``Schema``
    - ``#include <nanobind/pyarrow/type.h>``
  * - ``Scalars``
    - ``#include <nanobind/pyarrow/scalar.h>``
  * - ``DataTypes``
    - ``#include <nanobind/pyarrow/datatype.h>``
  * - ``Buffer``, ``ResizableBuffer``, ``MutableBuffer``
    - ``#include <nanobind/pyarrow/buffer.h>``
  * - ``Tensor``, ``NumericTensor<..>``
    - ``#include <nanobind/pyarrow/tensor.h>``
  * - ``SparseCOOTensor``, ``SparseCSCMatrix``, ``SparseCSFTensor``, ``SparseCSRMatrix``
    - ``#include <nanobind/pyarrow/sparse_tensor.h>``

**Example**: The following code snippet shows how to create bindings for a ``pyarrow.DoubleArray``:

.. code-block:: cpp

    #include <memory>
    #include <nanobind/nanobind.h>

    #include <nanobind/pyarrow/pyarrow_import.h>
    #include <nanobind/pyarrow/array_primitive.h>

    namespace nb = nanobind;

    NB_MODULE(test_pyarrow_ext, m) {
        static nb::detail::pyarrow::ImportPyarrow module;
        m.def("my_pyarrow_function", [](std::shared_ptr<arrow::DoubleArray> arr) {
                auto data = arr->data()->Copy();
                return std::make_shared<arrow::DoubleArray>(std::move(data));
            }
        );
    }

If you want to consume the ``C++`` artifacts as distributed by the ``PyPi`` ``pyarrow`` package in your own ``CMake`` 
project, please have a look at `FindPyArrow.cmake <https://github.com/wjakob/nanobind/cmake/FindPyArrow.cmake>`__.