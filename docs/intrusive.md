# Intrusive reference counting

Like _pybind11_, _nanobind_ provides a way of binding classes with builtin
("intrusive") reference counting. This is the most general and cheapest way of
handling shared ownership between C++ and Python, but it requires that the base
class of an object hierarchy is adapted according to the needs of _nanobind_.

Ordinarily, a simple class with intrusive reference counting might look as
follows:

```cpp
class Object {
public:
    void inc_ref() const noexcept { ++m_ref_count; }

    void dec_ref() const noexcept {
        if (--m_ref_count == 0)
            delete this;
    }

private:
    mutable std::atomic<size_t> m_ref_count { 0 };
};
```

The advantage of this over standard approaches like `std::shared_ptr<T>` is
that no separate control block must be allocated. Subtle technical band-aids
like `std::enable_shared_from_this<T>` to avoid undefined behavior are also
no longer necessary.

However, one issue that tends to arise when a type like `Object` is wrapped
using _nanobind_ is that there are now *two* separate reference counts
referring to the same object: one in Python's `PyObject`, and one in `Object`.
This can lead to a problematic reference cycle:

- Python's `PyObject` needs to keep `Object` alive so that the instance can be
  safely passed to C++ functions.

- The C++ `Object` may in turn need to keep the `PyObject` alive. This is the
  case when a subclass uses `NB_TRAMPOLINE` and `NB_OVERRIDE` features to route
  C++ virtual function calls back to a Python implementation.

The source of the problem is that there are *two* separate counters that try to
reason about the reference count of *one* instance. The solution is to reduce
this to just one counter: 

- if an instance lives purely on the C++ side, the `m_ref_count` field is
  used to reason about the number of references.

- The first time that an instance is exposed to Python (by being created from
  Python, or by being returned from a bound C++ function), lifetime management
  is delegated to Python.

The files
[`tests/object.h`](https://github.com/wjakob/nanobind/blob/master/tests/object.h)
and
[`tests/object.cpp`](https://github.com/wjakob/nanobind/blob/master/tests/object.cpp)
contain an example implementation of a suitable base class named `Object`. It
contains an extra optimization to use a single field of type
`std::atomic<uintptr_t> m_state;` (8 bytes) to store *either* a reference
counter or a pointer to a `PyObject*`.

The main change in _nanobind_-based bindings is that the base class must
specify a `nb::intrusive_ptr` annotation to inform an instance that lifetime
management has been taken over by Python. This annotation is automatically
inherited by all subclasses.

```cpp
nb::class_<Object>(
    m, "Object",
    nb::intrusive_ptr<Object>(
        [](Object *o, PyObject *po) noexcept { o->set_self_py(po); }));
```

