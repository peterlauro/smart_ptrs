#ifndef STDX_CONCEPTS_H
#define STDX_CONCEPTS_H

#include "type_traits.h"

namespace stdx
{
  namespace detail
  {
    template<typename T>
    struct as_cref_
    {
      using type = T const &;
    };
    template<typename T>
    struct as_cref_<T &>
    {
      using type = T const &;
    };
    template<typename T>
    struct as_cref_<T &&>
    {
      using type = T const &;
    };
    template<>
    struct as_cref_<void>
    {
      using type = void;
    };
    template<>
    struct as_cref_<void const>
    {
      using type = void const;
    };

    template<typename T>
    using as_cref_t = typename as_cref_<T>::type;

    //
    //  CXX20
    //

    // https://en.cppreference.com/w/cpp/types/common_reference
    // https://ericniebler.github.io/std/wg21/D0022.html
    // very very simplified implementation - just for comparable check on const lvalue references
    template<typename T, typename U, typename = void>
    struct common_reference2
    {
    };

    template<typename T, typename U>
    struct common_reference2<T, U,
      std::void_t<
        std::enable_if_t<std::is_reference_v<T> && std::is_reference_v<U>>,
        std::common_type_t<remove_cvref_t<T>, remove_cvref_t<U>>
      >
    >
    {
      using type = as_cref_t<std::common_type_t<remove_cvref_t<T>, remove_cvref_t<U>>>;
    };
  }

  template<typename... Ts>
  struct common_reference
  {
  };

  template<typename T>
  struct common_reference<T>
  {
    using type = T;
  };

  template<typename T, typename U>
  struct common_reference<T, U> : detail::common_reference2<T, U>
  {
  };

  template<typename T, typename U>
  using common_reference_t = typename common_reference<T, U>::type;

  // https://en.cppreference.com/w/cpp/concepts/same_as
  template<typename T, typename U>
  using SameAs_t = std::enable_if_t<std::is_same_v<T, U> && std::is_same_v<U, T>>;

  template<typename T, typename U>
  using SameAs = is_detected<SameAs_t, T, U>;

  template<typename T, typename U>
  inline constexpr bool SameAs_v = SameAs<T, U>::value;

  // https://en.cppreference.com/w/cpp/concepts/convertible_to
  template<typename From, typename To>
  using ExplicitlyConvertibleTo_t = decltype(static_cast<To>(std::declval<From>()));

  template<typename From, typename To>
  using ConvertibleTo_t = std::enable_if_t<
    std::conjunction_v<
      std::is_convertible<From, To>, //implicitly convertible
      is_detected<ExplicitlyConvertibleTo_t, From, To>
    >
  >;

  template<typename From, typename To>
  using ConvertibleTo = is_detected<ConvertibleTo_t, From, To>;

  template<typename From, typename To>
  inline constexpr bool ConvertibleTo_v = ConvertibleTo<From, To>::value;

  // https://en.cppreference.com/w/cpp/concepts/common_reference_with
  template<typename T, typename U>
  using CommonReferenceWith_t = std::enable_if_t<
    std::conjunction_v<
      SameAs<common_reference_t<T, U>, common_reference_t<U, T>>,
      ConvertibleTo<T, common_reference_t<T, U>>,
      ConvertibleTo<U, common_reference_t<T, U>>
    >
  >;

  template<typename T, typename U>
  using CommonReferenceWith = is_detected<CommonReferenceWith_t, T, U>;

  template<typename T, typename U>
  inline constexpr bool CommonReferenceWith_v = CommonReferenceWith<T, U>::value;

  // https://en.cppreference.com/w/cpp/concepts/equality_comparable
  template<typename T, typename U>
  using WeaklyEqualityComparableWith_t = std::enable_if_t<
    std::conjunction_v<
      std::is_convertible<decltype(std::declval<detail::as_cref_t<T>>() == std::declval<detail::as_cref_t<U>>()), bool>,
      std::is_convertible<decltype(std::declval<detail::as_cref_t<T>>() != std::declval<detail::as_cref_t<U>>()), bool>,
      std::is_convertible<decltype(std::declval<detail::as_cref_t<U>>() == std::declval<detail::as_cref_t<T>>()), bool>,
      std::is_convertible<decltype(std::declval<detail::as_cref_t<U>>() != std::declval<detail::as_cref_t<T>>()), bool>
    >
  >;

