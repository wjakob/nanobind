/* Shared declarations of the multi-TU split-mode test extension. The bindings
   are deliberately spread across two translation units that exchange bound
   types, which validates that the per-DSO 'nb_backend' table merges into a
   single instance. The same sources also build an LTO twin under a different
   module name. */

#pragma once

#include <nanobind/nanobind.h>

#if !defined(NB_ABI_MULTI_NAME)
#  define NB_ABI_MULTI_NAME test_abi_multi_ext
#  define NB_ABI_MULTI_NS   abi_multi
#endif

/* The twins register distinct C++ types (both modules join the same
   domain, and a typeid may only be bound once per domain) */
namespace NB_ABI_MULTI_NS {

struct Point {
    int x = 0, y = 0;
    Point() = default;
    Point(int x, int y) : x(x), y(y) { }
};

struct Box {
    Point min, max;
};

}

using namespace NB_ABI_MULTI_NS;

void bind_part2(nanobind::module_ &m);
