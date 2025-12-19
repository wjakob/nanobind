#pragma once

#include <memory>

class NeverDestruct {
  public:
    static NeverDestruct& make();

    NeverDestruct(const NeverDestruct&) = delete;
    NeverDestruct& operator=(const NeverDestruct&) = delete;

    int var() const;
    void set_var(int i);

  private:
    NeverDestruct();

    // incomplete type error if nanobind tries to instantiate the destructor
    struct NDImpl;
    std::unique_ptr<NDImpl> impl;
};
