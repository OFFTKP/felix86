#pragma once

#include <type_traits>

// Source file for generic enum utilities.
// Note that these are always unstable and are not to
// be directly relied upon in projects consuming this
// library.

namespace biscuit {

// Macro that defines all of the built in operators for conveniently using
// enum classes as flag types without needing to define all of the basic
// boilerplate.
#define BISCUIT_DEFINE_ENUM_FLAG_OPERATORS(type)                     \
  [[nodiscard]]                                                      \
  constexpr type operator|(type a, type b) noexcept {                \
    using T = std::underlying_type_t<type>;                          \
    return static_cast<type>(static_cast<T>(a) | static_cast<T>(b)); \
  }                                                                  \
  [[nodiscard]]                                                      \
  constexpr type operator&(type a, type b) noexcept {                \
    using T = std::underlying_type_t<type>;                          \
    return static_cast<type>(static_cast<T>(a) & static_cast<T>(b)); \
  }                                                                  \
  [[nodiscard]]                                                      \
  constexpr type operator^(type a, type b) noexcept {                \
    using T = std::underlying_type_t<type>;                          \
    return static_cast<type>(static_cast<T>(a) ^ static_cast<T>(b)); \
  }                                                                  \
  constexpr type& operator|=(type& a, type b) noexcept {             \
    a = a | b;                                                       \
    return a;                                                        \
  }                                                                  \
  constexpr type& operator&=(type& a, type b) noexcept {             \
    a = a & b;                                                       \
    return a;                                                        \
  }                                                                  \
  constexpr type& operator^=(type& a, type b) noexcept {             \
    a = a ^ b;                                                       \
    return a;                                                        \
  }                                                                  \
  [[nodiscard]]                                                      \
  constexpr type operator~(type key) noexcept {                      \
    using T = std::underlying_type_t<type>;                          \
    return static_cast<type>(~static_cast<T>(key));                  \
  }

} // namespace biscuit
