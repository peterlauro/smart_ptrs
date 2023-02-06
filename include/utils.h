#ifndef STDX_UTILS_H
#define STDX_UTILS_H

#include <functional>
#include <limits>
#include <type_traits>

#define requires_T(...) \
, std::enable_if_t<(__VA_ARGS__)>* = nullptr

namespace stdx
{
namespace detail
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

  // clamp_cast
  // narrowing cast which saturate the output value at min or max if the input value
  // overflow / underflow the value range of output type.
  template<typename T>
  [[nodiscard]]
  constexpr auto max_value() noexcept
  {
    // https://en.cppreference.com/w/cpp/language/types
    // https://en.cppreference.com/w/cpp/types/numeric_limits
    if constexpr (std::is_floating_point_v<T>)
    {
      // the T is floating point
      return static_cast<long double>(std::numeric_limits<T>::max());
    }
    else if constexpr (std::is_signed_v<T>)
    {
      // the T is signed integral
      return static_cast<std::intmax_t>(std::numeric_limits<T>::max());
    }
    else
    {
      // the T is unsigned integral
      return static_cast<std::uintmax_t>(std::numeric_limits<T>::max());
    }
  }

  template<class From, class To>
  [[nodiscard]]
  constexpr To clamp_cast_impl(From from)
  {
    if constexpr ((std::is_signed_v<From> && std::is_signed_v<To>) ||
      (std::is_unsigned_v<From> && std::is_unsigned_v<To>))
    {
      constexpr From low = static_cast<From>(std::numeric_limits<To>::lowest());
      constexpr From hi = static_cast<From>(std::numeric_limits<To>::max());

      if (from > hi)
      {
        return std::numeric_limits<To>::max();
      }

      if (from < low)
      {
        return std::numeric_limits<To>::lowest();
      }

      return static_cast<To>(from);
    }
    else
    {
      constexpr From low{ 0 };
      constexpr From hi = static_cast<From>(std::numeric_limits<To>::max());

      if (from > hi)
      {
        return std::numeric_limits<To>::max();
      }

      if (from < low)
      {
        return static_cast<To>(low);
      }

      return static_cast<To>(from);
    }
  }
} // end of namespace detail

  /**
   * \brief narrowing cast which saturate the output value at min or max if the input value
   *       overflow/underflow the value range of output To type.
   * \tparam To output type
   * \tparam From input type
   * \param from input value
   * \return input value after the narrowing cast to output type
   * \note To and From types can be any from standard arithmetic types
   */
  template<typename To, typename From>
  [[nodiscard]]
  constexpr To
  clamp_cast(From from) noexcept
  {
    static_assert(is_standard_arithmetic_v<To>);
    static_assert(is_standard_arithmetic_v<From>);

    using from_type = std::remove_cv_t<From>;
    using to_type = std::remove_cv_t<To>;

    constexpr auto from_max_value = detail::max_value<from_type>();
    constexpr auto to_max_value = detail::max_value<to_type>();

    using common_max_type = std::common_type_t<decltype(from_max_value), decltype(to_max_value)>;

    if constexpr (std::is_same_v<from_type, to_type>)
    {
      return from;
    }
    else if constexpr (std::is_signed_v<from_type> && std::is_unsigned_v<to_type>)
    {
      // signed -> unsigned
      if constexpr (static_cast<common_max_type>(from_max_value) > static_cast<common_max_type>(to_max_value))
      {
        return detail::clamp_cast_impl<from_type, to_type>(from);
      }
      else
      {
        if (from < static_cast<from_type>(0))
        {
          return To{ 0 };
        }
        return static_cast<To>(from);
      }
    }
    else
    {
      // unsigned -> signed
      // signed -> signed
      // unsigned -> unsigned
      if constexpr (static_cast<common_max_type>(from_max_value) > static_cast<common_max_type>(to_max_value))
      {
        return detail::clamp_cast_impl<from_type, to_type>(from);
      }
      else
      {
        return static_cast<To>(from);
      }
    }
  }
}

#endif
