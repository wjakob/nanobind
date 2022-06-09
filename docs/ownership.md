# Object ownership and shared/unique pointers

When a C++ type is instantiated within Python via _nanobind_, the resulting
instance is stored _within_ the created Python object (henceforth `PyObject`).
Alternatively, when an already existing C++ instance is transferred to Python
via a function return value and `rv_policy::reference`,
`rv_policy::reference_internal`, or `rv_policy::take_ownership`, _nanobind_
creates a smaller `PyObject` that only stores a pointer to the instance data.

This is _very different_ from _pybind11_, where the instance `PyObject`
contained a _holder type_ (typically `std::unique_ptr<T>`) storing a pointer to
the instance data. Dealing with holders caused inefficiencies and introduced
complexity; they were therefore removed in _nanobind_. This has implications on
object ownership, shared ownership, and interactions with C++ shared/unique
pointers.

- **Intrusive reference counting**: Like _pybind11_, _nanobind_ provides a way
  of binding classes with builtin ("intrusive") reference counting. This is the
  most general and cheapest way of handling shared ownership between C++ and
  Python, but it requires that the base class of an object hierarchy is adapted
  according to the needs of _nanobind_. Details on using intrusive reference
  counting can be found
  [here](https://github.com/wjakob/nanobind/blob/master/docs/intrusive.md).

- **Shared pointers**: It is possible to bind functions that receive and return
  `std::shared_ptr<T>` by including the optional type caster
  [`nanobind/stl/shared_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/shared_ptr.h)
  in your code.

  When calling a C++ function with a `std::shared_ptr<T>` argument from Python,
  ownership must be shared between Python and C++. _nanobind_ does this by
  increasing the reference count of the `PyObject` and then creating a
  `std::shared_ptr<T>` with a new control block containing a custom deleter
  that will in turn reduce the Python reference count upon destruction of the
  shared pointer.

  When a C++ function returns a `std::shared_ptr<T>`, _nanobind_ checks if the
  instance already has a `PyObject` counterpart (nothing needs to be done in
  this case). Otherwise, it indicates shared ownership by creating a temporary
  `std::shared_ptr<T>` on the heap that will be destructed when the `PyObject`
  is garbage collected.

  Shared pointers therefore remain usable despite the lack of _holders_. The
  approach in _nanobind_ was chosen following on discussions with [Ralf
  Grosse-Kunstleve](https://github.com/rwgk); it is unusual in that multiple
  `shared_ptr` control blocks are potentially allocated for the same object,
  which means that `std::shared_ptr<T>::use_count()` generally won't show the
  true global reference count.

  _nanobind_ refuses conversion of classes that derive from
  `std::enable_shared_from_this<T>`. This is a fundamental limitation:
  _nanobind_ instances do not create a base shared pointer that declares
  ownership of an object. Other parts of a C++ codebase might then incorrectly
  assume ownership and eventually try to `delete` a _nanobind_ instance
  allocated using `pymalloc` (which is undefined behavior). A compile-time
  assertion catches this and warns about the problem.

- **Unique pointers**: It is possible to bind functions that receive and return
  `std::unique_ptr<T, Deleter>` by including the optional type caster
  [`nanobind/stl/unique_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/unique_ptr.h)
  in your code.

  Whereas `std::shared_ptr<T>` could abstract over details concerning storage
  and the deletion mechanism, this is not possible in simpler
  `std::unique_ptr`, which means that some of those details leak into the type
  signature.

  When calling a C++ function with a `std::unique_ptr<T, Deleter>` argument
  from Python, there is an ownership transfer from Python to C++ that must be
  handled.

  * When `Deleter` is `std::default_delete<T>` (i.e., the default when no
    `Deleter` is specified), this ownership transfer is only possible when the
    instance was originally created by a _new expression_ within C++ and
    _nanobind_ has taken over ownership (i.e., it was created by a function
    returning a raw pointer `T *value` with `rv_policy::take_ownership`, or a
    function returning a `std::unique_ptr<T>`). This limitation exists because
    the `Deleter` will execute the statement `delete value` when the unique
    pointer expires, causing undefined behavior when the object was allocated
    within Python. _nanobind_ detects this and refuses such unsafe conversions
    with a warning.

  * To enable ownership transfer under all conditions, _nanobind_ provides a
    custom `Deleter` named `nb::deleter<T>` that uses reference counting to
    keep the underlying `PyObject` alive during the lifetime of the unique
    pointer. Following this route requires changing function signatures so that
    they use `std::unique_ptr<T, nb::deleter<T>>` instead of
    `std::unique_ptr<T>`. This custom deleter supports ownership by both C++
    and Python and can be used in all situations.

  In both cases, a Python object may continue to exist after ownership was
  transferred to C++ side. _nanobind_ marks this object as _invalid_: any
  operations involving it will fail with a `TypeError`. Reverse ownership
  transfer at a later point will make it usable again.

  Binding functions that return a `std::unique_ptr<T, Deleter>` always works:
  _nanobind_ will then (re-)acquire ownership of the object.

  Deleters other than `std::default_delete<T>` or `nb::deleter<T>` are _not
  supported_.
