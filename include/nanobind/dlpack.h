/*
    nanobind/dlpack.h: functionality to input/output tensors via DLPack

    Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.

    The API below is based on the DLPack project
    (https://github.com/dmlc/dlpack/blob/main/include/dlpack/dlpack.h)
*/

#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(dlpack)

enum class DeviceType : int32_t {
	Undefined = 0, CPU = 1, CUDA = 2, CUDAHost = 3,
	OpenCL = 4, Vulkan = 7, Metal = 8, ROCM = 10,
	ROCMHost = 11, CUDAManaged = 13, OneAPI = 14
};

enum class DataTypeCode : uint8_t {
	Int = 0, UInt = 1, Float = 2, Bfloat = 4, Complex = 5
} ;

struct Device {
	DeviceType device_type = DeviceType::Undefined;
	int32_t device_id = 0;
};

struct DataType {
	uint8_t code = 0;
	uint8_t bits = 0;
	uint16_t lanes = 0;

    bool operator==(const DataType &o) const {
        return code == o.code && bits == o.bits && lanes == o.lanes;
    }
    bool operator!=(const DataType &o) const { return !operator==(o); }
};

struct Tensor {
	void *data = nullptr;
	Device device;
	int32_t ndim = 0;
	DataType dtype;
	int64_t *shape = nullptr;
	int64_t *strides = nullptr;
	uint64_t byte_offset = 0;
};

NAMESPACE_END(dlpack)

constexpr size_t any = (size_t) -1;
template <size_t... Is> class shape { };
template <char O> class order {
    static_assert(O == 'C' || O == 'F', "Only C ('C') and Fortran ('F')-style "
                                        "ordering conventions are supported!");
};

template <typename T> constexpr dlpack::DataType dtype() {
	static_assert(
		std::is_floating_point_v<T> || std::is_integral_v<T>,
		"nanobind::dtype<T>: T must be a floating point or integer variable!"
	);

	dlpack::DataType result;

	if constexpr (std::is_floating_point_v<T>)
		result.code = (uint8_t) dlpack::DataTypeCode::Float;
	else if constexpr (std::is_signed_v<T>)
		result.code = (uint8_t) dlpack::DataTypeCode::Int;
	else
		result.code = (uint8_t) dlpack::DataTypeCode::UInt;

	result.bits = sizeof(T) * 8;
	result.lanes = 1;

	return result;
}


NAMESPACE_BEGIN(detail)

struct TensorReq {
	dlpack::DataType dtype;
	uint32_t ndim = 0;
	size_t *shape = nullptr;
	bool req_shape = false;
	bool req_dtype = false;
	char req_order = '\0';
};

template <typename T, typename = int> struct tensor_arg;

template <typename T> struct tensor_arg<T, enable_if_t<std::is_floating_point_v<T>>> {
	static constexpr size_t size = 0;

    static constexpr auto name =
        const_name("dtype=float") + const_name<sizeof(T) * 8>();

    static void apply(TensorReq &tr) {
        tr.dtype = dtype<T>();
        tr.req_dtype = true;
    }
};

template <typename T> struct tensor_arg<T, enable_if_t<std::is_integral_v<T>>> {
	static constexpr size_t size = 0;

    static constexpr auto name =
        const_name("dtype=") + const_name<std::is_unsigned_v<T>>("u", "") +
        const_name("int") + const_name<sizeof(T) * 8>();

    static void apply(TensorReq &tr) {
        tr.dtype = dtype<T>();
        tr.req_dtype = true;
    }
};

template <size_t... Is> struct tensor_arg<shape<Is...>> {
	static constexpr size_t size = sizeof...(Is);
    static constexpr auto name =
        const_name("shape=(") +
        concat(const_name<Is == any>(const_name("*"), const_name<Is>())...) +
        const_name(")");

	static void apply(TensorReq &tr) {
		size_t i = 0;
		((tr.shape[i++] = Is), ...);
		tr.ndim = (uint32_t) sizeof...(Is);
		tr.req_shape = true;
	}
};

template <char O> struct tensor_arg<order<O>> {
	static constexpr size_t size = 0;
    static constexpr auto name =
        const_name("order='") + const_name(O) + const_name('\'');

    static void apply(TensorReq &tr) {
		tr.req_order = O;
	}
};

NAMESPACE_END(detail)

template <typename... Args> class tensor {
public:
    tensor() = default;

    explicit tensor(detail::TensorHandle *handle) : m_handle(handle) {
        if (handle)
            m_tensor = *detail::tensor_inc_ref(handle);
    }

    ~tensor() {
		detail::tensor_dec_ref(m_handle);
	}

    tensor(const tensor &t) : m_handle(t.m_handle), m_tensor(t.m_tensor) {
        detail::tensor_inc_ref(m_handle);
    }

    tensor(tensor &&t) noexcept : m_handle(t.m_handle), m_tensor(t.m_tensor) {
		t.m_handle = nullptr;
		t.m_tensor = dlpack::Tensor();
    }

    tensor &operator=(tensor &&t) noexcept {
		detail::tensor_dec_ref(m_handle);
        m_handle = t.m_handle;
        m_tensor = t.m_tensor;
		t.m_handle = nullptr;
		t.m_tensor = dlpack::Tensor();
		return *this;
    }

    tensor &operator=(const tensor &t) {
		detail::tensor_inc_ref(t.m_handle);
		detail::tensor_dec_ref(m_handle);
        m_handle = t.m_handle;
        m_tensor = t.m_tensor;
	}

	dlpack::DataType dtype() const { return m_tensor.dtype; }
	size_t ndim() const { return m_tensor.ndim; }
	size_t shape(size_t i) const { return m_tensor.shape[i]; }
	size_t strides(size_t i) const { return m_tensor.strides[i]; }
	bool is_valid() const { return m_handle != nullptr; }

private:
    detail::TensorHandle *m_handle = nullptr;
    dlpack::Tensor m_tensor;
};

NAMESPACE_BEGIN(detail)

template <typename... Args> struct type_caster<tensor<Args...>> {
    NB_TYPE_CASTER(tensor<Args...>, const_name("tensor[") +
                                        concat(detail::tensor_arg<Args>::name...) +
                                        const_name("]"));

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
		constexpr size_t size = (0 + ... + detail::tensor_arg<Args>::size);
		size_t shape[size + 1];
		detail::TensorReq req;
		req.shape = shape;
        (detail::tensor_arg<Args>::apply(req), ...);
        value = tensor<Args...>(tensor_create(src.ptr(), &req));
        return value.is_valid();
    }

    static handle from_cpp(const tensor<Args...> &, rv_policy,
                           cleanup_list *) noexcept {
        return handle();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
