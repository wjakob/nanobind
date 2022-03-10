# nanobind — Seamless operability between C++17 and Python

[![Continuous Integration](https://github.com/wjakob/nanobind/actions/workflows/ci.yml/badge.svg)](https://github.com/wjakob/nanobind/actions/workflows/ci.yml)
[![](https://img.shields.io/pypi/v/nanobind.svg)](https://pypi.org/pypi/nanobind/)
![](https://img.shields.io/pypi/l/nanobind.svg)
[![](https://img.shields.io/badge/Example_project-Link-green)](https://github.com/wjakob/nanobind_example)

_nanobind_ is a small binding library that exposes C++ types in Python and vice
versa. It is reminiscent of
_[Boost.Python](https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html)_
and _[pybind11](http://github.com/pybind/pybind11)_ and uses near-identical
syntax. In contrast to these existing tools, _nanobind_ is more _efficient_:
bindings compile in a shorter amount of time, producing smaller binaries with
better runtime performance.

## Why _yet another_ binding library?

I started the _[pybind11](http://github.com/pybind/pybind11)_ project back in
2015 to generate better and more efficient C++/Python bindings. Thanks to many
amazing contributions by others, _pybind11_ has become a core dependency of
software across the world including flagship projects like PyTorch and
Tensorflow. Every day, the repository is cloned more than 100.000 times.
Hundreds of contributed extensions and generalizations address use cases of
this diverse audience. However, all of this success also came with _costs_: the
complexity of the library grew tremendously, which had a negative impact on
efficiency.

Ironically, the situation today feels like 2015 all over again: binding
generation with existing tools (_Boost.Python_, _pybind11_) is slow and
produces enormous binaries with overheads on runtime performance. At the same
time, key improvements in C++17 and Python 3.8 provide opportunities for
drastic simplifications. Therefore, I am starting _another_ binding project..
This time, the scope is intentionally limited so that this doesn't turn into an
endless cycle.

## Performance

> **TLDR**: _nanobind_ bindings compile ~2-3× faster, producing
~3× smaller binaries, with up to ~8× lower overheads on runtime performance
(when comparing to _pybind11_ with `-Os` size optimizations).

The following experiments analyze the performance of a very large
function-heavy (`func`) and class-heavy (`class`) binding microbenchmark
compiled using _Boost.Python_, _pybind11_, and _nanobind_ in both `debug` and
size-optimized (`opt`) modes. 
A comparison with [cppyy](https://cppyy.readthedocs.io/en/latest/) (which uses
dynamic compilation) is also shown later.
Details on the experimental setup can be found
[here](https://github.com/wjakob/nanobind/blob/master/docs/benchmark.md). 

The first plot contrasts the **compilation time**, where "_number_ ×"
annotations denote the amount of time spent relative to _nanobind_. As shown
below, _nanobind_ achieves a consistent ~**2-3× improvement** compared to
_pybind11_.
<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/times.svg" alt="Compilation time benchmark" width="850"/>
</p>

_nanobind_ also greatly reduces the **binary size** of the compiled bindings.
There is a roughly **3× improvement** compared to _pybind11_ and a **8-9×
improvement** compared to _Boost.Python_ (both with size optimizations).
<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/sizes.svg" alt="Binary size benchmark" width="850"/>
</p>

The last experiment compares the **runtime performance overheads** by calling
one of the bound functions many times in a loop. Here, it is also interesting
to compare against [cppyy](https://cppyy.readthedocs.io/en/latest/) (gray bar)
and a pure Python implementation that runs bytecode without binding overheads
(hatched red bar).

<p align="center">
<img src="https://github.com/wjakob/nanobind/raw/master/docs/images/perf.svg" alt="Runtime performance benchmark" width="850"/>
</p>

This data shows that the overhead of calling a _nanobind_ function is lower
than that of an equivalent function call done within CPython. The functions
benchmarked here don't perform CPU-intensive work, so this this mainly measures
the overheads of performing a function call, boxing/unboxing arguments and
return values, etc.

The difference to _pybind11_ is _significant_: a ~**2× improvement** for simple
functions, and an **~8× improvement** when classes are being passed around.
Complexities in _pybind11_ related to overload resolution, multiple
inheritance, and holders are the main reasons for this difference. Those
features were either simplified or completely removed in _nanobind_.

Finally, there is a **~1.4× improvement** in both experiments compared
to _cppyy_ (please ignore the two `[debug]` columns—I did not feel
comfortable adjusting the JIT compilation flags; all _cppyy_ bindings
are therefore optimized.)

## What are technical differences between _nanobind_ and _cppyy_?

_cppyy_ is based on dynamic parsing of C++ code and *just-in-time* (JIT)
compilation of bindings via the LLVM compiler infrastructure. The authors of
_cppyy_ report that their tool produces bindings with much lower overheads
compared to _pybind11_, and the above plots show that this is indeed true.
However, _nanobind_ retakes the performance lead in these experiments. 

With speed gone as the main differentiating factor, other qualitative
differences make these two tools appropriate to different audiences: _cppyy_
has its origin in CERN's ROOT mega-project and must be highly dynamic to work
with that codebase: it can parse header files to generate bindings as needed.
_cppyy_ works particularly well together with PyPy and can avoid
boxing/unboxing overheads with this combination. The main downside of _cppyy_
is that it depends on big and complex machinery (Cling/Clang/LLVM) that must
be deployed on the user's side and then run there. There isn't a way of
pre-generating bindings and then shipping just the output of this process.

_nanobind_ is relatively static in comparison: you must tell it which functions
to expose via binding declarations. These declarations offer a high degree of
flexibility that users will typically use to create bindings that feel
_pythonic_. At compile-time, those declarations turn into a sequence of CPython
API calls, which produces self-contained bindings that are easy to redistribute
via [PyPI](https://pypi.org) or elsewhere. Tools like
[cibuildwheel](https://cibuildwheel.readthedocs.io/en/stable/) and
[scikit-build](https://scikit-build.readthedocs.io/en/latest/index.html) can
fully automate the process of generating _Python wheels_ for each target
platform. A minimal [example
project](https://github.com/wjakob/nanobind_example) shows how to do this
automatically via [GitHub Actions](https://github.com/features/actions).

## What are technical differences between _nanobind_ and _pybind11_?

_nanobind_ and _pybind11_ are the most similar of all of the binding tools
compared above.

The main difference is between them is largely philosophical: _pybind11_ must
deal with *all of C++* to bind complex legacy codebases, while _nanobind_
targets a smaller C++ subset. **The codebase has to adapt to the binding tool
and not the other way around!** This change of perspective allows _nanobind_ to
be simpler and faster. Pull requests with extensions and generalizations were
welcomed in _pybind11_, but they will likely be rejected in this project.

### Removed features

A number of _pybind11_ features are unavailable in _nanobind_. The list below
uses the following symbols:

- ○: This removal is a design choice. Use _pybind11_ if you need this feature.
- ●: This feature will likely be added at some point; your help is welcomed.
- ◑: Unclear, to be discussed.

Removed features include:

- ○ Multiple inheritance was a persistent source of complexity in _pybind11_,
  and it is one of the main casualties in creating _nanobind_. C++ classes
  involving multiple inheritance cannot be mapped onto an equivalent Python
  class hierarchy.
- ○ _nanobind_ instances co-locate instance data with a Python object instead
  of accessing it via a _holder_ type. This is a major difference compared to
  _pybind11_, which has implications on object ownership. Shared/unique
  pointers are still supported with some restrictions, see below for details.
- ○ Binding does not support C++ classes with overloaded or deleted `operator
  new` / `operator delete`.
- ○ The ability to run several independent Python interpreters in the same
  process is unsupported. (This would require TLS lookups for _nanobind_ data
  structures, which is undesirable.)
- ○ `kw_only` / `pos_only` argument annotations were removed.
- ○ The `options` class for customizing docstring generation was removed.
- ○ Workarounds for old/buggy/non-standard-compliant compilers are gone and
  will not be reintroduced.
- ○ Module-local types and exceptions are unsupported.
- ○ Custom metaclasses are unsupported.
- ● Many STL type caster have not yet been ported.
- ● PyPy support is gone. (PyPy requires many workaround in _pybind11_ that
  complicate the its internals. Making PyPy interoperate with _nanobind_ will
  likely require changes to the PyPy CPython emulation layer.)
- ◑ Eigen and NumPy integration have been removed.
- ◑ Buffer protocol functionality was removed.
- ◑ Nested exceptions are not supported.
- ◑ Features to facilitate pickling and unpickling were removed.
- ◑ Support for embedding the interpreter and evaluating Python code
    strings was removed.

### Removed features

Bullet points marked with ● or ◑ may be reintroduced eventually, but this will
need to be done in a careful opt-in manner that does not affect code
complexity, binary size, and compilation/runtime performance of basic bindings
that don't depend on these features.

### Optimizations

Besides removing features, the rewrite was an opportunity to address
long-standing performance issues in _pybind11_:

- C++ objects are now co-located with the Python object whenever possible (less
  pointer chasing compared to _pybind11_). The per-instance overhead for
  wrapping a C++ type into a Python object shrinks by 2.3x. (_pybind11_: 56
  bytes, _nanobind_: 24 bytes.)
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
  require an explicit opt-in by including an extra header file (e.g. `#include
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

_nanobind_ depends on recent versions of everything:

- **C++17**: The `if constexpr` feature was crucial to simplify the internal
  meta-templating of this library.
- **Python 3.8+**: _nanobind_ heavily relies on [PEP 590 vector
  calls](https://www.python.org/dev/peps/pep-0590) that were introduced in
  version 3.8.
- **CMake 3.17+**: Recent CMake versions include important improvements to
  `FindPython` that this project depends on.
- **Supported compilers**: Clang 7, GCC 8, MSVC2019 (or newer) are officially
  supported.

  Other compilers like MinGW, Intel (icpc, oneAPI), NVIDIA (PGI, nvcc) may or
  may not work but aren't officially supported. Pull requests to work around
  bugs in these compilers will not be accepted, as similar changes introduced
  significant complexity in _pybind11_. Instead, please file bugs with the
  vendors so that they will fix their compilers.

### CMake interface

Note: for your convenience, a minimal example of a project with C++ bindings
compiled using _nanobind_ and
[`scikit-build`](https://scikit-build.readthedocs.io/en/latest/) is available
in the [`nanobind_example`](https://github.com/wjakob/nanobind_example)
repository. To set up a build system manually, read on:

_nanobind_ provides a CMake convenience function that automates the process of
building a python extension module. This works analogously to _pybind11_.
Example:

```cmake
add_subdirectory(.. path to nanobind directory ..)
nanobind_add_module(my_ext common.h source_1.cpp source_2.cpp)
```

The defaults chosen by this function are somewhat opinionated. In particular,
it performs the following steps to produce efficient bindings.

- In non-debug modes, it compiles with _size optimizations_ (i.e., `-Os`). This
  is generally the mode that you will want to use for C++/Python bindings.
  Switching to `-O3` would enable further optimizations like vectorization,
  loop unrolling, etc., but these all increase compilation time and binary size
  with no real benefit for bindings.

  If your project contains portions that benefit from `-O3`-level
  optimizations, then it's better to run two separate compilation steps.
  An example is shown below:

  ```cmake
  # Compile project code with current optimization mode configured in CMake
  add_library(example_lib STATIC source_1.cpp source_2.cpp)
  # Need position independent code (-fPIC) to link into 'example_ext' below
  set_target_properties(example_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

  # Compile extension module with size optimization and add 'example_lib'
  nanobind_add_module(example_ext common.h source_1.cpp source_2.cpp)
  target_link_libraries(example_ext PRIVATE example_lib)
  ```

  Size optimizations can be disabled by specifying the optional `NOMINSIZE`
  argument, though doing so is not recommended.

- `nanobind_add_module()` also disables stack-smashing protections (i.e., it
  specifies `-fno-stack-protector` to Clang/GCC). Protecting against such
  vulnerabilities in a Python VM seems futile, and it adds non-negligible extra
  cost (+8% binary size in my benchmarks). This behavior can be disabled by
  specifying the optional `PROTECT_STACK` flag. Either way, is not recommended
  that you use _nanobind_ in a setting where it presents an attack surface.

- In non-debug compilation modes, it strips internal symbol names from the
  resulting binary, which leads to a substantial size reduction. This behavior
  can be disabled using the optional `NOSTRIP` argument.

- Link-time optimization (LTO) is _not active_ by default; benefits compared to
  _pybind11_ are relatively low, and this tends to make linking a build
  bottleneck. That said, the optional `LTO` argument can be specified to enable
  LTO in non-debug modes.

- The function also sets the target to C++17 mode (it's fine to manually
  increase this later on, e.g., to C++20)

- It appends the library suffix (e.g., `.cpython-39-darwin.so`) based on
  information provided by CMake's `FindPython` module.

- It statically or dynamically links against `libnanobind` depending on the
  value of the `NB_SHARED` parameter of the CMake project. Note that
  `NB_SHARED` is not an input of the `nanobind_add_module()` function. Rather,
  it should be specified before including the `nanobind` CMake project:

  ```cmake

  set(NB_SHARED OFF CACHE INTERNAL "") # Request static compilation of libnanobind
  add_subdirectory(.. path to nanobind directory ..)
  nanobind_add_module(...)
  ```

### API differences

_nanobind_ mostly follows the _pybind11_ API, hence the [pybind11
documentation](https://pybind11.readthedocs.io/en/stable) is the main source of
documentation for this project. A number of simplifications and noteworthy
changes are detailed below.

- **Namespace**. _nanobind_ types and functions are located in the `nanobind` namespace. The
  `namespace nb = nanobind;` shorthand alias is recommended.

- **Macros**. The `PYBIND11_*` macros (e.g., `PYBIND11_OVERRIDE(..)`) were
  renamed to `NB_*` (e.g., `NB_OVERRIDE(..)`).

- **Shared pointers and holders**. _nanobind_ removes the concept of a _holder
  type_, which caused inefficiencies and introduced complexity in _pybind11_.
  This has implications on object ownership, shared ownership, and interactions
  with C++ shared/unique pointers. Please see the following [separate
  document](docs/ownership.md) for the nitty-gritty details.

  The gist is that use of shared/unique pointers requires one or both of the
  following optional header files:

  - [`nanobind/stl/unique_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/unique_ptr.h)
  - [`nanobind/stl/shared_ptr.h`](https://github.com/wjakob/nanobind/blob/master/include/nanobind/stl/shared_ptr.h)

  Binding functions that take ``std::unique_ptr<T>`` arguments involves some
  limitations that can be avoided by changing their signatures to use
  ``std::unique_ptr<T, nb::deleter<T>>`` instead. Usage of
  ``std::enable_shared_from_this<T>`` is prohibited and will raise a
  compile-time assertion. This is consistent with the philosophy of this
  library: _the codebase has to adapt to the binding tool and not the other way
  around_.

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

- **Type casters.** The API of custom type casters has changed _significantly_.
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

  | _pybind11_           | _nanobind_      |
  | -------------------- | --------------- |
  | `error_already_set`  | `python_error`  |
  | `reinterpret_borrow` | `borrow`        |
  | `reinterpret_steal`  | `steal`         |
  | `custom_type_setup`  | `type_callback` |

- **New features.**

  - _nanobind_ can store supplemental data along with registered types. This
    information is co-located with the Python type object. An example use of
    this fairly advanced feature is in libraries that register large numbers of
    different types (e.g. flavors of tensors). Generically implemented
    functions can use the supplement to handle each type slightly differently.

    ```cpp
    struct Supplement {
        ...
    };

    // Register a new type Test, and reserve space for sizeof(Supplement)
    nb::class_<Test> cls(m, "Test", nb::supplement<Supplement>())

    /// Mutable reference to 'Supplement' portion in Python type object
    Supplement &supplement = nb::type_supplement<Supplement>(cls);
    ```

  - The function `nb::type<Class>()` can be used to look up the Python
    type object associated with a bound C++ type named `Class`.

  - The `nb::ready()` returns `true` if the GIL is held and the Python
    interpreter is not currently being finalized. Use this function to to test
    if it is safe to issue Python API calls.

## How to cite this project?

Please use the following BibTeX template to cite nanobind in scientific
discourse:

```bibtex
@misc{nanobind,
   author = {Wenzel Jakob},
   year = {2022},
   note = {https://github.com/wjakob/nanobind},
   title = {nanobind -- Seamless operability between C++17 and Python}
}
```
