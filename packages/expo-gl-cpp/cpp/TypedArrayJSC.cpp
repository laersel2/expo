#include <stdint.h>
#include <stdlib.h>

#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSContextRef.h>

#include "TypedArrayJSI.h"

// CASE I
//
// Mock implementation for cases when JSC does not provide
// JSTypedArray.h (iOS < 10 or android with react-native < 0.59)
// and typed array hack is disabled
//
// TODO switch to __has_include
#include "TypedArrayJSCMock.h"

// CASE II (not supported)
//
// Inefficient implementation used when JSC does not provide
// JSTypedArray.h (iOS < 10 or android with react-native < 0.59)
// and typed array hack is enabled
//
#include "TypedArrayJSCHack.h"


// CASE III
// Use TypedArray api provided by JSC
//
#if defined(__APPLE__) || defined(EXJS_USE_JSC_TYPEDARRAY_HEADER)
#include <JavaScriptCore/JSTypedArray.h>
#endif


class JSCTypedArray: public TypedArray {
private:
  template<enum TypedArray::Type> struct jscTypeMap;

  template<> struct jscTypeMap<Type::Int8Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeInt8Array; };
  template<> struct jscTypeMap<Type::Int16Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeInt16Array; };
  template<> struct jscTypeMap<Type::Int32Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeInt32Array; };
  template<> struct jscTypeMap<Type::Uint8Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeUint8Array; };
  template<> struct jscTypeMap<Type::Uint8ClampedArray> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeUint8ClampedArray; };
  template<> struct jscTypeMap<Type::Uint16Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeUint16Array; };
  template<> struct jscTypeMap<Type::Uint32Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeUint32Array; };
  template<> struct jscTypeMap<Type::Float32Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeFloat32Array; };
  template<> struct jscTypeMap<Type::Float64Array> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeFloat64Array; };
  template<> struct jscTypeMap<Type::ArrayBuffer> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeArrayBuffer; };
  template<> struct jscTypeMap<Type::None> { static constexpr JSTypedArrayType type = kJSTypedArrayTypeNone; };

  template<enum TypedArray::Type T>
  typename jscTypeMap<T>::type jscArrayType() { return jscTypeMap<T>::type; }

public:

  template <Type T>
  jsi::Value create(jsi::Runtime& runtime, std::vector<ContentType<T>> data);

  template <Type T>
  std::vector<ContentType<T>> fromJSValue(jsi::Runtime& runtime, const jsi::Value& jsVal);

  std::vector<uint8_t> rawFromJSValue(jsi::Runtime& runtime, const jsi::Value& val);

private:
  friend TypedArray;
  friend jsi::Runtime;

  bool usingTypedArrayHack = false; // TODO

  // fake class to extract jsc specific values from jsi::Runtime
  // partialy copied from JSCRuntime.cpp
  class JSCRuntime : public jsi::Runtime{
  public:
    JSGlobalContextRef ctx;
    std::atomic<bool> ctxInvalid;

    class JSCObjectValue final : public PointerValue {
    public:
      JSCObjectValue(JSGlobalContextRef ctx, const std::atomic<bool>& ctxInvalid, JSObjectRef obj);

      void invalidate() override;

      JSGlobalContextRef ctx_;
      const std::atomic<bool>& ctxInvalid_;
      JSObjectRef obj_;
    };

    virtual JSGlobalContextRef getCtx() {
        return ctx;
    }

    static jsi::Value toJSI(JSCRuntime* jsc, JSValueRef value) {
      JSObjectRef objRef = JSValueToObject(jsc->ctx, value, nullptr);
      if (!objRef) {
        objRef = JSObjectMake(jsc->ctx, nullptr, nullptr);
      }
      return jsi::Runtime::make<jsi::Object>(new JSCRuntime::JSCObjectValue(jsc->ctx, jsc->ctxInvalid, objRef));
    }

    static JSValueRef toJSC(jsi::Runtime& runtime, const jsi::Value& value) {
      return static_cast<const JSCRuntime::JSCObjectValue*>(
        jsi::Runtime::getPointerValue(value.asObject(runtime)))->obj_;
    }
  };

  JSCRuntime* getCtxRef(jsi::Runtime& runtime) {
    return reinterpret_cast<JSCRuntime*>(&runtime);
  }
};

