#include <nanobind/ndarray.h>
#include <atomic>
#include <memory>
#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)

NAMESPACE_BEGIN(dlpack)

/// Indicates the managed_dltensor_versioned is read only.
static constexpr uint64_t flag_bitmask_read_only = 1UL << 0;

struct version {
    uint32_t major;
    uint32_t minor;
};

NAMESPACE_END(dlpack)

// ========================================================================

NAMESPACE_BEGIN(detail)

// DLPack version 0, deprecated Feb 2024, obsoleted March 2025
struct managed_dltensor {
    dlpack::dltensor dltensor;
    void *manager_ctx;
    void (*deleter)(managed_dltensor *);
};

// DLPack version 1, pre-release Feb 2024, release Sep 2024
struct managed_dltensor_versioned {
    dlpack::version version;
    void *manager_ctx;
    void (*deleter)(managed_dltensor_versioned *);
    uint64_t flags = 0UL;
    dlpack::dltensor dltensor;
};

static void mt_from_buffer_delete(managed_dltensor_versioned* self) {
    gil_scoped_acquire guard;
    Py_buffer *buf = (Py_buffer *) self->manager_ctx;
    PyBuffer_Release(buf);
    PyMem_Free(buf);
    PyMem_Free(self);  // This also frees shape and size arrays.
}

// Forward declaration
struct ndarray_handle;

template<typename MT>
static void mt_from_handle_delete(MT* self) {
    gil_scoped_acquire guard;
    ndarray_handle* th = (ndarray_handle *) self->manager_ctx;
    PyMem_Free(self);
    ndarray_dec_ref(th);
}

template<bool versioned>
static void capsule_delete(PyObject *capsule) {
    const char* capsule_name;
    if constexpr (versioned)
        capsule_name = "dltensor_versioned";
    else
        capsule_name = "dltensor";

    using MT = std::conditional_t<versioned, managed_dltensor_versioned,
                                             managed_dltensor>;
    error_scope scope; // temporarily save any existing errors
    MT* mt = (MT*) PyCapsule_GetPointer(capsule, capsule_name);
    if (mt)
        mt->deleter(mt);
    else
        PyErr_Clear();
}

// Reference-counted wrapper for versioned or unversioned managed tensors
struct ndarray_handle {
    union {
        managed_dltensor           *mt_unversioned;
        managed_dltensor_versioned *mt_versioned;
    };
    std::atomic<size_t> refcount;
    PyObject *owner, *self;
    bool versioned;     // This tags which union member is active.
    bool free_strides;  // True if we added strides to an imported tensor.
    bool call_deleter;  // True if tensor was imported, else PyMem_Free(mt).
    bool ro;            // Whether tensor is read-only.

    PyObject* make_capsule_unversioned() {
        PyObject* capsule;
        if (!versioned && mt_unversioned->manager_ctx == this) {
            capsule = PyCapsule_New(mt_unversioned, "dltensor",
                                    capsule_delete</*versioned=*/false>);
        } else {
            scoped_pymalloc<managed_dltensor> mt;
            memcpy(&mt->dltensor,
                   (versioned) ? &mt_versioned->dltensor
                               : &mt_unversioned->dltensor,
                   sizeof(dlpack::dltensor));
            mt->manager_ctx = this;
            mt->deleter = mt_from_handle_delete<managed_dltensor>;
            capsule = PyCapsule_New(mt.release(), "dltensor",
                                    capsule_delete</*versioned=*/false>);
        }
        check(capsule, "Could not make unversioned capsule");
        refcount++;
        return capsule;
    }

    PyObject* make_capsule_versioned() {
        PyObject* capsule;
        if (versioned && mt_versioned->manager_ctx == this) {
            capsule = PyCapsule_New(mt_versioned, "dltensor_versioned",
                                    capsule_delete</*versioned=*/true>);
        } else {
            scoped_pymalloc<managed_dltensor_versioned> mt;
            mt->version = {dlpack::major_version, dlpack::minor_version};
            mt->manager_ctx = this;
            mt->deleter = mt_from_handle_delete<managed_dltensor_versioned>;
            mt->flags = (ro) ? dlpack::flag_bitmask_read_only : 0;
            memcpy(&mt->dltensor,
                   (versioned) ? &mt_versioned->dltensor
                               : &mt_unversioned->dltensor,
                   sizeof(dlpack::dltensor));
            capsule = PyCapsule_New(mt.release(), "dltensor_versioned",
                                    capsule_delete</*versioned=*/true>);
        }
        check(capsule, "Could not make versioned capsule");
        refcount++;
        return capsule;
    }
};

