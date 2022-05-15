# Removed pybind11 features

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
- ◑ NumPy integration was replaced by a more general ``nb::tensor<>``
  integration that supports CPU/GPU tensors produced by various frameworks
  (NumPy, PyTorch, TensorFlow, JAX, ..).
- ◑ Eigen integration was removed.
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
