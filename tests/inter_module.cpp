#include "inter_module.h"

Shared create_shared() {
    return { 123 };
}

bool check_shared(const Shared &shared, int expected) {
    return shared.value == expected;
}

void increment_shared(Shared &shared) {
    ++shared.value;
}