// ========================================================================

static void nb_ndarray_dealloc(PyObject *self) {
    PyTypeObject *tp = Py_TYPE(self);
    ndarray_dec_ref(((nb_ndarray *) self)->th);
    PyObject_Free(self);
    Py_DECREF(tp);
}

static int nb_ndarray_getbuffer(PyObject *self, Py_buffer *view, int) {
    ndarray_handle *th = ((nb_ndarray *) self)->th;
    dlpack::dltensor &t = (th->versioned) ? th->mt_versioned->dltensor
                                          : th->mt_unversioned->dltensor;

    if (t.device.device_type != device::cpu::value) {
        PyErr_SetString(PyExc_BufferError, "Only CPU-allocated ndarrays can be "
                                           "accessed via the buffer protocol!");
        return -1;
    }

    const char *format = nullptr;
    switch ((dlpack::dtype_code) t.dtype.code) {
        case dlpack::dtype_code::Int:
            switch (t.dtype.bits) {
                case 8: format = "b"; break;
                case 16: format = "h"; break;
                case 32: format = "i"; break;
                case 64: format = "q"; break;
            }
            break;

        case dlpack::dtype_code::UInt:
            switch (t.dtype.bits) {
                case 8: format = "B"; break;
                case 16: format = "H"; break;
                case 32: format = "I"; break;
                case 64: format = "Q"; break;
            }
            break;

        case dlpack::dtype_code::Float:
            switch (t.dtype.bits) {
                case 16: format = "e"; break;
                case 32: format = "f"; break;
                case 64: format = "d"; break;
            }
            break;

        case dlpack::dtype_code::Complex:
            switch (t.dtype.bits) {
                case 64: format = "Zf"; break;
                case 128: format = "Zd"; break;
            }
            break;

        case dlpack::dtype_code::Bool:
            format = "?";
            break;

        default:
            break;
    }

    if (!format || t.dtype.lanes != 1) {
        PyErr_SetString(PyExc_BufferError,
            "Cannot convert DLPack dtype into buffer protocol format!");
        return -1;
    }

    view->buf = (void *) ((uintptr_t) t.data + t.byte_offset);
    view->obj = self;
    Py_INCREF(self);

    scoped_pymalloc<Py_ssize_t> shape_and_strides(2 * (size_t) t.ndim);
    Py_ssize_t* shape = shape_and_strides.get();
    Py_ssize_t* strides = shape + t.ndim;

    const Py_ssize_t itemsize = t.dtype.bits / 8;
    Py_ssize_t len = itemsize;
    for (size_t i = 0; i < (size_t) t.ndim; ++i) {
        len *= (Py_ssize_t) t.shape[i];
        shape[i] = (Py_ssize_t) t.shape[i];
        strides[i] = (Py_ssize_t) t.strides[i] * itemsize;
    }

    view->len = len;
    view->itemsize = itemsize;
    view->readonly = th->ro;
    view->ndim = t.ndim;
    view->format = (char *) format;
    view->shape = shape;
    view->strides = strides;
    view->suboffsets = nullptr;
    view->internal = shape_and_strides.release();

    return 0;
}

static void nb_ndarray_releasebuffer(PyObject *, Py_buffer *view) {
    PyMem_Free(view->internal);
}

// This function implements __dlpack__() for a nanobind.nb_ndarray.
static PyObject *nb_ndarray_dlpack(PyObject *self, PyObject *const *args,
                                   Py_ssize_t nargsf, PyObject *kwnames) {
    if (PyVectorcall_NARGS(nargsf) != 0) {
        PyErr_SetString(PyExc_TypeError,
                "__dlpack__() does not accept positional arguments");
        return nullptr;
    }
    Py_ssize_t nkwargs = (kwnames) ? NB_TUPLE_GET_SIZE(kwnames) : 0;

    long max_major_version = 0;
    for (Py_ssize_t i = 0; i < nkwargs; ++i) {
        PyObject* key = NB_TUPLE_GET_ITEM(kwnames, i);
        if (key == static_pyobjects[pyobj_name::dl_device_str] ||
            key == static_pyobjects[pyobj_name::copy_str])
            // These keyword arguments are ignored.  This branch of the code
            // is here to avoid a Python call to RichCompare if these kwargs
            // are provided by the caller.
            continue;
        if (key == static_pyobjects[pyobj_name::max_version_str] ||
            PyObject_RichCompareBool(key,
                static_pyobjects[pyobj_name::max_version_str], Py_EQ) == 1) {
            PyObject* value = args[i];
            if (value == Py_None)
                break;
            if (!PyTuple_Check(value) || NB_TUPLE_GET_SIZE(value) != 2) {
                PyErr_SetString(PyExc_TypeError,
                        "max_version must be None or tuple[int, int]");
                return nullptr;
            }
            max_major_version = PyLong_AsLong(NB_TUPLE_GET_ITEM(value, 0));
            break;
        }
    }

    ndarray_handle *th = ((nb_ndarray *) self)->th;
    PyObject *capsule;
    if (max_major_version >= dlpack::major_version)
        capsule = th->make_capsule_versioned();
    else
        capsule = th->make_capsule_unversioned();

    return capsule;
}

