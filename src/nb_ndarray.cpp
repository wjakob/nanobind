#include <nanobind/ndarray.h>
#include <atomic>
#include "nb_internals.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// ========================================================================

struct managed_dltensor {
    dlpack::dltensor dltensor;
    void *manager_ctx;
    void (*deleter)(managed_dltensor *);
};

struct ndarray_handle {
    managed_dltensor *ndarray;
    std::atomic<size_t> refcount;
    PyObject *owner, *self;
    bool free_shape;
    bool free_strides;
    bool call_deleter;
    bool ro;
};

static void ndarray_capsule_destructor(PyObject *o) {
    error_scope scope; // temporarily save any existing errors
    managed_dltensor *mt =
        (managed_dltensor *) PyCapsule_GetPointer(o, "dltensor");

    if (mt)
        ndarray_dec_ref((ndarray_handle *) mt->manager_ctx);
    else
        PyErr_Clear();
}

static void nb_ndarray_dealloc(PyObject *self) {
    PyTypeObject *tp = Py_TYPE(self);
    ndarray_dec_ref(((nb_ndarray *) self)->th);
    PyObject_Free(self);
    Py_DECREF(tp);
}

static int nd_ndarray_tpbuffer(PyObject *exporter, Py_buffer *view, int) {
    nb_ndarray *self = (nb_ndarray *) exporter;

    dlpack::dltensor &t = self->th->ndarray->dltensor;

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
        PyErr_SetString(
            PyExc_BufferError,
            "Don't know how to convert DLPack dtype into buffer protocol format!");
        return -1;
    }

    view->format = (char *) format;
    view->itemsize = t.dtype.bits / 8;
    view->buf = (void *) ((uintptr_t) t.data + t.byte_offset);
    view->obj = exporter;
    Py_INCREF(exporter);

    Py_ssize_t len = view->itemsize;
    scoped_pymalloc<Py_ssize_t> strides((size_t) t.ndim),
                                  shape((size_t) t.ndim);

    for (size_t i = 0; i < (size_t) t.ndim; ++i) {
        len *= (Py_ssize_t) t.shape[i];
        strides[i] = (Py_ssize_t) t.strides[i] * view->itemsize;
        shape[i] = (Py_ssize_t) t.shape[i];
    }

    view->ndim = t.ndim;
    view->len = len;
    view->readonly = self->th->ro;
    view->suboffsets = nullptr;
    view->internal = nullptr;
    view->strides = strides.release();
    view->shape = shape.release();

    return 0;
}

static void nb_ndarray_releasebuffer(PyObject *, Py_buffer *view) {
    PyMem_Free(view->shape);
    PyMem_Free(view->strides);
}


static PyObject *nb_ndarray_dlpack(PyObject *self, PyTypeObject *,
                            PyObject *const *, Py_ssize_t ,
                            PyObject *) {
    nb_ndarray *self_nd = (nb_ndarray *) self;
    ndarray_handle *th = self_nd->th;

    PyObject *r =
        PyCapsule_New(th->ndarray, "dltensor", ndarray_capsule_destructor);
    if (r)
        ndarray_inc_ref(th);
    return r;
}

static PyObject *nb_ndarray_dlpack_device(PyObject *self, PyTypeObject *,
                            PyObject *const *, Py_ssize_t ,
                            PyObject *) {
    nb_ndarray *self_nd = (nb_ndarray *) self;
    dlpack::dltensor &t = self_nd->th->ndarray->dltensor;
    PyObject *r = PyTuple_New(2);
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
    return r;
}

static PyMethodDef nb_ndarray_members[] = {
   { "__dlpack__", (PyCFunction) (void *) nb_ndarray_dlpack, METH_FASTCALL | METH_KEYWORDS, nullptr },
   { "__dlpack_device__", (PyCFunction) (void *) nb_ndarray_dlpack_device, METH_FASTCALL | METH_KEYWORDS, nullptr },
   { nullptr, nullptr, 0, nullptr }
};

