/* This extension is built against a backend module name that does not
   exist; importing it must fail with an instructive ImportError. */

#include <nanobind/nanobind.h>

NB_MODULE(test_abi_missing_ext, m) {
    m.attr("loaded") = true;
}
