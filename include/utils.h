#ifndef STDX_UTILS_H
#define STDX_UTILS_H

#include <functional>
#include <type_traits>

#define requires_T(...) \
, typename std::enable_if_t<(__VA_ARGS__)>* = nullptr

namespace stdx::detail
{
  template<typename K, bool Enabled>
  struct conditionally_enabled_hash
  {
    [[nodiscard]]
    size_t operator()(const K& key) const
    noexcept(noexcept(std::hash<K>::do_hash(key)))
    {
      return std::hash<K>::do_hash(key);
    }
  };

  template<typename K>
  struct conditionally_enabled_hash<K, false>
  {
    conditionally_enabled_hash() = delete;
    conditionally_enabled_hash(const conditionally_enabled_hash&) = delete;
    conditionally_enabled_hash(conditionally_enabled_hash&&) = delete;
    conditionally_enabled_hash& operator=(const conditionally_enabled_hash&) = delete;
    conditionally_enabled_hash& operator=(conditionally_enabled_hash&&) = delete;
  };

  template<typename K, typename = void>
  struct is_nothrow_hashable : std::false_type
  {
  };

  template<class K>
  struct is_nothrow_hashable<K, std::void_t<decltype(std::hash<K>{}(std::declval<const K&>()))>>
    : std::bool_constant<noexcept(std::hash<K>{}(std::declval<const K&>()))>
  {
  };

  template<typename K>
  inline constexpr auto is_nothrow_hashable_v = is_nothrow_hashable<K>::value;
}

#endif
