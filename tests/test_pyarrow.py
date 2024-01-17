import test_pyarrow_ext as t
import pyarrow as pa
import numpy as np
import re
import pytest
import decimal
import datetime

@pytest.mark.parametrize("data,func", [(pa.array([1, 2, 3]), t.test_array), 
                                       (pa.nulls(10), t.test_null_array), 
                                       (pa.chunked_array([[2, 2, 4], [4, 5, 100]]), t.test_chunked_array),
                                       (pa.array([True, False, False]), t.test_boolean_array),])
def test_base_arrays(data, func):
    assert func(data).equals(data)


def check_chunked_array(arr):
    chunked_arr = pa.chunked_array(arr)
    assert t.test_chunked_array(chunked_arr).equals(chunked_arr)

@pytest.mark.parametrize("dtype", [np.float16, np.float32, np.float64, 
                                   np.int8, np.int16, np.int32, np.int64,
                                   np.uint8, np.uint16, np.uint32, np.uint64])
def test_numeric_array(dtype):
    np_arr = np.array([1., 2., 3.], dtype=dtype())
    arr = pa.array(np_arr)
    func = getattr(t, f"test_{str(arr.type)}_array")
    assert func(arr).equals(arr)
    check_chunked_array(arr)


@pytest.mark.parametrize("dtype", [pa.string, pa.large_string])
def test_string_array(dtype):
    arr = pa.array(['foo', 'bar'] * 50, type=dtype())
    func = getattr(t, f"test_{str(arr.type)}_array")
    assert func(arr).equals(arr)
    check_chunked_array(arr)

def test_list_array():
    values = pa.array([1, 2, 3, 4])
    offsets = pa.array([0, 2, 4])
    arr = pa.ListArray.from_arrays(offsets, values)
    assert t.test_list_array(arr).equals(arr)

@pytest.mark.parametrize("construct,func", [(pa.RecordBatch, t.test_record_batch), (pa.Table, t.test_table)])
def test_tabular(construct, func):
    n_legs = pa.array([2, 2, 4, 4, 5, 100])
    animals = pa.array(["Flamingo", "Parrot", "Dog", "Horse", "Brittle stars", "Centipede"])
    names = ["n_legs", "animals"]
    data = construct.from_arrays([n_legs, animals], names=names)
    assert func(data).equals(data)

def test_field():
    f = pa.field('test', pa.int32())
    f_copy = t.test_field(f)
    assert f_copy.equals(f)

def test_schema():
    schema = pa.schema([('some_int', pa.int32()),('some_string', pa.string())])
    schema_copy = t.test_schema(schema)
    assert schema_copy.equals(schema)


@pytest.mark.parametrize(['value', 'ty', 'callback'], [
    (False, None, "boolean"),
    (True, None, "boolean"),
    (1, None, "int64"),
    (-1, None, "int64"),
    (1, pa.int8(), "int8"),
    (1, pa.uint8(), "uint8"),
    (1, pa.int16(), "int16"),
    (1, pa.uint16(), "uint16"),
    (1, pa.int32(), "int32"),
    (1, pa.uint32(), "uint32"),
    (1, pa.int64(), "int64"),
    (1, pa.uint64(), "uint64"),
    (1.0, None, "double"),
    (np.float16(1.0), pa.float16(), "halffloat"),
    (1.0, pa.float32(), "float"),
    (decimal.Decimal("1.123"), None, "decimal128"),
    (decimal.Decimal("1.1234567890123456789012345678901234567890"),
     None, "decimal256"),
    ("string", None, "string"),
    (b"bytes", None, "binary"),
    ("largestring", pa.large_string(), "largestring"),
    (b"largebytes", pa.large_binary(), "largebinary"),
    (b"abc", pa.binary(3), "fixedsizebinary"),
    ([1, 2, 3], None, "list"),
    ([1, 2, 3, 4], pa.large_list(pa.int8()), "largelist"),
    ([1, 2, 3, 4, 5], pa.list_(pa.int8(), 5),"fixedsizelist"),
    (datetime.date.today(), None, "date32"),
    (datetime.date.today(), pa.date64(), "date64"),
    (datetime.datetime.now(), None, "timestamp"),
    (datetime.datetime.now().time().replace(microsecond=0), pa.time32('s'),
     "time32"),
    (datetime.datetime.now().time(), None, "time64"),
    (datetime.timedelta(days=1), None, "duration"),
    (pa.MonthDayNano([1, -1, -10100]), None,
     "monthdaynanointerval"),
    ({'a': 1, 'b': [1, 2]}, None, "struct"),
    ([('a', 1), ('b', 2)], pa.map_(pa.string(), pa.int8()), "map"),
])
def test_scalar(value, ty, callback):
    s = pa.scalar(value, type=ty)
    s.validate()
    s.validate(full=True)
    func = getattr(t, f"test_{callback}_scalar")
    assert func(s).equals(s)

