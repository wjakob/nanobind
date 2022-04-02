# **Low-level instance interface**

_nanobind_ exposes a low-level interface to provide fine-grained control over
the sequence of steps that instantiates a Python object wrapping a C++
instance. Like the above point, this is useful when writing generic binding
code that manipulates _nanobind_-based objects of various types.

An example is shown below:

```cpp
/* Look up the Python type object associated with a C++ class named `MyClass`.
   Requires a previous nb::class_<> binding declaration, otherwise this line
   will return a NULL pointer (this can be checked via py_type.is_valid()). */
nb::handle py_type = nb::type<MyClass>();

// Type metadata can also be queried in the other direction
assert(py_type.is_valid() &&                             // Did the type lookup work?
       nb::type_check(py_type) &&
       nb::type_size(py_type) == sizeof(MyClass) &&      // nanobind knows the size+alignment
       nb::type_align(py_type) == alignof(MyClass) &&
       nb::type_info(py_type) == typeid(MyClass));       // Query C++ RTTI record

/* Allocate an uninitialized Python instance of this type. Nanobind will
   refuse to pass this (still unitialized) object to bound C++ functions */
nb::object py_inst = nb::inst_alloc(py_type);
assert(nb::inst_check(py_inst) && py_inst.type().is(py_type) && !nb::inst_ready(py_inst));

/* For POD types, the following line zero-initializes the object and marks
   it as ready. Alternatively, the next lines show how to perform a fancy
   object initialization using the C++ constructor */
// nb::inst_zero(py_inst);

// Get a C++ pointer to the uninitialized instance data
MyClass *ptr = nb::inst_ptr<MyClass>(py_inst);

// Perform an in-place construction of the C++ object
new (ptr) MyClass();

/* Mark the Python object as ready. When reference count reaches zero,
   nanobind will automatically call the destructor (MyClass::~MyClass). */
nb::inst_mark_ready(py_inst);
assert(nb::inst_ready(py_inst));

/* Alternatively, we can force-call the destructor and transition the
   instance back to non-ready status. The instance could then be reused
   by initializing it yet again. */
nb::inst_destruct(py_inst);
assert(!nb::inst_ready(py_inst));

/* We can copy- or move-construct 'py_inst' from another instance of the
   same type. This calls the C++ copy or move constructor and transitions
   'py_inst' back to 'ready' status. Note that this is equivalent to calling
   an in-place version of these constructors above but compiles to more
   compact code (the 'nb::class_<MyClass>' declaration has already created
   bindings for both constructors, and this simply calls those bindings). */
// nb::inst_copy(/* dst = */ py_inst, /* src = */ some_other_instance);
// nb::inst_move(/* dst = */ py_inst, /* src = */ some_other_instance);
```

Note that these functions are all _unsafe_ in the sense that they do not
verify that their input arguments are valid. This is done for performance
reasons, and such checks (if needed) are therefore the responsibility of
the caller. Functions labeled `nb::type_*` should only be called with
_nanobind_ type objects, and functions labeled `nb::inst_*` should only be
called with _nanobind_ instance objects. The functions `nb::type_check()`
and `nb::inst_check()` accept any Python object and test whether something
is a _nanobind_ type or instance object.