// This function implements __dlpack_device__() for a nanobind.nb_ndarray.
static PyObject *nb_ndarray_dlpack_device(PyObject *self, PyObject *) {
    ndarray_handle *th = ((nb_ndarray *) self)->th;
    dlpack::dltensor& t = (th->versioned)
                              ? th->mt_versioned->dltensor
                              : th->mt_unversioned->dltensor;
    PyObject *r;
    if (t.device.device_type == 1 && t.device.device_id == 0) {
        r = static_pyobjects[pyobj_name::dl_cpu_tpl];
        Py_INCREF(r);
    } else {
        r = PyTuple_New(2);
        PyObject *r0 = PyLong_FromLong(t.device.device_type);
        PyObject *r1 = PyLong_FromLong(t.device.device_id);
        if (!r || !r0 || !r1) {
            Py_XDECREF(r);
            Py_XDECREF(r0);
            Py_XDECREF(r1);
            return nullptr;
        }
        NB_TUPLE_SET_ITEM(r, 0, r0);
        NB_TUPLE_SET_ITEM(r, 1, r1);
    }
    return r;
}

static PyMethodDef nb_ndarray_methods[] = {
   { "__dlpack__", (PyCFunction) (void *) nb_ndarray_dlpack,
                   METH_FASTCALL | METH_KEYWORDS, nullptr },
   { "__dlpack_device__", nb_ndarray_dlpack_device, METH_NOARGS, nullptr },
   { nullptr, nullptr, 0, nullptr }
};

static PyTypeObject *nb_ndarray_tp() noexcept {
    nb_internals *internals_ = internals;
    PyTypeObject *tp = internals_->nb_ndarray.load_acquire();

    if (NB_UNLIKELY(!tp)) {
        lock_internals guard(internals_);
        tp = internals_->nb_ndarray.load_relaxed();
        if (tp)
            return tp;

        PyType_Slot slots[] = {
            { Py_tp_dealloc, (void *) nb_ndarray_dealloc },
            { Py_tp_methods, (void *) nb_ndarray_methods },
            { Py_bf_getbuffer, (void *) nb_ndarray_getbuffer },
            { Py_bf_releasebuffer, (void *) nb_ndarray_releasebuffer },
            { 0, nullptr }
        };

        PyType_Spec spec = {
            /* .name = */ "nanobind.nb_ndarray",
            /* .basicsize = */ (int) sizeof(nb_ndarray),
            /* .itemsize = */ 0,
            /* .flags = */ Py_TPFLAGS_DEFAULT,
            /* .slots = */ slots
        };

        tp = (PyTypeObject *) PyType_FromSpec(&spec);
        check(tp, "nb_ndarray type creation failed!");

        internals_->nb_ndarray.store_release(tp);
    }

    return tp;
}

// ========================================================================

using mt_unique_ptr_t = std::unique_ptr<managed_dltensor_versioned,
                                        decltype(&mt_from_buffer_delete)>;