@pytest.mark.parametrize("data_type", [
    pa.null(),
    pa.bool_(),
    pa.int8(),
    pa.int16(),
    pa.int32(),
    pa.int64(),
    pa.uint8(),
    pa.uint16(),
    pa.uint32(),
    pa.uint64(),
    pa.float16(),
    pa.float32(),
    pa.float64(),
    pa.time32('s'),
    pa.time64('us'),
    pa.timestamp('us'),
    pa.date32(),
    pa.date64(),
    pa.duration('s'),
    pa.month_day_nano_interval(),
    pa.binary(),
    pa.binary(10),
    pa.string(),
    pa.large_string(),
    pa.large_binary(),
    pa.decimal128(19, 4), 
    pa.decimal256(76, 38),
    pa.list_(pa.int32()),
    pa.list_(pa.int32(), 2),
    pa.large_list(pa.uint16()),
    pa.map_(pa.string(), pa.int32()),
    pa.struct([pa.field('a', pa.int32()),
               pa.field('b', pa.int8()),
               pa.field('c', pa.string())]),
    pa.union([pa.field('a', pa.binary(10)),
              pa.field('b', pa.string())], mode=pa.lib.UnionMode_DENSE),
    pa.union([pa.field('a', pa.binary(10)),
              pa.field('b', pa.string())], mode=pa.lib.UnionMode_SPARSE),
    pa.dictionary(pa.int32(), pa.string()),
    pa.run_end_encoded(pa.int64(), pa.uint8())])
def test_data_types(data_type):
    def resolve_callback():
        callback_name = type(data_type).__name__.lower()
        if callback_name == "datatype":
            callback_name = re.sub(r"\[.*?\]","", str(data_type).split('(')[0])
        else:
            callback_name = callback_name.replace("type", "")
        return getattr(t, f"test_{callback_name}_type")

    func = resolve_callback()
    assert func(data_type).equals(data_type)
    assert t.test_data_type(data_type).equals(data_type)

def test_buffer():
    buffer = pa.allocate_buffer(10)
    assert t.test_buffer(buffer).equals(buffer)
    buffer = pa.allocate_buffer(10, resizable=True)
    assert t.test_mutable_buffer(buffer).equals(buffer)
    assert t.test_resizable_buffer(buffer).equals(buffer)

def test_tensor():
    arr = np.array([[2, 2, 4], [4, 5, 100]], np.int32)
    tensor = pa.Tensor.from_numpy(arr, dim_names=["dim1","dim2"])
    assert t.test_tensor(tensor).equals(tensor)

def test_failure_01():
    arr = pa.array([1., 2., 3.])
    with pytest.raises(TypeError, match=r".*test_int64_array\(arg: pyarrow\.lib\.Int64Array, /\) -> pyarrow\.lib\.Int64Array\n\nInvoked with types: DoubleArray.*"):
        t.test_int64_array(arr)

def test_failure_02():
    schema = pa.schema([('some_int', pa.int32()),('some_string', pa.string())])
    with pytest.raises(TypeError, match=r".*test_double_array\(arg: pyarrow\.lib\.DoubleArray, /\) -> pyarrow\.lib\.DoubleArray\n\nInvoked with types: Schema.*"):
        t.test_double_array(schema)

if __name__ == "__main__":
    test_data_types(pa.date32())
    test_scalar(False, None, pa.BooleanScalar)
    test_scalar(decimal.Decimal("1.123"), None, "decimal128")
    test_numeric_array(np.float64)
    test_tabular(pa.Table, t.test_table)