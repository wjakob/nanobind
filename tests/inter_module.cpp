#include "inter_module.h"

Shared create_shared() {
    return { 123 };
}

std::shared_ptr<Shared> create_shared_sp() {
    return std::make_shared<Shared>(create_shared());
}

std::unique_ptr<Shared> create_shared_up() {
    return std::make_unique<Shared>(create_shared());
}

bool check_shared(const Shared &shared) {
    return shared.value == 123;
}

bool check_shared_sp(std::shared_ptr<Shared> shared) {
    return shared->value == 123;
}

bool check_shared_up(std::unique_ptr<Shared> shared) {
    return shared->value == 123;
}

SharedEnum create_enum() {
    return SharedEnum::Two;
}

bool check_enum(SharedEnum e) {
    return e == SharedEnum::Two;
}

void throw_shared() {
    throw create_shared();
}
