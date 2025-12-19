#include "test_classes.h"

struct NeverDestroy::NDImpl{
    int var = 0;
};

NeverDestroy::NeverDestroy() {
    impl = std::make_unique<NeverDestroy::NDImpl>();
}

int NeverDestroy::getVar() const {
    return impl->var;
}

void NeverDestroy::setVar(int i) {
    impl->var = i;
}

NeverDestroy& NeverDestroy::make() {
    static NeverDestroy nd;
    return nd;
}
