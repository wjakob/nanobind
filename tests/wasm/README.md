# wasm32-emscripten (Pyodide) build smoke test

This directory is not part of the regular native test suite (`tests/`). It
cross-compiles a minimal nanobind extension for `wasm32-emscripten`, `pip
install`s the resulting wheel into a `pyodide venv` (a virtualenv whose
`python`/`pip` transparently run through a real Pyodide runtime), and runs
`test_smoke.py` with `pytest` inside it -- an ordinary Python packaging and
testing workflow, just targeting Pyodide instead of the host platform. Driven
by the `wasm32-emscripten (Pyodide)` CI job in
`.github/workflows/pyodide.yml`.

It exists to guard against a specific class of regression: nanobind headers
that use C stdio symbols (`fprintf`, `stderr`, `snprintf`, ...) without
including `<cstdio>` themselves, relying instead on an accidental transitive
include through `<Python.h>`. That transitive include is not available when
targeting Python's stable ABI (`Py_LIMITED_API`, which CPython's `Python.h`
stops pulling `<stdio.h>` in for as of Python 3.11), which is exactly the
configuration a portable Pyodide wheel needs -- so a regression here silently
passes on a normal desktop build and only breaks under Emscripten. See the PR
that introduced this test for a real-world report of this failure mode
(found while cross-compiling [onnxsim](https://github.com/onnxsim/onnxsim)
for Pyodide).

`smoke.cpp` targets `STABLE_ABI` and fails to compile if nanobind's own CMake
logic ever silently drops the stable ABI (e.g. because the Pyodide/Emscripten
Python version regresses below 3.12, the floor `nanobind_add_module`
requires for `STABLE_ABI` in linked mode) -- that would make the test pass
without exercising the code path it exists to cover.

Building this locally requires a matching Emscripten SDK and a Pyodide
cross-build environment; see the CI job for the exact commands.
