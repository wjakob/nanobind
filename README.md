# nanobind — Seamless operability between C++17 and Python

[![Documentation](https://readthedocs.org/projects/nanobind/badge/?version=latest)](https://nanobind.readthedocs.io/en/latest/)
[![Continuous Integration](https://github.com/wjakob/nanobind/actions/workflows/ci.yml/badge.svg)](https://github.com/wjakob/nanobind/actions/workflows/ci.yml)
[![](https://img.shields.io/pypi/v/nanobind.svg)](https://pypi.org/pypi/nanobind/)
![](https://img.shields.io/pypi/l/nanobind.svg)
[![](https://img.shields.io/badge/Example-Link-green)](https://github.com/wjakob/nanobind_example)
[![](https://img.shields.io/badge/Changelog-Link-green)](https://nanobind.readthedocs.io/en/latest/changelog.html)

_nanobind_ is a small binding library that exposes C++ types in Python and vice
versa. It is reminiscent of
[Boost.Python](https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html) and
[pybind11](https://github.com/pybind/pybind11) and uses near-identical syntax.
In contrast to these existing tools, nanobind is _more efficient_: bindings
compile in a shorter amount of time, produce smaller binaries, and have better
runtime performance.

More concretely,
[benchmarks](https://nanobind.readthedocs.io/en/latest/benchmark.html) show
**~2-3× faster** compile time, **~3× smaller** binaries, and up to **~8×
lower** runtime overheads compared to pybind11.

## Documentation

nanobind's [readthedocs page](https://nanobind.readthedocs.io/en/latest/)
provides tutorials and detailed reference documentation.
