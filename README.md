# nanobind — Seamless operability between C++17 and Python

**nanobind** is a small binding library that exposes C++ types in Python and
vice versa. It is reminiscent of
[Boost.Python](http://www.boost.org/doc/libs/1_58_0/libs/python/doc) and
[pybind11](http://github.com/pybind/pybind11) and uses near-identical syntax.

## Historical context: why _yet another_ binding library?

The author of this library started the
[pybind11](http://github.com/pybind/pybind11) project in back in 2015 to
improve the efficiency of C++ ↔ Python bindings. pybind11 is now widely used:
the repository is cloned more than 100.000 times per day, and it has become a
core dependency of software across the world including flagship projects like
PyTorch, Tensorflow, etc. Many extensions and generalizations were added by
users and core developers over the years to address use cases of this diverse
audience. The downside of this success was a growth in code complexity along
with significant compile- and runtime overheads.

Ironically, the situation feels just like 2015: bindings are once again
extremely slow to compile with existing tools (Boost.Python, pybind11), and a
new C++ standard has come along with the potential to dramatically simplify
things. I feel compelled to do this one last time..

## Talk is cheap, show me the numbers.

TBD

## Differences between `nanobind` and `pybind11`

The main difference is philosophical: _pybind11_ must deal with *all of C++* to
bind complex legacy codebases, while _nanobind_ targets a smaller C++ subset.
The codebase has to adapt to the binding tool and not the other way around.
Pull requests with extensions and generalizations were welcomed in pybind11,
while they will likely be rejected in this project.

### Removed features

Support for _multiple inheritance_ was a persistent source of complexity in
pybind11, and it is one of the main casualties in creating _nanobind_. Besides
this, the following features were removed:

- Binding of classes with overloaded `operator new` / `operator delete`.
- Module-local types or exceptions.
- Eigen and NumPy integration.
- Nested exceptions
- Pickling
- ``kw_only`` / ``pos_only`` argument annotations
- MyPy-compatible docstrings.
- The `options` class for customizing docstring generation.

### Optimizations

TBD

### Dependencies

nanobind depends on very recent versions of everything:

- C++17
  * ``if constexpr`` is crucial to simplify the internal meta-templating.
- Python 3.8+
  * nanobind heavily relies on [vector calls](https://www.python.org/dev/peps/pep-0590)
    that were introduced in version 3.8..
- CMake 3.17+
  * Recent CMake versions include important improvements to `FindPython`
    that this project depends on.

