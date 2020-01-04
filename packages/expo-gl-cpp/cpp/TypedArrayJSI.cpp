#include "TypedArrayJSI.h"

static std::unique_ptr<TypedArray> instance { nullptr };

std::unique_ptr<TypedArray>& TypedArray::get() {
  return instance;
};

static void setInstance(std::unique_ptr<TypedArray>&& implementation) {
  instance = std::move(implementation);
}
