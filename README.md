# nanobind: tiny and efficient C++/Python bindings

[![Documentation](https://img.shields.io/readthedocs/nanobind/latest)](https://nanobind.readthedocs.io/en/latest/)
[![Continuous Integration](https://img.shields.io/github/actions/workflow/status/wjakob/nanobind/ci.yml?label=tests)](https://github.com/wjakob/nanobind/actions/workflows/ci.yml)
[![](https://img.shields.io/pypi/v/nanobind.svg?color=brightgreen)](https://pypi.org/pypi/nanobind/)
![](https://img.shields.io/pypi/l/nanobind.svg?color=brightgreen)
[![](https://img.shields.io/badge/Example-Link-brightgreen)](https://github.com/wjakob/nanobind_example)
[![](https://img.shields.io/badge/Changelog-Link-brightgreen)](https://nanobind.readthedocs.io/en/latest/changelog.html)

<p align="center">
    <picture>
      <source media="(prefers-color-scheme: dark)" width="800" srcset="https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2023/03/28/nanobind_logo_dark.png">
      <source media="(prefers-color-scheme: light)" width="800" srcset="https://github.com/wjakob/nanobind/raw/master/docs/images/logo.jpg">
      <img alt="nanobind logo" width="800" src="https://github.com/wjakob/nanobind/raw/master/docs/images/logo.jpg">
    </picture>
</p>

_nanobind_ is a small binding library that exposes C++ types in Python and vice
versa. It is reminiscent of
[Boost.Python](https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html) and
[pybind11](https://github.com/pybind/pybind11) and uses near-identical syntax.
In contrast to these existing tools, nanobind is more efficient: bindings
compile in a shorter amount of time, produce smaller binaries, and have better
runtime performance.

More concretely,
[benchmarks](https://nanobind.readthedocs.io/en/latest/benchmark.html) show up
to **~4× faster** compile time, **~5× smaller** binaries, and **~10× lower**
runtime overheads compared to pybind11. nanobind also outperforms Cython in
important metrics (**3-12×** binary size reduction, **1.6-4×** compilation time
reduction, similar runtime performance).

## Testimonials

A selection of testimonials from projects that migrated from pybind11 to nanobind.

<table>
<tr><td>

**IREE** · [LLVM Discourse](https://discourse.llvm.org/t/nanobind-for-mlir-python-bindings/83511/5)

> *"IREE and its derivatives switched 1.5 years ago. It has been one of the single best dep decisions I've made. Not only is it much-much faster to compile, it produces smaller binaries and has a much more lean interface to the underlying Python machinery that all adds up to significant performance improvements. Worked exactly like it said on the tin."*

— **Stella Laurenzo**, Google

</td></tr>
<tr><td>

**XLA/MLIR** · [GitHub PR](https://github.com/llvm/llvm-project/pull/118583)

> *"For a complicated Google-internal LLM model in JAX, this change improves the MLIR lowering time by around 5s (out of around 30s), which is a significant speedup for simply switching binding frameworks."*

— **Peter Hawkins**, Google

</td></tr>
<tr><td>

**Apple MLX** · [X](https://x.com/awnihannun/status/1890495434021326974)

> *"MLX uses nanobind to bind C++ to Python. It's a critical piece of MLX infra and is why running Python code is nearly the same speed as running C++ directly. Also makes it super easy to move arrays between frameworks."*

— **Awni Hannun**, Apple

</td></tr>
<tr><td>

**JAX** · [GitHub](https://github.com/jax-ml/jax/commit/70b7d501816c6e9f131a0a8b3e4a527e53eeebd7)

> *"nanobind has a number of [advantages](https://nanobind.readthedocs.io/en/latest/why.html), notably speed of compilation and dispatch, but the main reason to do this for these bindings is because nanobind can target the Python Stable ABI starting with Python 3.12. This means that we will not need to ship per-Python version CUDA plugins starting with Python 3.12."*

— **Peter Hawkins**, Google

</td></tr>
<tr><td>

**FEniCS / DOLFINx** · [GitHub](https://github.com/FEniCS/dolfinx/pull/2820)

> *"nanobind is smaller than pybind11, the wrappers build faster and it has significantly improved support for wrapping multi-dimensional arrays, which we use heavily. The nanobind docs are easier to follow on the low-level details, which makes understanding the memory management in the wrapper layer easier."*

— **Garth N. Wells**
</td></tr>
<tr><td>

**PennyLane** · [Release notes](https://docs.pennylane.ai/projects/catalyst/en/stable/dev/release_notes.html)

> *"Nanobind has been developed as a natural successor to the pybind11 library and offers a number of advantages like its ability to target Python's Stable ABI."*

</td></tr>
</table>

## Documentation

Please see the following links for tutorial and reference documentation in
[HTML](https://nanobind.readthedocs.io/en/latest/) and
[PDF](https://nanobind.readthedocs.io/_/downloads/en/latest/pdf/) formats.

## License and attribution

All material in this repository is licensed under a three-clause [BSD
license](LICENSE).

Please use the following BibTeX template to cite nanobind in scientific
discourse:

```bibtex
@misc{nanobind,
   author = {Wenzel Jakob},
   year = {2022},
   note = {https://github.com/wjakob/nanobind},
   title = {nanobind: tiny and efficient C++/Python bindings}
}
```

The nanobind logo was designed by [AndoTwin Studio](https://andotwinstudio.com)
(high-resolution download:
[light](https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2023/03/27/nanobind_logo.jpg),
[dark](https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2023/03/28/nanobind_logo_dark_1.png)).
