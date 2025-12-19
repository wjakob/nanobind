#pragma once

#include <memory>

class NeverDestroy {
  public:
    static NeverDestroy& make();

    NeverDestroy(const NeverDestroy&) = delete;
    NeverDestroy& operator=(const NeverDestroy&) = delete;

    int getVar() const;
    void setVar(int i);

  private:
    NeverDestroy();

    // incomplete type error if nanobind tries to instantiate the destructor
    struct NDImpl;
    std::unique_ptr<NDImpl> impl;
};
