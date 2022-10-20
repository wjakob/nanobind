#include "object.h"
#include <stdexcept>

static void (*object_inc_ref_py)(PyObject *) noexcept = nullptr;
static void (*object_dec_ref_py)(PyObject *) noexcept = nullptr;

void Object::inc_ref() const noexcept {
    uintptr_t value = m_state.load(std::memory_order_relaxed);

    while (true) {
        if (value & 1) {
            if (!m_state.compare_exchange_weak(value,
                                               value + 2,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed))
                continue;
        } else {
            object_inc_ref_py((PyObject *) value);
        }

        break;
    }
}

void Object::dec_ref() const noexcept {
    uintptr_t value = m_state.load(std::memory_order_relaxed);

    while (true) {
        if (value & 1) {
            if (value == 1) {
                fprintf(stderr, "Object::dec_ref(%p): reference count underflow!", this);
                abort();
            } else if (value == 3) {
                delete this;
            } else {
                if (!m_state.compare_exchange_weak(value,
                                                   value - 2,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed))
                    continue;
            }
        } else {
            object_dec_ref_py((PyObject *) value);
        }
        break;
    }
}

void Object::set_self_py(PyObject *o) noexcept {
    uintptr_t value = m_state.load(std::memory_order_relaxed);
    if (value & 1) {
        value >>= 1;
        for (uintptr_t i = 0; i < value; ++i)
            object_inc_ref_py(o);

        m_state.store((uintptr_t) o);
    } else {
        fprintf(stderr, "Object::set_self_py(%p): a Python object was already present!", this);
        abort();
    }
}

PyObject *Object::self_py() const noexcept {
    uintptr_t value = m_state.load(std::memory_order_relaxed);
    if (value & 1)
        return nullptr;
    else
        return (PyObject *) value;
}

void object_init_py(void (*object_inc_ref_py_)(PyObject *) noexcept,
                    void (*object_dec_ref_py_)(PyObject *) noexcept) {
    object_inc_ref_py = object_inc_ref_py_;
    object_dec_ref_py = object_dec_ref_py_;
}
