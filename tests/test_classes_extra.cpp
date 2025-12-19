#include "test_classes.h"

struct NeverDestruct::NDImpl{
    int var = 0;
};

NeverDestruct::NeverDestruct() {
    impl = std::make_unique<NeverDestruct::NDImpl>();
}

int NeverDestruct::var() const {
    return impl->var;
}

void NeverDestruct::set_var(int i) {
    impl->var = i;
}

NeverDestruct& NeverDestruct::make() {
    static NeverDestruct nd;
    return nd;
}
