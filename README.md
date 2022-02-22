# nanobind — Seamless operability between C++17 and Python

_nanobind_ is a small binding library that exposes C++ types in Python and vice
versa. It is reminiscent of
_[Boost.Python](https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html)_ and
_[pybind11](http://github.com/pybind/pybind11)_ and uses near-identical syntax.

## Why _yet another_ binding library?

I started the _[pybind11](http://github.com/pybind/pybind11)_ project in back
in 2015 to improve the efficiency of C++/Python bindings. Thanks to many
amazing contributions by others, _pybind11_ has become a core dependency of
software across the world including flagship projects like PyTorch and
Tensorflow. Every day, the repository is cloned more than 100.000 times.
Hundreds of contributed extensions and generalizations address use cases of
this diverse audience. However, all of this success also came with _costs_: the
complexity of the library grew tremendously, causing overheads on binary size,
compilation time, and runtime performance.

Ironically, the situation today feels like 2015 all over again: binding
generation with existing tools (_Boost.Python_, _pybind11_) is slow and
produces enormous binaries with overheads on runtime performance.
At the same time, key improvements in C++17 and Python 3.8 provide
opportunities for drastic simplifications. It seems that another round of this
cycle is needed (though the plan is that this doesn't become a _vicious cycle_,
see below..)

## Performance numbers

> **TLDR**: _nanobind_ bindings compile ~2-3× faster, producing
~3× smaller binaries, with up to ~8× lower overheads on runtime performance
(when comparing to _pybind11_ in _MinSizeRel_ mode).

The following microbenchmark binds a _large_ number of trivial functions that
only perform a few additions. The objective of this is to quantify the overhead
of bindings on _compilation time_, _binary size_, and _runtime performance_.

Two separate benchmarks analyze function-heavy (`func_`) and class-heavy
(`class_`) bindings. The former consists of 720 declarations of the form
(with permuted integer types)
```cpp
m.def("test_0050", [](uint16_t a, int64_t b, int32_t c, uint64_t d, uint32_t e, float f) {
    return a+b+c+d+e+f;
});
```
while the latter does exactly the same computation but packaged up in `struct`s with bindings.
```cpp
struct Struct50 {
    uint16_t a; int64_t b; int32_t c; uint64_t d; uint32_t e; float f;
    Struct50(uint16_t a, int64_t b, int32_t c, uint64_t d, uint32_t e, float f)
        : a(a), b(b), c(c), d(d), e(e), f(f) { }
    float sum() const { return a+b+c+d+e+f; }
};

py::class_<Struct50>(m, "Struct50")
    .def(py::init<uint16_t, int64_t, int32_t, uint64_t, uint32_t, float>())
    .def("sum", &Struct50::sum);
```
Each benchmark is compiled in debug mode (`debug`) and with optimizations
(`opt`) that minimize size (i.e., `-Os`).

The following plot shows compilation time for _Boost.Python_, _pybind11_, and _nanobind_.
The "_number_ ×" annotations denote time relative to _nanobind_, which is
consistently faster (e.g., a ~**2-3× improvement** compared to to _pybind11_).
<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/times.svg" alt="Compilation time benchmark" width="850"/>
</p>

As the next plot shows, _nanobind_ also greatly reduces the size of the
compiled bindings. There is a roughly **3× improvement** compared to _pybind11_
when compiling with optimizations.
<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/sizes.svg" alt="Binary size benchmark" width="850"/>
</p>

The last experiment compares the runtime performance overheads by by calling
one of the bound functions many times in a loop. Here, it is also interesting
to compare to a pure Python implementation that runs bytecode without binding
overheads (hatched red bar).

<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/perf.svg" alt="Runtime performance benchmark" width="850"/>
</p>

This shows that the overhead of calling a _nanobind_ function is lower than
that of an equivalent CPython function. The difference to
_pybind11_ is _significant_: a ~**2× improvement** for simple functions, and
**~8× improvement** when classes are being passed around. Complexities in
_pybind11_ related to overload resolution, multiple inheritance, and holders
are the main reasons for this difference (those features were either simplified
or completely removed in _nanobind_).

The code to generate these plots is available
[here](https://github.com/wjakob/nanobind/blob/master/docs/microbenchmark.ipynb).

## What are differences between _nanobind_ and _pybind11_?

The main difference is philosophical: _pybind11_ must deal with *all of C++* to
bind complex legacy codebases, while _nanobind_ targets a smaller C++ subset.
**The codebase has to adapt to the binding tool and not the other way around!**
Pull requests with extensions and generalizations were welcomed in _pybind11_,
but they will likely be rejected in this project.

### Removed features

Support for multiple inheritance was a persistent source of complexity in
_pybind11_, and it is one of the main casualties in creating _nanobind_.
The list below uses the following symbols:

- ⊘: This removal is a design choice. Use _pybind11_ if you need this feature.
- ⊛: This feature may be added at some point; help is welcomed.
- ⊚: Unclear, to be discussed.

The following _pybind11_ features are unavailable:

- ⊘ _nanobind_ does not instantiate a _holder_ type internally, which has
  implications on object ownership (though shared/unique pointers are still
  supported with some restrictions, see below).
- ⊘ Binding of classes with overloaded or deleted `operator new` / `operator
  delete` is unsupported.
- ⊘ The ability to run several independent Python interpreters in the same
  process is unsupported. (This would require TLS lookups for _nanobind_ data
  structures, which is undesirable.)
- ⊘ `kw_only` / `pos_only` argument annotations were removed.
- ⊘ Type signatures in docstrings are "best-effort". Getting all the details
  right for tools like MyPy is a rabbit hole that I don't wish to pursue in
  _nanobind_.
- ⊘ The `options` class for customizing docstring generation was removed.
- ⊛ Module-local type or exception declarations are currently unsupported.
- ⊛ Many STL type caster have not yet been ported.
- ⊚ PyPy support is gone. (PyPy requires many workaround in _pybind11_ that
  complicate the its internals. Making PyPy interoperate with _nanobind_ will
  likely require changes to the PyPy CPython emulation layer.)
- ⊚ Eigen and NumPy integration have been removed.
- ⊚ Nested exceptions are not supported.
- ⊚ Pickling has not been ported yet.
- ⊚ Custom metaclasses are unsupported.

Features marked with ⊛ or ⊚ may be reintroduced eventually, but this will need
to be done in an opt-in manner that does not affect binary size and
compilation/runtime performance of the base case.

### Optimizations

Besides removing features, the rewrite was an opportunity to address
long-standing performance issues:

- C++ objects are now co-located with the Python object whenever possible (less
  pointer chasing compared to _pybind11_). The per-instance overhead for
  wrapping a C++ type into a
  Python object shrinks by 2.3x. (_pybind11_: 56 bytes, _nanobind_: 24 bytes.)
- C++ function binding information is now co-located with the Python function
  object (less pointer chasing).
- C++ type binding information is now co-located with the Python type object
  (less pointer chasing, fewer hashtable lookups).
- _nanobind_ internally replaces `std::unordered_map` with a more efficient
  hash table ([tsl::robin_map](https://github.com/Tessil/robin-map), which is
  included as a git submodule).
- function calls from/to Python are realized using [PEP 590 vector
  calls](https://www.python.org/dev/peps/pep-0590), which gives a nice speed
  boost. The main function dispatch loop no longer allocates heap memory.
- _pybind11_ was designed as a header-only library, which is generally a good
  thing because it simplifies the compilation workflow. However, one major
  downside of this is that a large amount of redundant code has to be compiled
  in each binding file (e.g., the function dispatch loop and all of the related
  internal data structures). _nanobind_ compiles a separate shared or static
  support library (`libnanobind`) and links it against the binding code to
  avoid redundant compilation. When using the CMake `nanobind_add_module()`
  function, this all happens transparently.
- `#include <pybind11/pybind11.h>` pulls in a large portion of the STL (about
  2.1 MiB of headers with Clang and libc++). _nanobind_ minimizes STL usage to
  avoid this problem. Type casters even for for basic types like `std::string`
  require an explicit include directive (e.g. `#include
  <nanobind/stl/string.h>`).
- _pybind11_ is dependent on *link time optimization* (LTO) to produce
  reasonably-sized bindings, which makes linking a build time bottleneck. With
  _nanobind_'s split into a precompiled core library and minimal
  metatemplating, LTO is no longer important.
- _nanobind_ maintains efficient internal data structures for lifetime
  management (needed for `nb::keep_alive`, `nb::rv_policy::reference_internal`,
  the `std::shared_ptr` interface, etc.). With these changes, it is no longer
  necessary that bound types are weak-referenceable, which saves a pointer per
  instance.

### Other improvements

Besides performance improvements, _nanobind_ includes a quality-of-live
improvements for developers:

- When the python interpreter shuts down, _nanobind_ reports instance, type,
  and function leaks related to bindings, which is useful for tracking down
  reference counting issues.

- _nanobind_ deletes its internal data structures when the Python interpreter
  terminates, which avoids memory leak reports in tools like _valgrind_.

- In _pybind11_, function docstrings are pre-rendered while the binding code
  runs (`.def(...)`). This can create confusing signatures containing C++ types
  when the binding code of those C++ types hasn't yet run. _nanobind_ does not
  pre-render function docstrings, they are created on the fly when queried.

### Dependencies

_nanobind_ depends on very recent versions of everything:

- **C++17**: The `if constexpr` feature was crucial to simplify the internal
  meta-templating of this library.
- **Python 3.8+**: _nanobind_ heavily relies on [PEP 590 vector
  calls](https://www.python.org/dev/peps/pep-0590) that were introduced in
  version 3.8.
- **CMake 3.17+**: Recent CMake versions include important improvements to
  `FindPython` that this project depends on.

### API differences

_nanobind_ mostly follows the _pybind11_ API, hence the [pybind11
documentation](https://pybind11.readthedocs.io/en/stable) is the main source of
documentation for this project. A number of simplifications are detailed
below.

To port existing code with minimal adaptation, you can include
```cpp
#include <nanobind/pybind11.h>
```
which exposes a `pybind11` namespace with the previous naming scheme. However,
make note of the _holder_ and `NB_TRAMPOLINE` discussion below: these will
require a few adaptation on your end if your code uses holder types or
`PYBIND11_OVERRIDE_*()`.

For new projects, note the following differences:

- **Namespace**. _nanobind_ types and functions are located in the `nanobind` namespace. The
  `namespace nb = nanobind;` shorthand alias is recommended.

- **Macros**. The `PYBIND11_*` macros (e.g., `PYBIND11_OVERRIDE(..)`) were
  renamed to `NB_*` (e.g., `NB_OVERRIDE(..)`).

- **Shared pointers and holders**. _nanobind_ removes the concept of a _holder
  type_, which caused inefficiencies and introduced complexity in _pybind11_.
  This has implications on object ownership, shared ownership, and interactions
  with C++ shared/unique pointers. Please see the following [separate
  document](docs/ownership.md) for the nitty-gritty details.

  The gist is that use of shared/unique pointers requires including extra
  header files:

  - [`nanobind/stl/unique_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/unique_ptr.h)
  - [`nanobind/stl/shared_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/shared_ptr.h)

  Furthermore, ``enable_shared_from_this<T>`` should be _completely avoided_,
  and some changes to function signatures taking unique pointers may be
  required. This is consistent with the philosophy of this library: _the
  codebase has to adapt to the binding tool and not the other way around_.

  It is no longer necessary to specify holder types in the type declaration:

  _pybind11_:
  ```cpp
  py::class_<MyType, std::shared_ptr<MyType>>(m, "MyType")
    ...
  ```

  _nanobind_:
  ```cpp
  nb::class_<MyType>(m, "MyType")
    ...
  ```

- **Implicit type conversions**. In _pybind11_, implicit conversions were
  specified using a follow-up function call. In _nanobind_, they are specified
  within the constructor declarations:

  _pybind11_:
  ```cpp
  py::class_<MyType>(m, "MyType")
      .def(py::init<MyOtherType>());

  py::implicitly_convertible<MyOtherType, MyType>();
  ```

  _nanobind_:
  ```cpp
  nb::class_<MyType>(m, "MyType")
      .def(nb::init_implicit<MyOtherType>());
  ```

- **Trampoline classes.** Trampolines, i.e., polymorphic class implementations
  that forward virtual function calls to Python, now require an extra
  `NB_TRAMPOLINE(parent, size)` declaration, where `parent` refers to the
  parent class and `size` is at least as big as the number of `NB_OVERRIDE_*()`
  calls. _nanobind_ caches information to enable efficient function dispatch,
  for which it must know the number of trampoline "slots". Example:

  ```cpp
  struct PyAnimal : Animal {
      NB_TRAMPOLINE(Animal, 1);

      std::string name() const override {
          NB_OVERRIDE(std::string, Animal, name);
      }
  };
  ```

  Trampoline declarations with an insufficient size may eventually trigger a
  Python `RuntimeError` exception with a descriptive label, e.g.
  `nanobind::detail::get_trampoline('PyAnimal::what()'): the trampoline ran out
  of slots (you will need to increase the value provided to the NB_TRAMPOLINE()
  macro)!`.

- *Type casters.* The API of custom type casters has changed _significantly_.
  In a nutshell, the following changes are needed:

  - `load()` was renamed to `from_python()`. The function now takes an extra
    `uint8_t flags` (instead `bool convert`, which is now represented by the
    flag `nanobind::detail::cast_flags::convert`). A `cleanup_list *` pointer
    keeps track of Python temporaries that are created by the conversion, and
    which need to be deallocated after a function call has taken place. `flags`
    and `cleanup` should be passed to any recursive usage of
    `type_caster::from_python()`.

  - `cast()` was renamed to `from_cpp()`. The function takes a return value
    policy (as before) and a `cleanup_list *` pointer.

  Both functions must be marked as `noexcept`. In contrast to _pybind11_,
  errors during type casting are only propagated using status codes. If a
  severe error condition arises that should be reported, use Python warning API
  calls for this, e.g. `PyErr_WarnFormat()`.

  Note that the cleanup list is only available when `from_python()` or
  `from_cpp()` are called as part of function dispatch, while usage by
  `nanobind::cast()` sets `cleanup` to `nullptr`. This case should be handled
  gracefully by refusing the conversion if the cleanup list is absolutely required.

  The [std::pair type
  caster](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/pair.h)
  may be useful as a reference for these changes.

- The following types and functions were renamed:

  | _pybind11_           | _nanobind_     |
  | -------------------- | -------------- |
  | `error_already_set`  | `python_error` |
  | `reinterpret_borrow` | `borrow`       |
  | `reinterpret_steal`  | `steal`        |

