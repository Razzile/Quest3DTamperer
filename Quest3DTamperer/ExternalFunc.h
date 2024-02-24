#pragma once

#include <spdlog/spdlog.h>

template <typename Func>
class ExternalFunction;

template <typename TR, typename... Args>
class ExternalFunction<TR(Args...)> {
  typedef TR(*Func)(Args...);
public:
  ExternalFunction() : _func(nullptr) {}
  ExternalFunction(uintptr_t offset) : _func(reinterpret_cast<Func>(offset)) {}

  ExternalFunction(const ExternalFunction &other) : _func(other._func) {}
  ExternalFunction(ExternalFunction &&other) noexcept : _func(other._func) {}
  ExternalFunction &operator=(const ExternalFunction &other) {
    if (this == &other)
      return *this;
    _func = other._func;
    return *this;
  }
  ExternalFunction &operator=(ExternalFunction &&other) noexcept {
    if (this == &other)
      return *this;
    _func = other._func;
    return *this;
  }

  __forceinline TR operator()(Args... args) { return _func(std::forward<Args>(args)...); }

  operator uintptr_t() { return reinterpret_cast<uintptr_t>(_func); }

private:
  Func _func;
};

#define CREATE_WRAPPER_FOR_CALLING_CONVENTION(convention)                      \
  template <typename TR, typename... Args>                                     \
  class ExternalFunction<TR(convention *)(Args...)> {                          \
    typedef TR(convention *Func)(Args...);                                     \
                                                                               \
  public:                                                                      \
    ExternalFunction() : _func(nullptr) {}                                     \
    ExternalFunction(uintptr_t offset)                                         \
        : _func(reinterpret_cast<Func>(offset)) {}                             \
                                                                               \
    ExternalFunction(const ExternalFunction &other) : _func(other._func) {}    \
    ExternalFunction(ExternalFunction &&other) noexcept                        \
        : _func(other._func) {}                                                \
    ExternalFunction &operator=(const ExternalFunction &other) {               \
      if (this == &other)                                                      \
        return *this;                                                          \
      _func = other._func;                                                     \
      return *this;                                                            \
    }                                                                          \
    ExternalFunction &operator=(ExternalFunction &&other) noexcept {           \
      if (this == &other)                                                      \
        return *this;                                                          \
      _func = other._func;                                                     \
      return *this;                                                            \
    }                                                                          \
    __forceinline TR operator()(Args... args) {                                \
      return _func(std::forward<Args>(args)...);                               \
    }                                                                          \
                                                                               \
    operator uintptr_t() { return reinterpret_cast<uintptr_t>(_func); }        \
                                                                               \
  private:                                                                     \
    Func _func;                                                                \
  };

// Create partial specializations for every calling convention on Windows
#if _MSC_VER

#if _WIN64
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__stdcall)
#else
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__stdcall)
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__fastcall)
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__cdecl)
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__thiscall)
#endif

#ifdef __cplusplus_cli
CREATE_WRAPPER_FOR_CALLING_CONVENTION(__clrcall)
#endif

CREATE_WRAPPER_FOR_CALLING_CONVENTION(__vectorcall)

#endif