static mt_unique_ptr_t make_mt_from_buffer_protocol(PyObject *o, bool ro) {
    mt_unique_ptr_t mt_unique_ptr(nullptr, &mt_from_buffer_delete);
    scoped_pymalloc<Py_buffer> view;
    if (PyObject_GetBuffer(o, view.get(),
                           ro ? PyBUF_RECORDS_RO : PyBUF_RECORDS)) {
        PyErr_Clear();
        return mt_unique_ptr;
    }

    char format_c = 'B';
    const char *format_str = view->format;
    if (format_str)
        format_c = *format_str;

    bool skip_first = format_c == '@' || format_c == '=';

    int32_t num = 1;
    if (*(uint8_t *) &num == 1) {
        if (format_c == '<')
            skip_first = true;
    } else {
        if (format_c == '!' || format_c == '>')
            skip_first = true;
    }

    if (skip_first && format_str)
        format_c = *++format_str;

    bool is_complex = format_str[0] == 'Z';
    if (is_complex)
        format_c = *++format_str;

    dlpack::dtype dt { };
    bool fail = format_str && format_str[1] != '\0';

    if (!fail) {
        switch (format_c) {
            case 'c':
            case 'b':
            case 'h':
            case 'i':
            case 'l':
            case 'q':
            case 'n': dt.code = (uint8_t) dlpack::dtype_code::Int; break;

            case 'B':
            case 'H':
            case 'I':
            case 'L':
            case 'Q':
            case 'N': dt.code = (uint8_t) dlpack::dtype_code::UInt; break;

            case 'e':
            case 'f':
            case 'd': dt.code = (uint8_t) dlpack::dtype_code::Float; break;

            case '?': dt.code = (uint8_t) dlpack::dtype_code::Bool; break;

            default: fail = true;
        }

        if (is_complex) {
            fail |= dt.code != (uint8_t) dlpack::dtype_code::Float;
            dt.code = (uint8_t) dlpack::dtype_code::Complex;
        }

        dt.lanes = 1;
        dt.bits = (uint8_t) (view->itemsize * 8);
    }

    if (fail) {
        PyBuffer_Release(view.get());
        return mt_unique_ptr;
    }

    int32_t ndim = view->ndim;

    static_assert(alignof(managed_dltensor_versioned) >= alignof(int64_t));
    scoped_pymalloc<managed_dltensor_versioned> mt(1, 2 * sizeof(int64_t)*ndim);
    int64_t* shape = nullptr;
    int64_t* strides = nullptr;
    if (ndim > 0) {
        shape = new ((void*) (mt.get() + 1)) int64_t[2 * ndim];
        strides = shape + ndim;
    }

    /* See comments in function ndarray_create(). */
#if 0
    uintptr_t data_uint = (uintptr_t) view->buf;
    void* data_ptr = (void *) (data_uint & ~uintptr_t{255});
    uint64_t data_offset = data_uint & uintptr_t{255};
#else
    void* data_ptr = view->buf;
    constexpr uint64_t data_offset = 0UL;
#endif

    mt->dltensor.data = data_ptr;
    mt->dltensor.device = { device::cpu::value, 0 };
    mt->dltensor.ndim = ndim;
    mt->dltensor.dtype = dt;
    mt->dltensor.shape = shape;
    mt->dltensor.strides = strides;
    mt->dltensor.byte_offset = data_offset;

    const int64_t itemsize = (int64_t) view->itemsize;
    for (int32_t i = 0; i < ndim; ++i) {
        int64_t stride = view->strides[i] / itemsize;
        if (stride * itemsize != view->strides[i]) {
            PyBuffer_Release(view.get());
            return mt_unique_ptr;
        }
        strides[i] = stride;
        shape[i] = (int64_t) view->shape[i];
    }

    mt->version = {dlpack::major_version, dlpack::minor_version};
    mt->manager_ctx = view.release();
    mt->deleter = mt_from_buffer_delete;
    mt->flags = (ro) ? dlpack::flag_bitmask_read_only : 0;

    mt_unique_ptr.reset(mt.release());
    return mt_unique_ptr;
}

bool ndarray_check(PyObject *o) noexcept {
    if (PyObject_HasAttr(o, static_pyobjects[pyobj_name::dunder_dlpack_str]) ||
        PyObject_CheckBuffer(o))
        return true;

    PyTypeObject *tp = Py_TYPE(o);
    if (tp == &PyCapsule_Type)
        return true;

    PyObject *name = nb_type_name((PyObject *) tp);
    check(name, "Could not obtain type name! (1)");

    const char *tp_name = PyUnicode_AsUTF8AndSize(name, nullptr);
    check(tp_name, "Could not obtain type name! (2)");

    bool result =
        // PyTorch
        strcmp(tp_name, "torch.Tensor") == 0 ||
        // XLA
        strcmp(tp_name, "jaxlib.xla_extension.ArrayImpl") == 0 ||
        // Tensorflow
        strcmp(tp_name, "tensorflow.python.framework.ops.EagerTensor") == 0 ||
        // Cupy
        strcmp(tp_name, "cupy.ndarray") == 0;

    Py_DECREF(name);
    return result;
}


