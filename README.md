# nanobind — Seamless operability between C++17 and Python

[![Documentation](https://img.shields.io/readthedocs/nanobind/latest)](https://nanobind.readthedocs.io/en/latest/)
[![Continuous Integration](https://img.shields.io/github/actions/workflow/status/wjakob/nanobind/ci.yml?label=tests)](https://github.com/wjakob/nanobind/actions/workflows/ci.yml)
[![](https://img.shields.io/pypi/v/nanobind.svg?color=brightgreen)](https://pypi.org/pypi/nanobind/)
![](https://img.shields.io/pypi/l/nanobind.svg?color=brightgreen)
[![](https://img.shields.io/badge/Example-Link-brightgreen)](https://github.com/wjakob/nanobind_example)
[![](https://img.shields.io/badge/Changelog-Link-brightgreen)](https://nanobind.readthedocs.io/en/latest/changelog.html)

<p align="center">
    <picture>
      <source media="(prefers-color-scheme: dark)" width="800" srcset="https://rgl.s3.eu-central-1.amazonaws.com/media/uploads/wjakob/2023/03/28/nanobind_logo_dark.png">
      <source media="(prefers-color-scheme: light)" width="800" srcset="https://github.com/wjakob/nanobind/raw/master/docs/images/logo.jpg">
      <img alt="nanobind logo" width="800" src="https://github.com/wjakob/nanobind/raw/master/docs/images/logo.jpg">
    </picture>
</p>

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

Please see the following links for tutorial and reference documentation in
[HTML](https://nanobind.readthedocs.io/en/latest/) and
[PDF](https://nanobind.readthedocs.io/_/downloads/en/latest/pdf/) formats.

The nanobind logo was designed by [AndoTwin
Studio](https://andotwinstudio.com). High-resolution version are available
here: [light](https://rgl.s3.eu-central-1.amazonaws.com/media/uploads/wjakob/2023/03/27/nanobind_logo.jpg),
[dark](https://rgl.s3.eu-central-1.amazonaws.com/media/uploads/wjakob/2023/03/28/nanobind_logo_dark_1.png).
