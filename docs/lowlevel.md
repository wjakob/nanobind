# Low-level instance interface

_nanobind_ exposes a low-level interface to provide fine-grained control over
the sequence of steps that instantiates a Python object wrapping a C++
instance. This is useful when writing generic binding code that manipulates
_nanobind_-based objects of various types.

Given a previous ``nb::class_<>`` binding declaration, 
this interface can be used to look up the Python type object associated
with a C++ class named `MyClass`.

```cpp
nb::handle py_type = nb::type<MyClass>();
```
In the case of failure, this line will return a ``NULL`` pointer, which can be
checked via ``py_type.is_valid()``. We can verify that the type lookup
succeeded, and that the returned instance indeed represents a _nanobind_-owned
type (via ``nb::type_check()``, which is redundant in this case):

```cpp
assert(py_type.is_valid() && nb::type_check(py_type));
```

_nanobind_ knows the size, alignment, and C++ RTTI ``std::type_info`` record of
all bound types. They can be queried on the fly in situations where this is useful.

```cpp
assert(nb::type_size(py_type) == sizeof(MyClass) &&
       nb::type_align(py_type) == alignof(MyClass) &&
       nb::type_info(py_type) == typeid(MyClass));
```

Given a type object representing a C++ type, we can create an uninitialized
instance via ``nb::inst_alloc()``. This is an ordinary Python object that can,
however, not (yet) be passed to bound C++ functions to prevent undefined
behavior. It must first be initialized.

```cpp
nb::object py_inst = nb::inst_alloc(py_type);
```

We can confirm that this newly created instance is managed by _nanobind_, that it
has the correct type in Python, and that it is not ``ready`` (i.e. uninitialized):

```cpp
assert(nb::inst_check(py_inst) &&
       py_inst.type().is(py_type) &&
       !nb::inst_ready(py_inst));
```

For simple *plain old data* (POD) types, the ``nb::inst_zero()`` function can be
used to zero-initialize the object and mark it as ready.

```cpp
nb::inst_zero(py_inst);
assert(nb::inst_ready(py_inst));
```

We can destruct this default instance and convert it back to non-ready status.
This memory region can then be reinitialized once more.
```cpp
nb::inst_destruct(py_inst);
assert(!nb::inst_ready(py_inst));
```

What follows is a more interesting example, where we use a lesser-known feature
of C++ (*placement new operator*) to construct an instance *in-place* into the
memory region allocated by _nanobind_.

```cpp
// Get a C++ pointer to the uninitialized instance data
MyClass *ptr = nb::inst_ptr<MyClass>(py_inst);

// Perform an in-place construction of the C++ object at address 'ptr'
new (ptr) MyClass(/* constructor arguments go here */);
```

Following this constructor call, we must inform _nanobind_ that the instance
object is now fully constructed. When its reference count reaches zero,
_nanobind_ will automatically call the in-place destructor (``MyClass::~MyClass``).

```cpp
nb::inst_mark_ready(py_inst);
assert(nb::inst_ready(py_inst));
```

Let's destroy this instance once more manually (which will, again, call the C++
destructor and mark the Python object as non-ready).
```cpp
nb::inst_destruct(py_inst);
```

Another useful feature is that _nanobind_ can copy- or move-construct ``py_inst``
from another instance of the same type. This calls the C++ copy or move
constructor and transitions ``py_inst`` back to ``ready`` status. Note that this is
equivalent to calling an in-place version of these constructors above but
compiles to more compact code (the ``nb::class_<MyClass>`` declaration had
already created bindings for both constructors, and this simply calls those
bindings).

```cpp
if (copy_instance)
    nb::inst_copy(/* dst = */ py_inst, /* src = */ some_other_instance);
else
    nb::inst_move(/* dst = */ py_inst, /* src = */ some_other_instance);
```

Note that these functions are all _unsafe_ in the sense that they do not
verify that their input arguments are valid. This is done for performance
reasons, and such checks (if needed) are therefore the responsibility of
the caller. Functions labeled `nb::type_*` should only be called with
_nanobind_ type objects, and functions labeled `nb::inst_*` should only be
called with _nanobind_ instance objects. 

The functions `nb::type_check()` and `nb::inst_check()` are exceptions to this
rule: they accept any Python object and test whether something is a _nanobind_
type or instance object.

# Even lower-level interface

Every nanobind object has two important flags that control its behavior:

1. ``ready``: is the object fully constructed? If set to ``false``, nanobind will
   raise an exception when the object is passed to a bound C++ function.

2. ``destruct``: Should nanobind call the C++ destructor when the instance
   is garbage collected?

The functions ``nb::inst_zero()``, ``nb::inst_mark_ready()``,
``nb::inst_move()``,  and ``nb::inst_copy()`` set both of these flags to
``true``, and ``nb::inst_destruct()`` sets both of them to ``false``.

In rare situations, the destructor should *not* be invoked when the instance
is garbage collected, for example when working with a nanobind instance
representing a field of a parent instance created using the
``nb::rv_policy::reference_internal`` return value policy. The library
therefore exposes two more functions that can be used to read/write these two
flags individually.

```cpp
void inst_set_state(handle h, bool ready, bool destruct);
std::pair<bool, bool> inst_state(handle h);
```

# Referencing existing instances

The above examples used the function ``nb::inst_alloc()`` to allocate
a Python object along space to hold a C++ instance associated with
the binding ``py_type``.

```cpp
nb::object py_inst = nb::inst_alloc(py_type);

// Next, perform a C++ in-place construction into the
// address given by nb::inst_ptr<MyClass>(py_inst)
... omitted, see the previous examples ...
```

What if the C++ instance already exists? Nanobind also supports this case via
the ``nb::inst_wrap()`` function—in this case, the Python object references
the existing memory region, which is potentially (slightly) less efficient
due to the need for an extra indirection.

```cpp
MyClass *inst = new MyClass();
nb::object py_inst = nb::inst_wrap(py_type, inst);

// Mark as ready, garbage-collecting 'py_inst' will
// cause 'inst' to be deleted as well
nb::inst_mark_ready(py_inst);
```
