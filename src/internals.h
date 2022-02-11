#include "smallvec.h"

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

struct internals {
    smallvec<void (*)(std::exception_ptr)> exception_translators;
};

extern internals &get_internals() noexcept;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

