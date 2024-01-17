#include <memory>
#include <nanobind/nanobind.h>

#include <nanobind/pyarrow/pyarrow_import.h>
#include <nanobind/pyarrow/array_primitive.h>
#include <nanobind/pyarrow/array_binary.h>
#include <nanobind/pyarrow/chunked_array.h>
#include <nanobind/pyarrow/tabular.h>
#include <nanobind/pyarrow/scalar.h>
#include <nanobind/pyarrow/datatype.h>
#include <nanobind/pyarrow/buffer.h>


namespace nb = nanobind;

using namespace nb::literals;

template <typename T> auto func() {
  return [](std::shared_ptr<T> arr) {
    return arr;
  };
}
template <typename T> auto funcCopy() {
  return [](std::shared_ptr<T> arr) {
    auto data = arr->data()->Copy();
    return std::make_shared<T>(std::move(data)); };
}

NB_MODULE(test_pyarrow_ext, m) {
    static nb::detail::pyarrow::ImportPyarrow module;
    // BaseTypes
    m.def("test_array", func<arrow::Array>());
    m.def("test_null_array", func<arrow::NullArray>());
    m.def("test_boolean_array", func<arrow::BooleanArray>());
    m.def("test_list_array", func<arrow::ListArray>());
    m.def("test_chunked_array", func<arrow::ChunkedArray>());

    // Buffer
    m.def("test_buffer", [](std::shared_ptr<arrow::Buffer> buffer){return arrow::Buffer::FromString(buffer->ToString());});
    m.def("test_mutable_buffer", [](std::shared_ptr<arrow::MutableBuffer> buffer){return arrow::MutableBuffer::FromString(buffer->ToString());});
    m.def("test_resizable_buffer", [](std::shared_ptr<arrow::ResizableBuffer> buffer){return arrow::ResizableBuffer::FromString(buffer->ToString());});

    // NumericArrays
    m.def("test_halffloat_array", funcCopy<arrow::HalfFloatArray>());
    m.def("test_float_array", funcCopy<arrow::FloatArray>());
    m.def("test_double_array", funcCopy<arrow::DoubleArray>());

    m.def("test_int8_array", funcCopy<arrow::Int8Array>());
    m.def("test_int16_array", funcCopy<arrow::Int16Array>());
    m.def("test_int32_array", funcCopy<arrow::Int32Array>());
    m.def("test_int64_array", funcCopy<arrow::Int64Array>());
    
    m.def("test_uint8_array", funcCopy<arrow::UInt8Array>());
    m.def("test_uint16_array", funcCopy<arrow::UInt16Array>());
    m.def("test_uint32_array", funcCopy<arrow::UInt32Array>());
    m.def("test_uint64_array", funcCopy<arrow::UInt64Array>());

    // StringArrays
    m.def("test_string_array", funcCopy<arrow::StringArray>());
    m.def("test_large_string_array", funcCopy<arrow::LargeStringArray>());

    // Two Dimensional Data
    m.def("test_record_batch", [](std::shared_ptr<arrow::RecordBatch> recordBatch) {return arrow::RecordBatch::Make(recordBatch->schema(), recordBatch->num_rows(), recordBatch->columns());});
    m.def("test_table", [](std::shared_ptr<arrow::Table> table) {return arrow::Table::Make(table->schema(), table->columns(), table->num_rows());});

    m.def("test_field", [](std::shared_ptr<arrow::Field> field){return field->WithName(field->name());});
    m.def("test_schema", [](std::shared_ptr<arrow::Schema> schema){return arrow::schema(schema->fields(), schema->endianness(), schema->metadata());});

    // Scalars
    m.def("test_decimal128_scalar", func<arrow::Decimal128Scalar>());
    m.def("test_decimal256_scalar", func<arrow::Decimal256Scalar>());

    // DataTypes
    m.def("test_data_type", func<arrow::DataType>());
    m.def("test_null_type", func<arrow::NullType>());
    m.def("test_bool_type", func<arrow::BooleanType>());
    m.def("test_int8_type", func<arrow::Int8Type>());
    m.def("test_int16_type", func<arrow::Int16Type>());
    m.def("test_int32_type", func<arrow::Int32Type>());
    m.def("test_int64_type", func<arrow::Int64Type>());
    m.def("test_uint8_type", func<arrow::UInt8Type>());
    m.def("test_uint16_type", func<arrow::UInt16Type>());
    m.def("test_uint32_type", func<arrow::UInt32Type>());
    m.def("test_uint64_type", func<arrow::UInt64Type>());
    m.def("test_time32_type", func<arrow::Time32Type>());
    m.def("test_time64_type", func<arrow::Time64Type>());
    m.def("test_date32_type", func<arrow::Date32Type>());
    m.def("test_date64_type", func<arrow::Date64Type>());
    m.def("test_timestamp_type", func<arrow::TimestampType>());
    m.def("test_duration_type", func<arrow::DurationType>());
    m.def("test_month_day_nano_interval_type", func<arrow::MonthDayNanoIntervalType>());
    m.def("test_halffloat_type", func<arrow::HalfFloatType>());
    m.def("test_float_type", func<arrow::FloatType>());
    m.def("test_double_type", func<arrow::DoubleType>());
    m.def("test_decimal128_type", func<arrow::Decimal128Type>());
    m.def("test_decimal256_type", func<arrow::Decimal256Type>());
    m.def("test_string_type", func<arrow::StringType>());
    m.def("test_binary_type", func<arrow::BinaryType>());
    m.def("test_fixedsizebinary_type", func<arrow::FixedSizeBinaryType>());
    m.def("test_large_string_type", func<arrow::LargeStringType>());
    m.def("test_large_binary_type", func<arrow::LargeBinaryType>());
    m.def("test_list_type", func<arrow::ListType>());
    m.def("test_fixedsizelist_type", func<arrow::FixedSizeListType>());
    m.def("test_largelist_type", func<arrow::LargeListType>());
    m.def("test_map_type", func<arrow::MapType>());
    m.def("test_struct_type", func<arrow::StructType>());
    m.def("test_denseunion_type", func<arrow::DenseUnionType>());
    m.def("test_sparseunion_type", func<arrow::SparseUnionType>());
    m.def("test_dictionary_type", func<arrow::DictionaryType>());
    m.def("test_runendencoded_type", func<arrow::RunEndEncodedType>());
}