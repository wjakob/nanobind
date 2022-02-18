# nanobind — Seamless operability between C++17 and Python

_nanobind_ is a small binding library that exposes C++ types in Python and
vice versa. It is reminiscent of
_[Boost.Python](http://www.boost.org/doc/libs/1_58_0/libs/python/doc)_ and
_[pybind11](http://github.com/pybind/pybind11)_ and uses near-identical syntax.

## Why _yet another_ binding library?

I started the _[pybind11](http://github.com/pybind/pybind11)_ project in back in
2015 to improve the efficiency of C++/Python bindings. Thanks to many amazing
contributions by others, _pybind11_ has become a core dependency of software
across the world including flagship projects like PyTorch, Tensorflow, etc. The
repository is cloned more than 100.000 times _per day_. Many extensions and
generalizations were added by users and core developers over the years to
address use cases of this diverse audience. However, all of this success also
came with _costs_: the complexity of the library grew tremendously, causing
overheads on binary size, compilation time, and runtime performance.

Ironically, the situation feels just like 2015: bindings are once again slow to
compile with existing tools (_Boost.Python_, _pybind11_), and a new C++
standard has come along with the potential to dramatically simplify things. It
seems I need to do this one more time..

## Talk is cheap, show me the numbers.

TBD

## What are differences between _nanobind_ and _pybind11_?

The main difference is philosophical: _pybind11_ must deal with *all of C++* to
bind complex legacy codebases, while _nanobind_ targets a smaller C++ subset.
**The codebase has to adapt to the binding tool and not the other way around!**
Pull requests with extensions and generalizations were welcomed in _pybind11_,
but they will likely be rejected in this project.

### Removed features

Support for multiple inheritance was a persistent source of complexity in
_pybind11_, and it is one of the main casualties in creating _nanobind_.
Besides this, the following features were removed:

- Binding of classes with overloaded `operator new` / `operator delete`.
- Module-local types or exceptions.
- Eigen and NumPy integration.
- Nested exceptions.
- Pickling.
- ``kw_only`` / ``pos_only`` argument annotations.
- MyPy-compatible docstrings.
- The `options` class for customizing docstring generation.

Some of these may be reintroduced eventually, but it will need to be done in an
opt-in manner that does not affect binary size and compilation/runtime
performance of the base case.

### Optimizations

Besides removing features, the rewrite was an opportunity to address
long-standing performance issues with _pybind11_:

- C++ objects are co-located with the Python object whenever
  possible (less pointer chasing).
- C++ function binding information is co-located with the Python function
  object (less pointer chasing).
- C++ type binding information is co-located with the Python type object (less
  pointer chasing, fewer hashtable lookups).
- _nanobind_ internally replaces `std::unordered_map` with a more efficient hash
  table ([tsl::robin_map](https://github.com/Tessil/robin-map), which is
  included as a git submodule).
- function calls from/to Python are realized using [vector
  calls](https://www.python.org/dev/peps/pep-0590), which gives a nice speed
  boost. The main function dispatch loop no longer allocates heap memory.
- _pybind11_ was designed as a header-only library, which is generally a good
  thing because it simplifies the compilation workflow. However, one major
  downside of this is that a large amount of redundant code has to be compiled
  in each binding file (e.g., the function dispatch loop and all of the related
  internal data structures). _nanobind_ compiles a separate shared or static
  support library `libnanobind.so` and links it against the binding code to
  avoid redundant compilation. When using the CMake ``nanobind_add_module()``
  function, this all happens automatically.


### Dependencies

nanobind depends on very recent versions of everything:

- **C++17**: The ``if constexpr`` feature was crucial to simplify the internal
  meta-templating of this library.
- **Python 3.8+**: _nanobind_ heavily relies on [vector
  calls](https://www.python.org/dev/peps/pep-0590) that were introduced in
  version 3.8.
- **CMake 3.17+**: Recent CMake versions include important improvements to
  `FindPython` that this project depends on.
