#pragma once

#include <jsi/jsi.h>

namespace jsi = facebook::jsi;

class TypedArray {
public:
  enum Type {
    Int8Array,
    Int16Array,
    Int32Array,
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    ArrayBuffer,
    None,
  };

private:
  // associate type of an array with type of a content
  template<enum Type> struct content;

  template<> struct content<Type::Int8Array> { typedef int8_t type; };
  template<> struct content<Type::Int16Array> { typedef int16_t type; };
  template<> struct content<Type::Int32Array> { typedef int32_t type; };
  template<> struct content<Type::Uint8Array> { typedef uint8_t type; };
  template<> struct content<Type::Uint8ClampedArray> { typedef uint8_t type; };
  template<> struct content<Type::Uint16Array> { typedef uint16_t type; };
  template<> struct content<Type::Uint32Array> { typedef uint32_t type; };
  template<> struct content<Type::Float32Array> { typedef float type; };
  template<> struct content<Type::Float64Array> { typedef double type; };
  template<> struct content<Type::ArrayBuffer> { typedef uint8_t type; };
  template<> struct content<Type::None> { typedef uint8_t type; };

public:
  template<enum Type T>
  using ContentType = typename content<T>::type;

  static std::unique_ptr<TypedArray>& get();

  template <Type T>
  jsi::Value create(jsi::Runtime& runtime, std::vector<ContentType<T>> data);

  void updateWithData(jsi::Runtime& runtime, const jsi::Value& val, std::vector<uint8_t> data);

  template <Type T>
  std::vector<ContentType<T>> fromJSValue(jsi::Runtime& runtime, const jsi::Value& val);

  std::vector<uint8_t> rawFromJSValue(jsi::Runtime& runtime, const jsi::Value& val);

  Type typeFromJSValue(jsi::Runtime& runtime, const jsi::Value& val);

private:
  static void setInstance(std::unique_ptr<TypedArray>&&);
};

void prepareJSCTypedArrayApi(jsi::Runtime& runtime);
void prepareHermesTypedArrayApi(jsi::Runtime& runtime);