ndarray_handle *ndarray_import(PyObject *src, const ndarray_config *c,
                               bool convert, cleanup_list *cleanup) noexcept {
    object capsule;
    const bool src_is_pycapsule = PyCapsule_CheckExact(src);
    mt_unique_ptr_t mt_unique_ptr(nullptr, &mt_from_buffer_delete);

    if (src_is_pycapsule) {
        capsule = borrow(src);
    } else {
        // Try calling src.__dlpack__()
        PyObject* args[] = {src, static_pyobjects[pyobj_name::dl_version_tpl]};
        Py_ssize_t nargsf = 1 | PY_VECTORCALL_ARGUMENTS_OFFSET;
        capsule = steal(PyObject_VectorcallMethod(
                          static_pyobjects[pyobj_name::dunder_dlpack_str],
                          args, nargsf,
                          static_pyobjects[pyobj_name::max_version_tpl]));

        // Python array API standard v2023 introduced max_version.
        // Try calling src.__dlpack__() without any kwargs.
        if (!capsule.is_valid() && PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Clear();
            capsule = steal(PyObject_VectorcallMethod(
                              static_pyobjects[pyobj_name::dunder_dlpack_str],
                              args, nargsf, nullptr));
        }

        // Try creating an ndarray via the buffer protocol
        if (!capsule.is_valid()) {
            PyErr_Clear();
            mt_unique_ptr = make_mt_from_buffer_protocol(src, c->ro);
        }

        // Try the function to_dlpack(), already obsolete in array API v2021
        if (!mt_unique_ptr && !capsule.is_valid()) {
            PyTypeObject *tp = Py_TYPE(src);
            try {
                const char *module_name =
                    borrow<str>(handle(tp).attr("__module__")).c_str();

                object package;
                if (strncmp(module_name, "tensorflow.", 11) == 0)
                    package = module_::import_("tensorflow.experimental.dlpack");
                else if (strncmp(module_name, "torch", 5) == 0)
                    package = module_::import_("torch.utils.dlpack");
                else if (strncmp(module_name, "jaxlib", 6) == 0)
                    package = module_::import_("jax.dlpack");

                if (package.is_valid())
                    capsule = package.attr("to_dlpack")(handle(src));
            } catch (...) {
                capsule.reset();
            }
            if (!capsule.is_valid())
                return nullptr;
        }
    }

    void* mt;  // can be versioned or unversioned
    bool versioned = true;
    if (mt_unique_ptr) {
        mt = mt_unique_ptr.get();
    } else {
        // Extract the managed_dltensor{_versioned} pointer from the capsule.
        mt = PyCapsule_GetPointer(capsule.ptr(), "dltensor_versioned");
        if (!mt) {
            PyErr_Clear();
            versioned = false;
            mt = PyCapsule_GetPointer(capsule.ptr(), "dltensor");
            if (!mt) {
                PyErr_Clear();
                return nullptr;
            }
        }
    }

    dlpack::dltensor& t = (versioned)
                              ? ((managed_dltensor_versioned *) mt)->dltensor
                              : ((managed_dltensor *) mt)->dltensor;

    uint64_t flags = (versioned) ? ((managed_dltensor_versioned *) mt)->flags
                                 : 0UL;

    // Reject a read-only ndarray if a writable one is required, and
    // reject an ndarray not on the required device.
    if ((!c->ro && (flags & dlpack::flag_bitmask_read_only))
        || (c->device_type != 0 && t.device.device_type != c->device_type)) {
        return nullptr;
    }

    // Check if the ndarray satisfies the remaining requirements.
    bool has_dtype = c->dtype != dlpack::dtype(),
         has_shape = c->ndim != -1,
         has_order = c->order != '\0';

    bool pass_dtype = true, pass_shape = true, pass_order = true;

    if (has_dtype)
        pass_dtype = t.dtype == c->dtype;

    if (has_shape) {
        pass_shape = t.ndim == c->ndim;
        if (pass_shape) {
            for (int32_t i = 0; i < c->ndim; ++i) {
                if (c->shape[i] != -1 && t.shape[i] != c->shape[i]) {
                    pass_shape = false;
                    break;
                }
            }
        }
    }

    int64_t size = 1;
    for (int32_t i = 0; i < t.ndim; ++i)
        size *= t.shape[i];

    // Tolerate any strides if the array has 1 or fewer elements
    if (pass_shape && has_order && size > 1) {
        char order = c->order;

        bool c_order = order == 'C' || order == 'A',
             f_order = order == 'F' || order == 'A';

        if (!t.strides) {
            /* When the provided tensor does not have a valid
               strides field, it uses the C ordering convention */
            if (c_order) {
                pass_order = true;
            } else {
                int nontrivial_dims = 0;
                for (int i = 0; i < t.ndim; ++i)
                    nontrivial_dims += (int) (t.shape[i] > 1);
                pass_order = nontrivial_dims <= 1;
            }
        } else {
            if (c_order) {
                for (int64_t i = t.ndim - 1, accum = 1; i >= 0; --i) {
                    c_order &= t.shape[i] == 1 || t.strides[i] == accum;
                    accum *= t.shape[i];
                }
            }

            if (f_order) {
                for (int64_t i = 0, accum = 1; i < t.ndim; ++i) {
                    f_order &= t.shape[i] == 1 || t.strides[i] == accum;
                    accum *= t.shape[i];
                }
            }

            pass_order = c_order || f_order;
        }
    }

    // Do not convert shape and do not convert complex numbers to non-complex.
    convert &= pass_shape &
               !(t.dtype.code == (uint8_t) dlpack::dtype_code::Complex
                 && has_dtype
                 && c->dtype.code != (uint8_t) dlpack::dtype_code::Complex);

    // Support implicit conversion of dtype and order.
    if (convert && (!pass_dtype || !pass_order) && !src_is_pycapsule) {
        PyTypeObject *tp = Py_TYPE(src);
        str module_name_o = borrow<str>(handle(tp).attr("__module__"));
        const char *module_name = module_name_o.c_str();

        char order = 'K'; // for NumPy. 'K' means 'keep'
        if (c->order)
            order = c->order;

        dlpack::dtype dt = has_dtype ? c->dtype : t.dtype;
        if (dt.lanes != 1)
            return nullptr;

        char dtype[11];
        if (dt.code == (uint8_t) dlpack::dtype_code::Bool) {
            std::strcpy(dtype, "bool");
        } else {
            const char *prefix = nullptr;
            switch (dt.code) {
                case (uint8_t) dlpack::dtype_code::Int:
                    prefix = "int";
                    break;
                case (uint8_t) dlpack::dtype_code::UInt:
                    prefix = "uint";
                    break;
                case (uint8_t) dlpack::dtype_code::Float:
                    prefix = "float";
                    break;
                case (uint8_t) dlpack::dtype_code::Bfloat:
                    prefix = "bfloat";
                    break;
                case (uint8_t) dlpack::dtype_code::Complex:
                    prefix = "complex";
                    break;
                default:
                    return nullptr;
            }
            snprintf(dtype, sizeof(dtype), "%s%u", prefix, dt.bits);
        }

        object converted;
        try {
            if (strncmp(module_name, "numpy", 5) == 0
                || strncmp(module_name, "cupy", 4) == 0) {
                converted = handle(src).attr("astype")(dtype, order);
            } else if (strncmp(module_name, "torch", 5) == 0) {
                module_ torch = module_::import_("torch");
                converted = handle(src).attr("to")(torch.attr(dtype));
                if (c->order == 'C')
                    converted = converted.attr("contiguous")();
            } else if (strncmp(module_name, "tensorflow.", 11) == 0) {
                module_ tensorflow = module_::import_("tensorflow");
                converted = tensorflow.attr("cast")(handle(src), dtype);
            } else if (strncmp(module_name, "jaxlib", 6) == 0) {
                converted = handle(src).attr("astype")(dtype);
            }
        } catch (...) { converted.reset(); }

        // Potentially try once again, recursively
        if (converted.is_valid()) {
            ndarray_handle *h =
                ndarray_import(converted.ptr(), c, false, nullptr);
            if (h && cleanup)
                cleanup->append(converted.release().ptr());
            return h;
        }
    }

    if (!pass_dtype || !pass_shape || !pass_order)
        return nullptr;

    // Create a reference-counted wrapper
    scoped_pymalloc<ndarray_handle> result;
    if (versioned)
        result->mt_versioned = (managed_dltensor_versioned *) mt;
    else
        result->mt_unversioned = (managed_dltensor *) mt;

    result->refcount = 0;
    result->owner = nullptr;
    result->versioned = versioned;
    result->call_deleter = true;
    result->ro = c->ro;

    if (src_is_pycapsule) {
        result->self = nullptr;
    } else {
        result->self = src;
        Py_INCREF(src);
    }

    // If ndim > 0, ensure that the strides member is initialized.
    if (t.strides || t.ndim == 0) {
        result->free_strides = false;
    } else {
        result->free_strides = true;

        scoped_pymalloc<int64_t> strides((size_t) t.ndim);
        for (int64_t i = t.ndim - 1, accum = 1; i >= 0; --i) {
            strides[i] = accum;
            accum *= t.shape[i];
        }
        t.strides = strides.release();
    }

    if (capsule.is_valid()) {
        // Mark the dltensor capsule as used, i.e., "consumed".
        const char* used_name = (versioned) ? "used_dltensor_versioned"
                                            : "used_dltensor";
        if (PyCapsule_SetName(capsule.ptr(), used_name) ||
            PyCapsule_SetDestructor(capsule.ptr(), nullptr))
            check(false, "ndarray_import(): could not mark capsule as used");
    }

    mt_unique_ptr.release();
    return result.release();
}

