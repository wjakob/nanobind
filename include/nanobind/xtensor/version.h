#pragma once

#include <xtensor/core/xtensor_config.hpp>

#ifndef NB_XTENSOR_VERSION_AT_LEAST
#define NB_XTENSOR_VERSION_AT_LEAST(major, minor, patch)                       \
    (XTENSOR_VERSION_MAJOR > (major) ||                                        \
     (XTENSOR_VERSION_MAJOR == (major) && XTENSOR_VERSION_MINOR > (minor)) ||  \
     (XTENSOR_VERSION_MAJOR == (major) && XTENSOR_VERSION_MINOR == (minor) &&  \
      XTENSOR_VERSION_PATCH >= (patch)))
#endif

static_assert(NB_XTENSOR_VERSION_AT_LEAST(0, 27, 1),
              "xtensor support in nanobind requires xtensor >= 0.27.1");
