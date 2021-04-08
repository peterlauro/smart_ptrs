#ifndef STDX_TYPETRAITS_H
#define STDX_TYPETRAITS_H

#include <type_traits>

namespace stdx
{
  //
  //  EXPERIMENTAL V2 type_traits; part of GCC/Clang, lack of support in MSVC
  //

  // https://people.eecs.berkeley.edu/~brock/blog/detection_idiom.php
  // https://blog.tartanllama.xyz/detection-idiom/
  // https://en.cppreference.com/w/cpp/experimental/is_detected
  namespace detail
  {
    template<typename Default, typename AlwaysVoid, template<typename...> class Op, typename... Args>
    struct detector
    {
      using value_t = std::false_type;
      using type = Default;
    };

    template<typename Default, template<typename...> class Op, typename... Args>
    struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
    {
      using value_t = std::true_type;
      using type = Op<Args...>;
    };
  }

  struct nonesuch
  {
    ~nonesuch() = delete;
    nonesuch(const nonesuch&) = delete;
    void operator=(const nonesuch&) = delete;
    nonesuch(nonesuch&&) = delete;
    void operator=(nonesuch&&) = delete;
  };

  template<template<typename...> class Op, typename... Args>
  using is_detected = typename detail::detector<nonesuch, void, Op, Args...>::value_t;

  template<template<typename...> class Op, typename... Args>
  inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;

  template<template<typename...> class Op, typename... Args>
  using detected_t = typename detail::detector<nonesuch, void, Op, Args...>::type;

  template<typename Default, template<typename...> class Op, typename... Args>
  using detected_or = detail::detector<Default, void, Op, Args...>;

  template<typename Default, template<typename...> class Op, typename... Args>
  using detected_or_t = typename detected_or<Default, Op, Args...>::type;

  template<typename Expected, template<typename...> class Op, typename... Args>
  using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

  template<typename Expected, template<typename...> class Op, typename... Args>
  inline constexpr bool is_detected_exact_v = is_detected_exact<Expected, Op, Args...>::value;

  template<typename To, template<typename...> class Op, typename... Args>
  using is_detected_convertible = std::is_convertible<detected_t<Op, Args...>, To>;

  template<typename To, template<typename...> class Op, typename... Args>
  inline constexpr bool is_detected_convertible_v = is_detected_convertible<To, Op, Args...>::value;

  //
  // CXX20
  //

  // https://en.cppreference.com/w/cpp/types/remove_cvref
  template<typename T>
  struct remove_cvref
  {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
  };

  template<typename T>
  using remove_cvref_t = typename remove_cvref<T>::type;

  // std::is_nothrow_convertible
  // https://en.cppreference.com/w/cpp/types/is_convertible
  // C++ proposal P0758r1
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0758r1.html
  namespace detail
  {
    template<typename From, typename To,
      bool = std::disjunction_v<std::is_void<From>, std::is_function<To>, std::is_array<To>>
    >
    struct do_is_nothrow_convertible : std::is_void<To>
    {};

    struct do_is_nothrow_convertible_impl
    {
      template<typename To>
      static void test_aux(To) noexcept;

      template<typename From, typename To>
      static std::bool_constant<noexcept(test_aux<To>(std::declval<From>()))>
      test(int);

      template<typename, typename>
      static std::false_type
      test(...);
    };

    template<typename From, typename To>
    struct do_is_nothrow_convertible<From, To, false>
    {
      using type = decltype(do_is_nothrow_convertible_impl::test<From, To>(0));
    };
  }

  template<typename From, typename To>
  struct is_nothrow_convertible : detail::do_is_nothrow_convertible<From, To>::type
  {
  };

  template<typename From, typename To>
  inline constexpr bool is_nothrow_convertible_v = is_nothrow_convertible<From, To>::value;
}

#endif