dlpack::dltensor *ndarray_inc_ref(ndarray_handle *th) noexcept {
    if (!th)
        return nullptr;
    ++th->refcount;
    return (th->versioned) ? &th->mt_versioned->dltensor
                           : &th->mt_unversioned->dltensor;
}

void ndarray_dec_ref(ndarray_handle *th) noexcept {
    if (!th)
        return;
    size_t rc_value = th->refcount--;

    if (rc_value == 0) {
        check(false, "ndarray_dec_ref(): reference count became negative!");
    } else if (rc_value == 1) {
        gil_scoped_acquire guard;

        Py_XDECREF(th->owner);
        Py_XDECREF(th->self);
        if (th->versioned) {
            managed_dltensor_versioned *mt = th->mt_versioned;
            if (th->free_strides) {
                PyMem_Free(mt->dltensor.strides);
                mt->dltensor.strides = nullptr;
            }
            if (th->call_deleter) {
                if (mt->deleter)
                    mt->deleter(mt);
            } else {
                PyMem_Free(mt);  // This also frees shape and size arrays.
            }
        } else {
            managed_dltensor *mt = th->mt_unversioned;
            if (th->free_strides) {
                PyMem_Free(mt->dltensor.strides);
                mt->dltensor.strides = nullptr;
            }
            assert(th->call_deleter);
            if (mt->deleter)
                mt->deleter(mt);
        }
        PyMem_Free(th);
    }
}

