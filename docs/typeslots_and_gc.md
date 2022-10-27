# Custom type slots, and integrating with Python's cyclic garbage collector

_nanobind_ exposes a low-level interface to add custom type slots
(``PyType_Slot`` in CPython API) to newly constructed types. This provides an
escape hatch to implement more complex features that are not supported using
builtin _nanobind_ features.

To use this feature, specify the ``nb::type_slots`` annotation when
creating the type.

```cpp
nb::class_<MyClass>(m, "MyClass", nb::type_slots(slots));
```

Here, ``slots`` should refer to an array of function pointers
that are tagged with a corresponding slot identifier. For example,
here is a table and example function that overrides the addition
operator so that it behaves like a multiplication.


```cpp
PyObject *myclass_tp_add(PyObject *a, PyObject *b) {
    return PyNumber_Multiply(a, b);
}

PyType_Slot slots[] = {
    { Py_nb_add, (void *) myclass_tp_add },
    { 0, nullptr }
};
```

The last entry of the ``slots`` table must equal ``{ 0, nullptr }`` and
indicates the end of this arbitrary-length data structure. Information on type
slots can be found in the CPython documentation of [type
objects](https://docs.python.org/3/c-api/typeobj.html) and [type
construction](https://docs.python.org/3/c-api/type.html).

This example is somewhat silly because it could have easily been accomplished
using builtin _nanobind_ features. The next section introduces a more
interesting example.

## Integrating C++ class bindings with Python's cyclic garbage collector

Python's main mechanism for reasoning about object lifetime is based on
reference counting. An object can be safely deconstructed once it is no longer
referenced from elsewhere, which occurs when its reference count reaches zero.

This mechanism is simple and efficient, but it breaks down when objects can
be part of reference cycles. For example, consider the following data structure

```cpp
struct Wrapper {
    std::shared_ptr<Wrapper> value;
};
```

with associated bindings

```cpp
nb::class_<Wrapper>(m, "Wrapper")
    .def(nb::init<>())
    .def_readwrite("value", &Wrapper::value);
```

If we instantiate this class with a cycle, it can never be reclaimed (even when
Python shuts down and is supposed to free up all memory).

```python
a = Wrapper()
a.value = a
del a
```

Nanobind will loudly complain about this:

```
nanobind: leaked 1 instances!
nanobind: leaked 1 types!
 - leaked type "test_classes_ext.Wrapper"
nanobind: leaked 3 functions!
 - leaked function "<anonymous>"
 - leaked function "__init__"
 - leaked function "<anonymous>"
nanobind: this is likely caused by a reference counting issue in the binding code.
```

The leaked instance references its type, which references several function
definitions, causing a longer sequence of leak warnings.

To fix this problem, Python's cyclic garbage collector must know how objects
are connected to each other. It can do this automatically for Python objects,
but it needs some help for bound C++ types

We will do this by exposing a ``tp_traverse`` type slot that walks through
the object graph, and a ``tp_clear`` type slot that can be called to break
a cycle.

```cpp
int wrapper_tp_traverse(PyObject *self, visitproc visit, void *arg) {
    // Retrieve a pointer to the C++ instance associated with 'self' (this can never fail)
    Wrapper *w = nb::inst_ptr<Wrapper>(self);

    // If w->value has an associated CPython object, return it.
    // If not, value.ptr() will equal NULL, which is also fine.
    nb::object value = nb::find(w->value);

    // Inform the Python GC about the instance (if non-NULL)
    Py_VISIT(value.ptr());

    return 0;
}

int wrapper_tp_clear(PyObject *self) {
    // Retrieve a pointer to the C++ instance associated with 'self' (this can never fail)
    Wrapper *w = nb::inst_ptr<Wrapper>(self);

    // Clear the cycle!
    w->value.reset();

    return 0;
}

// Slot data structure referencing the above two functions
PyType_Slot slots[] = {
    { Py_tp_traverse, (void *) wrapper_tp_traverse },
    { Py_tp_clear, (void *) wrapper_tp_clear },
    { 0, nullptr }
};
```

The expression ``nb::inst_ptr<Wrapper>(self)`` efficiently returns the C++
instance associated with a Python object and is documented as part of
_nanobind_'s [low level
interface](https://github.com/wjakob/nanobind/blob/master/docs/lowlevel.md).

Note the use of the ``nb::find()`` function, which behaves like ``nb::cast()`` by
returning the Python object associated with a C++ instance. The main
difference is that ``nb::cast()`` will create the Python object if it doesn't
exist, while ``nb::find()`` returns a ``nullptr`` object in that case.

To activate this machinery, the ``Wrapper`` type bindings must be made aware
of these extra type slots:

```
nb::class_<Wrapper>(m, "Wrapper", nb::type_slots(slots))
```

With this change, the cycle can be garbage-collected, and the leak warnings
disappear.

## More garbage collection subtlety: function objects

What if our wrapper class from the previous example instead stored
a function object?

```cpp
struct Wrapper {
    std::function<void(void)> value;
};
```

This can also be used to create a reference cycle! For example,
in Python we could write

```python
a = Wrapper()
a.value = lambda: print(a)
```

The function is now a
[closure](https://en.wikipedia.org/wiki/Closure_(computer_programming)) that
references external state. In this case, the ``a`` instance, which creates an
inter-language cycle, (``Wrapper`` → ``function`` wrapped in
``std::function<void(void)>`` → ``Wrapper``).

This is a quite common situation whenever Python callbacks can be registered
with abound C++ class (e.g., in a GUI framework), and hence it is important to
detect and handle reference cycles.

Via ``nb::find()``, it is possible to retrieve the Python ``function`` object
associated with the ``std::function<>``, which means that the previous
traversal function continues to work without changes.

```cpp
int wrapper_tp_traverse(PyObject *self, visitproc visit, void *arg) {
    Wrapper *w = nb::inst_ptr<Wrapper>(self);

    // If c->value corresponds to an associated CPython object, return it
    nb::object value = nb::find(w->value);

    // Inform the Python GC about it
    Py_VISIT(value.ptr());

    return 0;
}
```

Only the ``tp_clear`` slot requires small touchups:

```cpp
int wrapper_tp_clear(PyObject *self) {
    Wrapper *w = nb::inst_ptr<Wrapper>(self);
    w->value = std::function<void(void)>();
    return 0;
}
```
That's it!
