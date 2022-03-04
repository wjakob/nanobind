# Details on benchmarks

The microbenchmark used to compare different binding tools wraps a _large_ number of trivial functions that
only perform a few additions. 
The objective of this is to quantify the overhead
of bindings on _compilation time_, _binary size_, and _runtime performance_.
The function-heavy benchmark (`func_*`) consists
of 720 declarations of the form (with permuted integer types)
```cpp
m.def("test_0050", [](uint16_t a, int64_t b, int32_t c, uint64_t d, uint32_t e, float f) {
    return a+b+c+d+e+f;
});
```
while the latter (`class_*`) does exactly the same computation but packaged up in `struct`s with bindings.
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
(`opt`) that minimize size (i.e., `-Os`) and run on Python 3.9.10. Compilation
is done by AppleClang using consistent flags for all three binding tools.

The code to generate the plots on the main project page is available
[here](https://github.com/wjakob/nanobind/blob/master/docs/microbenchmark.ipynb).