ndarray_handle *ndarray_create(void *data, size_t ndim, const size_t *shape_in,
                               PyObject *owner, const int64_t *strides_in,
                               dlpack::dtype dtype, bool ro, int device_type,
                               int device_id, char order) {
    /* DLPack mandates 256-byte alignment of the 'DLTensor::data' field,
       but this requirement is generally ignored.  Also, PyTorch has/had
       a bug in ignoring byte_offset and assuming it's zero.
       It would be wrong to split the 64-bit raw pointer into two pieces,
       as disabled below, since the pointer dltensor.data must point to
       allocated memory (i.e., memory that can be accessed).
       A byte_offset can be used to support array slicing when data is an
       opaque device pointer or handle, on which arithmetic is impossible.
       However, this function is not slicing the data.
       See also: https://github.com/data-apis/array-api/discussions/779  */
#if 0
    uintptr_t data_uint = (uintptr_t) data;
    data = (void *) (data_uint & ~uintptr_t{255});      // upper bits
    uint64_t data_offset = data_uint & uintptr_t{255};  // lowest 8 bits
#else
    constexpr uint64_t data_offset = 0UL;
#endif
    if (device_type == 0)
        device_type = device::cpu::value;

    static_assert(alignof(managed_dltensor_versioned) >= alignof(int64_t));
    scoped_pymalloc<managed_dltensor_versioned> mt(1, 2 * sizeof(int64_t)*ndim);
    int64_t* shape = nullptr;
    int64_t* strides = nullptr;
    if (ndim > 0) {
        shape = new ((void*) (mt.get() + 1)) int64_t[2 * ndim];
        strides = shape + ndim;
    }

    for (size_t i = 0; i < ndim; ++i)
        shape[i] = (int64_t) shape_in[i];

    if (ndim > 0) {
        int64_t prod = 1;
        if (strides_in) {
            for (size_t i = 0; i < ndim; ++i)
                strides[i] = strides_in[i];
        } else if (order == 'F') {
            for (size_t i = 0; i < ndim; ++i) {
                strides[i] = prod;
                prod *= (int64_t) shape_in[i];
            }
        } else if (order == '\0' || order == 'A' || order == 'C') {
            for (ssize_t i = (ssize_t) ndim - 1; i >= 0; --i) {
                strides[i] = prod;
                prod *= (int64_t) shape_in[i];
            }
        } else {
            check(false, "ndarray_create(): unknown memory order requested!");
        }
    }

    scoped_pymalloc<ndarray_handle> result;

    mt->version = {dlpack::major_version, dlpack::minor_version};
    mt->manager_ctx = result.get();
    mt->deleter = [](managed_dltensor_versioned *self) {
                      ndarray_dec_ref((ndarray_handle *) self->manager_ctx);
                  };
    mt->flags = (ro) ? dlpack::flag_bitmask_read_only : 0;
    mt->dltensor.data = data;
    mt->dltensor.device.device_type = (int32_t) device_type;
    mt->dltensor.device.device_id = (int32_t) device_id;
    mt->dltensor.ndim = (int32_t) ndim;
    mt->dltensor.dtype = dtype;
    mt->dltensor.shape = shape;
    mt->dltensor.strides = strides;
    mt->dltensor.byte_offset = data_offset;
    result->mt_versioned = mt.release();
    result->refcount = 0;
    result->owner = owner;
    result->self = nullptr;
    result->versioned = true;
    result->free_strides = false;
    result->call_deleter = false;
    result->ro = ro;
    Py_XINCREF(owner);
    return result.release();
}

