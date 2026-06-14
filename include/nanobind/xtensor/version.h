#pragma once

/// xtensor 0.26.0 changed the location of header files.
#if __has_include(<xtensor/core/xtensor_config.hpp>)
#include <xtensor/core/xtensor_config.hpp>
#else
#error "xtensor/core/xtensor_config.hpp not found, probably using xtensor < 0.26.0");
#endif

#ifndef NB_XTENSOR_VERSION_AT_LEAST
#define NB_XTENSOR_VERSION_AT_LEAST(major, minor, patch)                      \
    (XTENSOR_VERSION_MAJOR > (major) ||                                       \
     (XTENSOR_VERSION_MAJOR == (major) && XTENSOR_VERSION_MINOR > (minor)) || \
     (XTENSOR_VERSION_MAJOR == (major) && XTENSOR_VERSION_MINOR == (minor) && \
      XTENSOR_VERSION_PATCH >= (patch)))
#endif

static_assert(NB_XTENSOR_VERSION_AT_LEAST(0, 26, 0),
              "xtensor support in nanobind requires xtensor >= 0.26.0");
