// tests/wasm/smoke.cpp: minimal stable-ABI extension for wasm32-emscripten
// (Pyodide) CI. See tests/wasm/README.md.

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

NB_MODULE(smoke_ext, m) {
    m.def("add", [](int a, int b) { return a + b; });
    m.attr("platform_abi_tag") = NB_PLATFORM_ABI_TAG;

    // Fail loudly rather than silently testing nothing if STABLE_ABI ever
    // gets dropped (e.g. the Pyodide Python version regresses below 3.12).
#if !defined(Py_LIMITED_API)
#    error "smoke_ext must be compiled against the stable ABI (Py_LIMITED_API undefined)"
#endif
    m.attr("targeted_stable_abi") = true;
}