static PyTypeObject *nd_ndarray_tp() noexcept {
    nb_internals *internals_ = internals;
    PyTypeObject *tp = internals_->nb_ndarray.load_acquire();

    if (NB_UNLIKELY(!tp)) {
        lock_internals guard(internals_);
        tp = internals_->nb_ndarray.load_relaxed();
        if (tp)
            return tp;

        PyType_Slot slots[] = {
            { Py_tp_dealloc, (void *) nb_ndarray_dealloc },
            { Py_tp_methods, (void *) nb_ndarray_members },
#if PY_VERSION_HEX >= 0x03090000
            { Py_bf_getbuffer, (void *) nd_ndarray_tpbuffer },
            { Py_bf_releasebuffer, (void *) nb_ndarray_releasebuffer },
#endif
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

#if PY_VERSION_HEX < 0x03090000
        tp->tp_as_buffer->bf_getbuffer = nd_ndarray_tpbuffer;
        tp->tp_as_buffer->bf_releasebuffer = nb_ndarray_releasebuffer;
#endif

        internals_->nb_ndarray.store_release(tp);
    }

    return tp;
}

static PyObject *dlpack_from_buffer_protocol(PyObject *o, bool ro) {
    scoped_pymalloc<Py_buffer> view;
    scoped_pymalloc<managed_dltensor> mt;

    if (PyObject_GetBuffer(o, view.get(),
                           ro ? PyBUF_RECORDS_RO : PyBUF_RECORDS)) {
        PyErr_Clear();
        return nullptr;
    }

    char format_c = 'B';
    const char *format_str = view->format;
    if (format_str)
        format_c = *format_str;

    bool skip_first = format_c == '@' || format_c == '=';

    int32_t num = 1;
    if(*(uint8_t *) &num == 1) {
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

            default:
                fail = true;
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
        return nullptr;
    }

    mt->deleter = [](managed_dltensor *mt2) {
        gil_scoped_acquire guard;
        Py_buffer *buf = (Py_buffer *) mt2->manager_ctx;
        PyBuffer_Release(buf);
        PyMem_Free(mt2->manager_ctx);
        PyMem_Free(mt2->dltensor.shape);
        PyMem_Free(mt2->dltensor.strides);
        PyMem_Free(mt2);
    };

    /* DLPack mandates 256-byte alignment of the 'DLTensor::data' field, but
       PyTorch unfortunately ignores the 'byte_offset' value.. :-( */
#if 0
    uintptr_t value_int = (uintptr_t) view->buf,
              value_rounded = (value_int / 256) * 256;
#else
    uintptr_t value_int = (uintptr_t) view->buf,
              value_rounded = value_int;
#endif

    mt->dltensor.data = (void *) value_rounded;
    mt->dltensor.device = { device::cpu::value, 0 };
    mt->dltensor.ndim = view->ndim;
    mt->dltensor.dtype = dt;
    mt->dltensor.byte_offset = value_int - value_rounded;

    scoped_pymalloc<int64_t> strides((size_t) view->ndim);
    scoped_pymalloc<int64_t> shape((size_t) view->ndim);
    const int64_t itemsize = static_cast<int64_t>(view->itemsize);
    for (size_t i = 0; i < (size_t) view->ndim; ++i) {
        int64_t stride = view->strides[i] / itemsize;
        if (stride * itemsize != view->strides[i]) {
            PyBuffer_Release(view.get());
            return nullptr;
        }
        strides[i] = stride;
        shape[i] = (int64_t) view->shape[i];
    }

    mt->manager_ctx = view.release();
    mt->dltensor.shape = shape.release();
    mt->dltensor.strides = strides.release();

    return PyCapsule_New(mt.release(), "dltensor", [](PyObject *o) {
        error_scope scope; // temporarily save any existing errors
        managed_dltensor *mt =
            (managed_dltensor *) PyCapsule_GetPointer(o, "dltensor");
        if (mt) {
            if (mt->deleter)
                mt->deleter(mt);
        } else {
            PyErr_Clear();
        }
    });
}

bool ndarray_check(PyObject *o) noexcept {
    if (PyObject_HasAttrString(o, "__dlpack__") || PyObject_CheckBuffer(o))
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


ndarray_handle *ndarray_import(PyObject *o, const ndarray_config *c,
                               bool convert, cleanup_list *cleanup) noexcept {
    object capsule;
    bool is_pycapsule = PyCapsule_CheckExact(o);

    // If this is not a capsule, try calling o.__dlpack__()
    if (!is_pycapsule) {
        capsule = steal(PyObject_CallMethod(o, "__dlpack__", nullptr));

        if (!capsule.is_valid()) {
            PyErr_Clear();
            PyTypeObject *tp = Py_TYPE(o);

            try {
                const char *module_name =
                    borrow<str>(handle(tp).attr("__module__")).c_str();

                object package;
                if (strncmp(module_name, "tensorflow.", 11) == 0)
                    package = module_::import_("tensorflow.experimental.dlpack");
                else if (strcmp(module_name, "torch") == 0)
                    package = module_::import_("torch.utils.dlpack");
                else if (strncmp(module_name, "jaxlib", 6) == 0)
                    package = module_::import_("jax.dlpack");

                if (package.is_valid())
                    capsule = package.attr("to_dlpack")(handle(o));
            } catch (...) {
                capsule.reset();
            }
        }

        // Try creating an ndarray via the buffer protocol
        if (!capsule.is_valid())
            capsule = steal(dlpack_from_buffer_protocol(o, c->ro));

        if (!capsule.is_valid())
            return nullptr;
    } else {
        capsule = borrow(o);
    }

    // Extract the pointer underlying the capsule
    void *ptr = PyCapsule_GetPointer(capsule.ptr(), "dltensor");
    if (!ptr) {
        PyErr_Clear();
        return nullptr;
    }

    // Check if the ndarray satisfies the requirements
    dlpack::dltensor &t = ((managed_dltensor *) ptr)->dltensor;

    bool has_dtype = c->dtype != dlpack::dtype(),
         has_device_type = c->device_type != 0,
         has_shape = c->ndim != -1,
         has_order = c->order != '\0';

    bool pass_dtype = true, pass_device = true,
         pass_shape = true, pass_order = true;

    if (has_dtype)
        pass_dtype = t.dtype == c->dtype;

    if (has_device_type)
        pass_device = t.device.device_type == c->device_type;

    if (has_shape) {
        pass_shape &= c->ndim == t.ndim;

        if (pass_shape) {
            for (int32_t i = 0; i < c->ndim; ++i) {
                if (c->shape[i] != t.shape[i] &&
                    c->shape[i] != -1) {
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

    bool refused_conversion = t.dtype.code == (uint8_t) dlpack::dtype_code::Complex &&
                              has_dtype &&
                              c->dtype.code != (uint8_t) dlpack::dtype_code::Complex;

    // Support implicit conversion of 'dtype' and order
    if (pass_device && pass_shape && (!pass_dtype || !pass_order) && convert &&
        capsule.ptr() != o && !refused_conversion) {
        PyTypeObject *tp = Py_TYPE(o);
        str module_name_o = borrow<str>(handle(tp).attr("__module__"));
        const char *module_name = module_name_o.c_str();

        char order = 'K'; // for NumPy. 'K' means 'keep'
        if (c->order)
            order = c->order;

        dlpack::dtype dt = has_dtype ? c->dtype : t.dtype;
        if (dt.lanes != 1)
            return nullptr;

        const char *prefix = nullptr;
        char dtype[11];
        if (dt.code == (uint8_t) dlpack::dtype_code::Bool) {
            std::strcpy(dtype, "bool");
        } else {
            switch (dt.code) {
                case (uint8_t) dlpack::dtype_code::Int: prefix = "int"; break;
                case (uint8_t) dlpack::dtype_code::UInt: prefix = "uint"; break;
                case (uint8_t) dlpack::dtype_code::Float: prefix = "float"; break;
                case (uint8_t) dlpack::dtype_code::Complex: prefix = "complex"; break;
                default:
                    return nullptr;
            }
            snprintf(dtype, sizeof(dtype), "%s%u", prefix, dt.bits);
        }

        object converted;
        try {
            if (strcmp(module_name, "numpy") == 0 || strcmp(module_name, "cupy") == 0) {
                converted = handle(o).attr("astype")(dtype, order);
            } else if (strcmp(module_name, "torch") == 0) {
                converted = handle(o).attr("to")(
                    arg("dtype") = module_::import_("torch").attr(dtype));
                if (c->order == 'C')
                    converted = converted.attr("contiguous")();
            } else if (strncmp(module_name, "tensorflow.", 11) == 0) {
                converted = module_::import_("tensorflow")
                                .attr("cast")(handle(o), dtype);
            } else if (strncmp(module_name, "jaxlib", 6) == 0) {
                converted = handle(o).attr("astype")(dtype);
            }
        } catch (...) { converted.reset(); }

        // Potentially try again recursively
        if (!converted.is_valid()) {
            return nullptr;
        } else {
            ndarray_handle *h =
                ndarray_import(converted.ptr(), c, false, nullptr);
            if (h && cleanup)
                cleanup->append(converted.release().ptr());
            return h;
        }
    }

    if (!pass_dtype || !pass_device || !pass_shape || !pass_order)
        return nullptr;

    // Create a reference-counted wrapper
    scoped_pymalloc<ndarray_handle> result;
    result->ndarray = (managed_dltensor *) ptr;
    result->refcount = 0;
    result->owner = nullptr;
    result->free_shape = false;
    result->call_deleter = true;
    result->ro = c->ro;

    if (is_pycapsule) {
        result->self = nullptr;
    } else {
        result->self = o;
        Py_INCREF(o);
    }

    // Ensure that the strides member is always initialized
    if (t.strides) {
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

    // Mark the dltensor capsule as "consumed"
    if (PyCapsule_SetName(capsule.ptr(), "used_dltensor") ||
        PyCapsule_SetDestructor(capsule.ptr(), nullptr))
        check(false, "nanobind::detail::ndarray_import(): could not mark "
                     "dltensor capsule as consumed!");

    return result.release();
}

dlpack::dltensor *ndarray_inc_ref(ndarray_handle *th) noexcept {
    if (!th)
        return nullptr;
    ++th->refcount;
    return &th->ndarray->dltensor;
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
        managed_dltensor *mt = th->ndarray;
        if (th->free_shape) {
            PyMem_Free(mt->dltensor.shape);
            mt->dltensor.shape = nullptr;
        }
        if (th->free_strides) {
            PyMem_Free(mt->dltensor.strides);
            mt->dltensor.strides = nullptr;
        }
        if (th->call_deleter) {
            if (mt->deleter)
                mt->deleter(mt);
        } else {
            PyMem_Free(mt);
        }
        PyMem_Free(th);
    }
}

ndarray_handle *ndarray_create(void *value, size_t ndim, const size_t *shape_in,
                               PyObject *owner, const int64_t *strides_in,
                               dlpack::dtype dtype, bool ro, int device_type,
                               int device_id, char order) {
    /* DLPack mandates 256-byte alignment of the 'DLTensor::data' field, but
       PyTorch unfortunately ignores the 'byte_offset' value.. :-( */
#if 0
    uintptr_t value_int = (uintptr_t) value,
              value_rounded = (value_int / 256) * 256;
#else
    uintptr_t value_int = (uintptr_t) value,
              value_rounded = value_int;
#endif
    if (device_type == 0)
        device_type = device::cpu::value;

    scoped_pymalloc<managed_dltensor> ndarray;
    scoped_pymalloc<ndarray_handle> result;
    scoped_pymalloc<int64_t> shape(ndim), strides(ndim);

    auto deleter = [](managed_dltensor *mt) {
        gil_scoped_acquire guard;
        ndarray_handle *th = (ndarray_handle *) mt->manager_ctx;
        ndarray_dec_ref(th);
    };

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
            check(false, "nanobind::detail::ndarray_create(): unknown "
                         "memory order requested!");
        }
    }

    ndarray->dltensor.data = (void *) value_rounded;
    ndarray->dltensor.device.device_type = (int32_t) device_type;
    ndarray->dltensor.device.device_id = (int32_t) device_id;
    ndarray->dltensor.ndim = (int32_t) ndim;
    ndarray->dltensor.dtype = dtype;
    ndarray->dltensor.byte_offset = value_int - value_rounded;
    ndarray->dltensor.shape = shape.release();
    ndarray->dltensor.strides = strides.release();
    ndarray->manager_ctx = result.get();
    ndarray->deleter = deleter;
    result->ndarray = (managed_dltensor *) ndarray.release();
    result->refcount = 0;
    result->owner = owner;
    result->self = nullptr;
    result->free_shape = true;
    result->free_strides = true;
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
    } else if (framework == numpy::value || framework == jax::value) {
        nb_ndarray *h = PyObject_New(nb_ndarray, nd_ndarray_tp());
        if (!h)
            return nullptr;
        h->th = th;
        ndarray_inc_ref(th);
        o = steal((PyObject *) h);
    } else {
        o = steal(PyCapsule_New(th->ndarray, "dltensor",
                                ndarray_capsule_destructor));
        ndarray_inc_ref(th);
    }

    try {
        if (framework == numpy::value) {
            return module_::import_("numpy")
                .attr("array")(o, arg("copy") = copy)
                .release()
                .ptr();
        } else {
            const char *pkg_name;
            switch (framework) {
                case pytorch::value: pkg_name = "torch.utils.dlpack"; break;
                case tensorflow::value: pkg_name = "tensorflow.experimental.dlpack"; break;
                case jax::value: pkg_name = "jax.dlpack"; break;
                case cupy::value: pkg_name = "cupy"; break;
                default: pkg_name = nullptr;
            }

            if (pkg_name)
                o = module_::import_(pkg_name).attr("from_dlpack")(o);
        }
    } catch (const std::exception &e) {
        PyErr_Format(PyExc_RuntimeError,
                     "nanobind::detail::ndarray_export(): could not "
                     "import ndarray: %s",
                     e.what());
        return nullptr;
    }

    if (copy) {
        const char* copy_str = "copy";
        if (framework == pytorch::value)
            copy_str = "clone";

        try {
            o = o.attr(copy_str)();
        } catch (std::exception &e) {
            PyErr_Format(PyExc_RuntimeError,
                         "nanobind::detail::ndarray_export(): copy failed: %s",
                         e.what());
            return nullptr;
        }
    }

    return o.release().ptr();
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