PyObject *ndarray_export(ndarray_handle *th, int framework,
                         rv_policy policy, cleanup_list *cleanup) noexcept {
    if (!th)
        return none().release().ptr();

    bool copy;
    switch (policy) {
        case rv_policy::reference_internal:
            if (cleanup && cleanup->self() != th->owner && !th->self) {
                if (th->owner) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "nanobind::detail::ndarray_export(): "
                                    "reference_internal policy cannot be "
                                    "applied (ndarray already has an owner)");
                    return nullptr;
                } else {
                    th->owner = cleanup->self();
                    Py_INCREF(th->owner);
                }
            }
            [[fallthrough]];

        case rv_policy::automatic:
        case rv_policy::automatic_reference:
            copy = th->owner == nullptr && th->self == nullptr;
            break;

        case rv_policy::copy:
        case rv_policy::move:
            copy = true;
            break;

        default:
            copy = false;
            break;
    }

    if (!copy) {
        if (th->self) {
            Py_INCREF(th->self);
            return th->self;
        } else if (policy == rv_policy::none) {
            return nullptr;
        }
    }

    object o;
    if (copy && framework == no_framework::value && th->self) {
        o = borrow(th->self);
    } else if (framework == no_framework::value ||
               framework == tensorflow::value) {
        // Make a new capsule wrapping an unversioned managed_dltensor.
        o = steal(th->make_capsule_unversioned());
    } else {
        // Make a Python object providing the buffer interface and having
        // the two DLPack methods __dlpack__() and __dlpack_device__().
        nb_ndarray *h = PyObject_New(nb_ndarray, nb_ndarray_tp());
        if (!h)
            return nullptr;
        h->th = th;
        ndarray_inc_ref(th);
        o = steal((PyObject *) h);
    }

    if (framework == numpy::value) {
        try {
            PyObject* pkg_mod = module_import("numpy");
            PyObject* args[] = {pkg_mod, o.ptr(),
                                (copy) ? Py_True : Py_False};
            Py_ssize_t nargsf = 2 | PY_VECTORCALL_ARGUMENTS_OFFSET;
            return PyObject_VectorcallMethod(
                        static_pyobjects[pyobj_name::array_str], args, nargsf,
                        static_pyobjects[pyobj_name::copy_tpl]);
        } catch (const std::exception &e) {
            PyErr_Format(PyExc_TypeError,
                         "could not export nanobind::ndarray: %s",
                         e.what());
            return nullptr;
        }
    }

    try {
        const char* pkg_name;
        switch (framework) {
            case pytorch::value:
                pkg_name = "torch.utils.dlpack";
                break;
            case tensorflow::value:
                pkg_name = "tensorflow.experimental.dlpack";
                break;
            case jax::value:
                pkg_name = "jax.dlpack";
                break;
            case cupy::value:
                pkg_name = "cupy";
                break;
            case memview::value:
                return PyMemoryView_FromObject(o.ptr());
            default:
                pkg_name = nullptr;
        }
        if (pkg_name) {
            PyObject* pkg_mod = module_import(pkg_name);
            PyObject* args[] = {pkg_mod, o.ptr()};
            Py_ssize_t nargsf = 2 | PY_VECTORCALL_ARGUMENTS_OFFSET;
            o = steal(PyObject_VectorcallMethod(
                          static_pyobjects[pyobj_name::from_dlpack_str],
                          args, nargsf, nullptr));
        }
    } catch (const std::exception &e) {
        PyErr_Format(PyExc_TypeError,
                     "could not export nanobind::ndarray: %s",
                     e.what());
        return nullptr;
    }

    if (copy) {
        PyObject* copy_function_name = static_pyobjects[pyobj_name::copy_str];
        if (framework == pytorch::value)
            copy_function_name = static_pyobjects[pyobj_name::clone_str];

        try {
            o = o.attr(copy_function_name)();
        } catch (std::exception &e) {
            PyErr_Format(PyExc_RuntimeError,
                         "copying nanobind::ndarray failed: %s",
                         e.what());
            return nullptr;
        }
    }

    return o.release().ptr();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