  template<typename T, typename U>
  using WeaklyEqualityComparableWith = is_detected<WeaklyEqualityComparableWith_t, T, U>;

  template<typename T, typename U>
  inline constexpr bool WeaklyEqualityComparableWith_v = WeaklyEqualityComparableWith<T, U>::value;

  template<typename T>
  using EqualityComparable = WeaklyEqualityComparableWith<T, T>;

  template<typename T>
  inline constexpr bool EqualityComparable_v = EqualityComparable<T>::value;

  template<typename T, typename U>
  using EqualityComparableWith_t = std::enable_if_t<
    std::conjunction_v<
      EqualityComparable<T>,
      EqualityComparable<U>,
      WeaklyEqualityComparableWith<T, U>,
      CommonReferenceWith<detail::as_cref_t<T>, detail::as_cref_t<U>>,
      EqualityComparable<common_reference_t<detail::as_cref_t<T>, detail::as_cref_t<U>>>
    >
  >;

  template<typename T, typename U>
  using EqualityComparableWith = is_detected<EqualityComparableWith_t, T, U>;

  template<typename T, typename U>
  inline constexpr bool EqualityComparableWith_v = EqualityComparableWith<T, U>::value;

  // https://en.cppreference.com/w/cpp/concepts/destructible
  template<typename T>
  using Destructible_t = std::enable_if_t<
    std::is_nothrow_destructible_v<T>
  >;

  template<typename T>
  using Destructible = is_detected<Destructible_t, T>;

  template<typename T>
  inline constexpr bool Destructible_v = Destructible<T>::value;

  // https://en.cppreference.com/w/cpp/concepts/invocable
  template<typename F, typename... Args>
  using Invocable_t = std::enable_if_t<std::is_invocable_v<F, Args...>>;

  template<typename F, class... Args>
  using Invocable = is_detected<Invocable_t, F, Args...>;

  template<typename F, class... Args>
  inline constexpr bool Invocable_v = Invocable<F, Args...>::value;

  // https://en.cppreference.com/w/cpp/concepts/constructible_from
  template<typename T, typename... Args>
  using ConstructibleFrom_t = std::enable_if_t<
    Destructible_v<T> &&
    std::is_constructible_v<T, Args...>
  >;

  template<typename T, typename... Args>
  using ConstructibleFrom = is_detected<ConstructibleFrom_t, T, Args...>;

  template<typename T, typename... Args>
  inline constexpr bool ConstructibleFrom_v = ConstructibleFrom<T, Args...>::value;

  // https://en.cppreference.com/w/cpp/concepts/move_constructible
  template<typename T>
  using MoveConstructible_t = std::enable_if_t<
    ConstructibleFrom_v<T, T> &&
    ConvertibleTo_v<T, T>
  >;

  template<typename T>
  using MoveConstructible = is_detected<MoveConstructible_t, T>;

  template<typename T>
  inline constexpr bool MoveConstructible_v = MoveConstructible<T>::value;

  // https://en.cppreference.com/w/cpp/concepts/copy_constructible
  template<typename T>
  using CopyConstructible_t = std::enable_if_t<
    MoveConstructible_v<T> &&
    ConstructibleFrom_v<T, std::add_lvalue_reference_t<T>> && ConvertibleTo_v<std::add_lvalue_reference_t<T>, T> &&
    ConstructibleFrom_v<T, std::add_const_t<std::add_lvalue_reference_t<T>>> && ConvertibleTo_v<std::add_const_t<std::add_lvalue_reference_t<T>>, T> &&
    ConstructibleFrom_v<T, std::add_const_t<T>> && ConvertibleTo_v<std::add_const_t<T>, T>
  >;

  template<typename T>
  using CopyConstructible = is_detected<CopyConstructible_t, T>;

  template<typename T>
  inline constexpr bool CopyConstructible_v = CopyConstructible<T>::value;

  // https://en.cppreference.com/w/cpp/concepts/derived_from
  template<typename Derived, typename Base>
  using DerivedFrom_t = std::enable_if_t<
    std::is_base_of_v<Base, Derived> &&
    std::is_convertible_v<const volatile Derived*, const volatile Base*>
  >;

  template<typename Derived, typename Base>
  using DerivedFrom = is_detected<DerivedFrom_t, Derived, Base>;

  template<typename Derived, typename Base>
  inline constexpr bool DerivedFrom_v = DerivedFrom<Derived, Base>::value;
}
#endif
