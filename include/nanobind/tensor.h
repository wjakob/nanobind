#include "ndarray.h"

#warning "The 'tensor.h' header and 'nanobind::tensor<..>' type are deprecated. Please use 'ndarray.h' and 'nanobind::ndarray<..>'"

NAMESPACE_BEGIN(NB_NAMESPACE)

template <typename... Args> using tensor = ndarray<Args...>;

NAMESPACE_END(NB_NAMESPACE)
