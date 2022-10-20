/**
 * This file contains an exemplary base object class with intrusive reference
 * counting. The implementation is designed so that it does not have a direct
 * dependency on Python or nanobind. It can therefore be used in codebases
 * where a Python interface is merely an optional component.
 *
 * The file 'docs/intrusive.md' explains the basic rationale of intrusive
 * reference counting, while comments in this file and 'object.cpp' explain
 * technical aspects.
 */

#include <atomic>
#pragma once

/* While the implementation does not directly depend on Python, the PyObject*
   type occurs in a few function interfaces (in a fully opaque manner). We must
   therefore forward-declare it. */
extern "C" {
    struct _object;
    typedef _object PyObject;
};

/**
 * \brief Object base class with intrusive reference counting
 *
 * The Object class provides a convenient foundation of a class hierarchy that
 * will ease lifetime and ownership-related issues whenever Python bindings are
 * involved.
 *
 * Internally, its constructor sets the `m_state` field to `1`, which indicates
 * that the instance is owned by C++. Bits 2..63 of this field are used to
 * store the actual reference count value. The `inc_ref()` and `dec_ref()`
 * functions can be used to increment or decrement this reference count. When
 * `dec_ref()` removes the last reference, the instance will be deallocated
 * using a `delete` expression handled using a polymorphic destructor.
 *
 * When a subclass of `Object` is constructed to Python or returned from C++ to
 * Python, nanobind will invoke `Object::set_self_py()`, which hands ownership
 * over to Python/nanobind. Any remaining references will be moved from the
 * `m_state` field to the Python reference count. In this mode, `inc_ref()` and
 * `dec_ref()` wrap Python reference counting primitives (`Py_INCREF()` /
 * `Py_DECREF()`) which must be made available by calling the function
 * `object_init_py` once during module initialization. Note that the `m_state`
 * field is also used to store a pointer to the `PyObject *`. Python instance
 * pointers are always aligned (i.e. bit 1 is zero), which disambiguates
 * between the two possible configurations.
 *
 * Within C++, the RAII helper class `ref` (defined below) can be used to keep
 * instances alive. This removes the need to call the `inc_ref()` / `dec_ref()`
 * functions explicitly.
 *
 * ```
 * {
 *    ref<MyClass> inst = new MyClass();
 *    inst->my_function();
 *    ...
 * } // end of scope, 'inst' automatically deleted if no longer referenced
 * ```
 *
 * A separate optional file ``object_py.h`` provides a nanobind type caster
 * to bind functions taking/returning values of type `ref<T>`.
 */
class Object {
public:
    Object() = default;

    /* The following move/assignment constructors/operators are no-ops. They
       intentionally do not change the reference count field (m_state) that
       is associated with a fixed address in memory */
    Object(const Object &) : Object() { }
    Object(Object &&) : Object() { }
    Object &operator=(const Object &) { return *this; }
    Object &operator=(Object &&) { return *this; }

    // Polymorphic default destructor
    virtual ~Object() = default;

    /// Increase the object's reference count
    void inc_ref() const noexcept;

    /// Decrease the object's reference count and potentially deallocate it
    void dec_ref() const noexcept;

    /// Return the Python object associated with this instance (or NULL)
    PyObject *self_py() const noexcept;

    /// Set the Python object associated with this instance
    void set_self_py(PyObject *self) noexcept;

private:
    mutable std::atomic<uintptr_t> m_state { 1 };
};

/**
 * \brief Install Python reference counting handlers
 *
 * The `Object` class is designed so that the dependency on Python is
 * *optional*: the code compiles in ordinary C++ projects, in which case the
 * Python reference counting functionality will simply not be used.
 *
 * Python binding code must invoke `object_init_py` and provide functions that
 * can be used to increase/decrease the Python reference count of an instance
 * (i.e., `Py_INCREF` / `Py_DECREF`).
 */
void object_init_py(void (*object_inc_ref_py)(PyObject *) noexcept,
                    void (*object_dec_ref_py)(PyObject *) noexcept);


/**
 * \brief RAII reference counting helper class
 *
 * ``ref`` is a simple RAII wrapper class that stores a pointer to a subclass
 * of ``Object``. It takes care of increasing and decreasing the reference
 * count of the underlying instance. When the last reference goes out of scope,
 * the associated object will be deallocated.
 *
 * A separate optional file ``object_py.h`` provides a nanobind type caster
 * to bind functions taking/returning values of type `ref<T>`.
 */
template <typename T> class ref {
public:
    /// Create a nullptr reference
    ref() : m_ptr(nullptr) { }

    /// Construct a reference from a pointer
    ref(T *ptr) : m_ptr(ptr) {
        if (ptr)
            ((Object *) ptr)->inc_ref();
    }

    /// Copy constructor
    ref(const ref &r) : m_ptr(r.m_ptr) {
        if (m_ptr)
            ((Object *) m_ptr)->inc_ref();
    }

    /// Move constructor
    ref(ref &&r) noexcept : m_ptr(r.m_ptr) {
        r.m_ptr = nullptr;
    }

    /// Destroy this reference
    ~ref() {
        if (m_ptr)
            ((Object *) m_ptr)->dec_ref();
    }

    /// Move another reference into the current one
    ref &operator=(ref &&r) noexcept {
        if (m_ptr)
            ((Object *) m_ptr)->dec_ref();
        m_ptr = r.m_ptr;
        r.m_ptr = nullptr;
        return *this;
    }

    /// Overwrite this reference with another reference
    ref &operator=(const ref &r) {
        if (r.m_ptr)
            ((Object *) r.m_ptr)->inc_ref();
        if (m_ptr)
            ((Object *) m_ptr)->dec_ref();
        m_ptr = r.m_ptr;
        return *this;
    }

    /// Overwrite this reference with a pointer to another object
    ref &operator=(T *ptr) {
        if (ptr)
            ((Object *) ptr)->inc_ref();
        if (m_ptr)
            ((Object *) m_ptr)->dec_ref();
        m_ptr = ptr;
        return *this;
    }

    /// Compare this reference with another reference
    bool operator==(const ref &r) const { return m_ptr == r.m_ptr; }

    /// Compare this reference with another reference
    bool operator!=(const ref &r) const { return m_ptr != r.m_ptr; }

    /// Compare this reference with a pointer
    bool operator==(const T *ptr) const { return m_ptr == ptr; }

    /// Compare this reference with a pointer
    bool operator!=(const T *ptr) const { return m_ptr != ptr; }

    /// Access the object referenced by this reference
    T *operator->() { return m_ptr; }

    /// Access the object referenced by this reference
    const T *operator->() const { return m_ptr; }

    /// Return a C++ reference to the referenced object
    T &operator*() { return *m_ptr; }

    /// Return a const C++ reference to the referenced object
    const T &operator*() const { return *m_ptr; }

    /// Return a pointer to the referenced object
    explicit operator T *() { return m_ptr; }

    /// Return a const pointer to the referenced object
    T *get() { return m_ptr; }

    /// Return a pointer to the referenced object
    const T *get() const { return m_ptr; }

private:
    T *m_ptr;
};
