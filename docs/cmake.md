# CMake interface

_nanobind_ provides a CMake convenience function that automates the process of
building a python extension module. This resembles similar functionality in
_pybind11_. For example, the following two lines of CMake code compile
a simple extension module named ``my_ext``:

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

- When requested via the optional `STABLE_ABI` parameter, and when your
  version of Python is sufficiently recent (3.12 +), the implementation
  will build a [stable ABI](https://docs.python.org/3/c-api/stable.html)
  extension module with a different suffix (e.g., `.abi3.so`). This comes at a
  performance cost since _nanobind_ can no longer access the internals of
  various data structures directly.

- It statically or dynamically links against `libnanobind` depending on whether
  the optional `NB_STATIC` parameter is provided to `nanobind_add_module()`. 


## Tutorial project

A minimal example of a project with C++ bindings compiled using _nanobind_ and
[`scikit-build`](https://scikit-build.readthedocs.io/en/latest/) is available
in the [`nanobind_example`](https://github.com/wjakob/nanobind_example)
repository.