template <JSCTypedArray::Type T>
jsi::Value JSCTypedArray::create(jsi::Runtime& runtime, std::vector<ContentType<T>> data) {
  auto jsc = getCtxRef(runtime);
  int byteLength = data.size * sizeof(ContentType<T>);
  JSTypedArrayType arrayType = jscArrayType<T>();
  if (data) {
    if (usingTypedArrayHack) {
      return JSObjectMakeTypedArrayWithData(jsc->ctx, arrayType, data.data(), byteLength);
    } else {
      void *dataMalloc = new char[byteLength];
      memcpy(dataMalloc, data, byteLength);
      return JSCRuntime::toJSI(jsc, JSObjectMakeTypedArrayWithBytesNoCopy(
              jsc->ctx,
              arrayType,
              dataMalloc,
              byteLength,
              [](void *data, void *ctx) { delete[] reinterpret_cast<char*>(data); },
              nullptr,
              nullptr));
    }
  } else {
    if (usingTypedArrayHack) {
      return JSCRuntime::toJSI(jsc, JSObjectMakeTypedArrayWithHack(jsc->ctx, arrayType, 0));
    } else {
      return JSCRuntime::toJSI(jsc, JSObjectMakeTypedArray(jsc->ctx, arrayType, 0, nullptr));
    }
  }
}

template <JSCTypedArray::Type T>
std::vector<JSCTypedArray::ContentType<T>> JSCTypedArray::fromJSValue(jsi::Runtime& runtime, const jsi::Value& jsVal) {
  auto jsc = getCtxRef(runtime);
  if (usingTypedArrayHack) {
    size_t byteLength;
    void* data = JSObjectGetTypedArrayDataMalloc(jsc->ctx, (JSObjectRef) JSCRuntime::toJSC(runtime, jsVal), &byteLength);
    auto start = static_cast<ContentType<T>*>(data);
    auto end = static_cast<ContentType<T>*>(reinterpret_cast<uint8_t*>(data) + byteLength);
    std::vector<ContentType<T>> result(start, end);
    free(data);
    return result;
  } else {
    void *data = nullptr;
    size_t byteLength = 0;
    size_t byteOffset = 0;

    JSObjectRef jsObject = (JSObjectRef) JSCRuntime::toJSC(runtime, jsVal);
    JSTypedArrayType type = JSValueGetTypedArrayType(jsc->ctx, JSCRuntime::toJSC(runtime, jsVal), nullptr);
    if (type == kJSTypedArrayTypeArrayBuffer) {
      byteLength = JSObjectGetArrayBufferByteLength(jsc->ctx, jsObject, nullptr);
      data = JSObjectGetArrayBufferBytesPtr(jsc->ctx, jsObject, nullptr);
      byteOffset = 0; // todo: no equivalent function for array buffer?
    } else if (type != kJSTypedArrayTypeNone) {
      byteLength = JSObjectGetTypedArrayByteLength(jsc->ctx, jsObject, nullptr);
      data = JSObjectGetTypedArrayBytesPtr(jsc->ctx, jsObject, nullptr);
      byteOffset = JSObjectGetTypedArrayByteOffsetHack(jsc->ctx, jsObject);
    }

    if (!data) {
      return std::vector<ContentType<T>>();
    }
    auto start = reinterpret_cast<ContentType<T>*>(reinterpret_cast<uint8_t*>(data) + byteOffset);
    auto end = reinterpret_cast<ContentType<T>*>(reinterpret_cast<uint8_t*>(data) + byteOffset + byteLength);
    return std::vector<ContentType<T>>(start, end);
  }
}

std::vector<uint8_t> JSCTypedArray::rawFromJSValue(jsi::Runtime& runtime, const jsi::Value& val) {
  return fromJSValue<TypedArray::Type::Uint8Array>(runtime, val);
}
