#ifndef STDX_MEMORY_H
#define STDX_MEMORY_H

#include "concepts.h"
#include "utils.h"

#include <atomic>
#include <memory>
#include <utility>

namespace stdx
{
  // An Intrusive Smart Pointer based on the proposal P0468R1
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0468r1.html
  // plus additional std smart_pointer functions (casting, hash)
  //
  // retain_ptr<T, Traits>
  // A retain pointer is an object that extends the lifetime of another object
  // (which in turn manages its own disposition) and manages that other object
  // through a pointer. Specifically, a retain pointer is an object r that stores
  // a pointer to a second object p and will cease to extend the lifetime of p when r
  // is itself destroyed (e.g., when leaving a block scope).
  // In this context, r is said to retain p, and p is said to be a self disposing object.
  namespace detail
  {
    /**
     * \brief helps to detects whether template parameter Traits defines a pointer type
     * \tparam Traits template type parameter
     */
     template<typename Traits>
     using has_pointer = typename Traits::pointer;

    /**
     * \brief helps to detects whether template parameter Traits defines a function default_action
     * \tparam Traits template type parameter
     * \note the signature of default_action: R default_action();
     * \note the R type may be type of retain_object_t or adopt_object_t
     */
    template<typename Traits>
    using has_default_action = typename Traits::default_action;

    /**
     * \brief helps to detects whether template parameter Traits defines a function use_count
     * \tparam Traits template type parameter
     * \note the signature of use_count: long use_count(pointer type)
     */
    template<typename Traits, typename P>
    using has_use_count = decltype(Traits::use_count(std::declval<P>()));

    /**
     * \brief helps to detects whether template parameter Traits defines a function increment
     * \tparam Traits template type parameter
     * \note the signature of use_count: void increment(pointer type)
     */
    template<typename Traits, typename P>
    using has_increment = decltype(Traits::increment(std::declval<P>()));

    /**
     * \brief helps to detects whether template parameter Traits defines a function decrement
     * \tparam Traits template type parameter
     * \note the signature of use_count: void decrement(pointer type)
     */
    template<typename Traits, typename P>
    using has_decrement = decltype(Traits::decrement(std::declval<P>()));
  } // end of namespace detail

  template<typename T> struct retain_traits;

  /**
   * \brief atomic_reference_count is a mixin type, provided for user defined types
   *        that simply rely on new and delete to have their lifetime extended by retain_ptr.
   *        The template parameter T is intended to be the type deriving from
   *        atomic_reference_count (a.k.a. the curiously repeating template pattern, CRTP).
   * \tparam T template type parameter
   */
  template<typename T>
  struct atomic_reference_count
  {
    using size_type = std::ptrdiff_t;

    template<typename>
    friend struct retain_traits;

  protected:
    constexpr atomic_reference_count() noexcept = default;

  private:
    std::atomic<size_type> m_count{ 1 };
  };

  /**
   * \brief reference_count is a mixin type, provided for user defined types
   *        that simply rely on new and delete to have their lifetime extended by retain_ptr.
   *        The template parameter T is intended to be the type deriving from
   *        reference_count (a.k.a. the curiously repeating template pattern, CRTP).
   * \tparam T template type parameter
   */
  template<typename T>
  struct reference_count
  {
    using size_type = std::ptrdiff_t;

    template<typename>
    friend struct retain_traits;

  protected:
    constexpr reference_count() noexcept = default;

  private:
    size_type m_count{ 1 };
  };

  /**
   * \brief sentinel type
   */
  struct retain_object_t
  {
    constexpr explicit retain_object_t() noexcept = default;
  };

  /**
   * \brief sentinel type
   */
  struct adopt_object_t
  {
    constexpr explicit adopt_object_t() noexcept = default;
  };

  inline constexpr retain_object_t retain_object{};
  inline constexpr adopt_object_t adopt_object{};

  /**
   * \brief The class template retain_traits serves the default traits object
   *        for the class template retain_ptr. Unless retain_traits is specialized
   *        for a specific type, the template parameter T must inherit from either
   *        atomic_reference_count<T> or reference_count. In the event that
   *        retain_traits is specialized for a type, the template parameter
   *        T may be an incomplete type.
   * \tparam T template type parameter
   * \note any user specialization of traits needs to define at least increment and decrement functions
   */
  template<typename T>
  struct retain_traits final
  {
    using element_type = T;
    using default_action = adopt_object_t;

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    static void increment(atomic_reference_count<U>* ptr) noexcept
    {
      ptr->m_count.fetch_add(1, std::memory_order_relaxed);
    }

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    static void decrement(atomic_reference_count<U>* ptr) noexcept
    {
      // gcc 12.1 complains about dereferencing a deleted ptr
      // the static cast to T* is required before the first ptr->
      auto t_ptr = static_cast<T*>(ptr);
      if (ptr->m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
      {
        delete t_ptr;
      }
    }

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    [[nodiscard]]
    static typename atomic_reference_count<U>::size_type use_count(const atomic_reference_count<U>* ptr) noexcept
    {
      return ptr->m_count.load(std::memory_order_relaxed);
    }

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    static void increment(reference_count<U>* ptr) noexcept
    {
      ++ptr->m_count;
    }

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    static void decrement(reference_count<U>* ptr) noexcept
    {
      // gcc 12.1 complains about dereferencing a deleted ptr
      // the static cast to T* is required before the first ptr->
      auto t_ptr = static_cast<T*>(ptr);
      if (--ptr->m_count == 0)
      {
        delete t_ptr;
      }
    }

    template<typename U
      requires_T(std::is_base_of_v<U, T>)
    >
    static typename reference_count<U>::size_type use_count(const reference_count<U>* ptr) noexcept
    {
      return ptr->m_count;
    }
  };

  /**
   * \brief The default type for the template parameter Traits is retain_traits.
   *        A client supplied template argument Traits shall be an object with non-member
   *        functions for which, given a ptr of type retain_ptr<T, Traits>::pointer,
   *        the expressions Traits::increment(ptr) and Traits::decrement(ptr) are valid
   *        and has the effect of retaining or disposing of the pointer as appropriate for that retainer.
   *
   *        If the qualified-id Traits::pointer is valid and denotes a type,
   *        then retain_ptr<T, Traits>::pointer shall be synonymous with Traits::pointer.
   *        Otherwise retain_ptr<T, Traits>::pointer shall be a synonym for element_type*.
   *        The type retain_ptr<T, Traits>::pointer shall satisfy the requirements of NullablePointer.use_count
   * \tparam T the type of the object managed by this
   * \tparam Traits the traits suitable for type T
   * \note The retain_ptr provides the same level of thread-safety as std::shared_ptr https://en.cppreference.com/w/cpp/memory/shared_ptr
   */
  template<typename T, typename Traits = retain_traits<T>>
  struct retain_ptr
  {
    using element_type = T;
    using traits_type = Traits;

    using pointer = detected_or_t<
      std::add_pointer_t<element_type>,
      detail::has_pointer,
      traits_type>;

  private:
    using default_action = detected_or_t<
      adopt_object_t,
      detail::has_default_action,
      traits_type>;

    static constexpr auto has_use_count_v = is_detected<
      detail::has_use_count,
      traits_type,
      pointer>{};

    static constexpr auto has_increment_v = is_detected<
      detail::has_increment,
      traits_type,
      pointer>{};

    static constexpr auto has_decrement_v = is_detected<
      detail::has_decrement,
      traits_type,
      pointer>{};

    using size_type = detected_or_t<
      ptrdiff_t,
      detail::has_use_count,
      traits_type,
      pointer>;

    static_assert(
      std::is_same_v<default_action, adopt_object_t> || std::is_same_v<default_action, retain_object_t>,
      "traits_type::default_action must return type of adopt_object_t or retain_object_t");

    static_assert(has_increment_v,
      "traits_type::increment needs to be defined."
      " Note: Check whether type T is derived from reference_count or atomic_reference_count.");

    static_assert(has_decrement_v,
      "traits_type::decrement needs to be defined."
      " Note: Check whether type T is derived from reference_count or atomic_reference_count.");

  public:
    /// @name Construction
    /// @{

    /**
     * \brief Constructs a retain_ptr object that retains nothing, value-initializing the stored pointer.
     */
    retain_ptr() noexcept = default;

    /**
     * \brief Constructs a retain_ptr that adopts p, initializing the stored pointer with p.
     * \param p a pointer to an object to manage
    * \note p's reference count remains untouched.
    */
    retain_ptr(pointer p, adopt_object_t) noexcept
      : m_ptr(p)
    {
    }

    /**
     * \brief Constructs a retain_ptr that retains p, initializing the stored pointer with p,
     *        and increments the reference count of p if p != nullptr by way of traits_type::increment.
     * \param p a pointer to an object to manage
     * \note If an exception is thrown during increment, this constructor will have no effect.
     */
    retain_ptr(pointer p, retain_object_t)
      : retain_ptr(p, adopt_object)
    {
      if (*this)
      {
        traits_type::increment(this->get());
      }
    }

    /**
     * \brief Constructs a retain_ptr by delegating to another retain_ptr constructor
     *        via traits_type::default_action.
     *        If traits_type::default_action does not exist, retain_ptr is constructed
     *        as if by retain_ptr(p, adopt_object_t).
     * \param p a pointer to an object to manage
     */
    explicit retain_ptr(pointer p)
      : retain_ptr(p, default_action())
    {
    }

    /**
     * \brief Constructs a retain_ptr by extending management from other to *this.
     *        The stored pointers reference count is incremented.
     * \param other another retain_pointer
     */
    retain_ptr(const retain_ptr& other) noexcept
      : m_ptr(other.m_ptr)
    {
      if (*this)
      {
        traits_type::increment(this->get());
      }
    }

    /**
     * \brief Constructs a retain_ptr by moving management from other to *this.
     *        The stored pointer reference count remains untouched.
     * \param other another retain_pointer
     */
    retain_ptr(retain_ptr&& other) noexcept
      : m_ptr(other.release())
    {
    }

    /**
     * \brief Constructs a retain_ptr by extending management from other to *this.
     *        The stored pointers reference count is incremented.
     * \tparam U needs to be DerivedFrom T
     * \tparam UTraits traits type of U type
     * \param other the instance of retain_ptr<U, UTraits>
     * \note not a part of proposal
     */
    template<typename U, typename UTraits
        requires_T(DerivedFrom_v<U, T>)
    >
    retain_ptr(const retain_ptr<U, UTraits>& other) noexcept
      : m_ptr(other.get())
    {
      if (other)
      {
        UTraits::increment(other.get());
      }
    }

    /**
     * \brief Constructs a retain_ptr by moving management from other to *this.
     *        The stored pointer reference count remains untouched.
     * \tparam U needs to be DerivedFrom T
     * \tparam UTraits traits type of U type
     * \param other the instance of retain_ptr<U, UTraits>
     * \note not a part of proposal
     */
    template<typename U, typename UTraits
        requires_T(DerivedFrom_v<U, T>)
    >
    retain_ptr(retain_ptr<U, UTraits>&& other) noexcept
        : m_ptr(other.release())
    {
    }

    /**
     * \brief Constructs a retain_ptr object that retains nothing, value-initializing the stored pointer.
     */
    retain_ptr(std::nullptr_t) noexcept
      : retain_ptr()
    {
    }

    /// @}

    /// @name Assignment Operators
    /// @{

    /**
     * \brief Copy assignment operator
     * \param other a retain_ptr object from which ownership will be transferred
     * \return *this
     */
    retain_ptr& operator=(const retain_ptr& other)
    {
      if (&other != this)
      {
        if (other)
        {
          traits_type::increment(other.get());
        }
        if (*this)
        {
          traits_type::decrement(this->get());
        }
        this->m_ptr = other.get();
      }
      return *this;
    }

    /**
     * \brief Copy assignment operator
     * \tparam U U needs to be DerivedFrom T
     * \tparam UTraits traits type of U type
     * \param other the instance of retain_ptr<U, UTraits>
     * \return *this
     * \note not a part of proposal
     */
    template<typename U, typename UTraits
      requires_T(DerivedFrom_v<U, T>)
    >
    retain_ptr& operator=(const retain_ptr<U, UTraits>& other)
    {
      if (other)
      {
        traits_type::increment(other.get());
      }
      if (*this)
      {
        traits_type::decrement(this->get());
      }
      this->m_ptr = other.get();

      return *this;
    }

    /**
     * \brief Move assignment operator
     * \param other a retain_ptr object from which ownership will be transferred
     * \return *this
     */
    retain_ptr& operator=(retain_ptr&& other) noexcept
    {
      if (&other != this)
      {
        retain_ptr(std::move(other)).swap(*this);
      }
      return *this;
    }

    /**
     * \brief Move assignment operator
     * \tparam U U needs to be DerivedFrom T
     * \tparam UTraits traits type of U type
     * \param other the instance of retain_ptr<U, UTraits>
     * \return *this
     * \note not a part of proposal
     */
    template<typename U, typename UTraits
      requires_T(DerivedFrom_v<U, T>)
    >
    retain_ptr& operator=(retain_ptr<U, UTraits>&& other) noexcept
    {
      if (*this)
      {
        traits_type::decrement(this->get());
      }
      this->m_ptr = other.release();
      return *this;
    }

    /**
     * \brief Effectively the same as calling reset()
     * \return *this
     */
    retain_ptr& operator=(std::nullptr_t) noexcept
    {
      this->reset();
      return *this;
    }

    /// @}

    /// @name Destruction
    /// @{

    /**
     * \brief destructor
     * \note Although retain_ptr<T> may be constructed with incomplete type T,
     *       the type T must be complete at the point of code where the destructor is called.
     */
    ~retain_ptr()
    {
      if (*this)
      {
        traits_type::decrement(this->get());
      }
    }

    /// @}

    /**
     * \brief Dereferences the stored pointer. The behavior is undefined if the stored pointer is null.
     * \return *get()
     * \note requires get() != nullptr, otherwise undefined behaviour
     */
    element_type& operator*() const noexcept
    {
      return *(this->get());
    }

    /**
     * \brief Dereferences the stored pointer. The behavior is undefined if the stored pointer is null.
     * \return get()
     * \note requires get() != nullptr
     * \note Use typically requires that element_type be a complete type.
     */
    pointer operator->() const noexcept
    {
      return this->get();
    }

    /**
     * \brief returns the stored pointer
     * \return stored pointer
     */
    [[nodiscard]]
    pointer get() const noexcept
    {
      return this->m_ptr;
    }

    /**
     * \brief the conversion operator to pointer type
     * \note the conversion operator is a part of proposal P0468R1,
     *       however the conversion operator is explicitly deleted by the current
     *       implementation
     */
    explicit operator pointer() const noexcept = delete;
    // {
    //     return this->get();
    // }

    /**
     * \brief Checks whether *this owns an object, i.e. whether get() != nullptr
     */
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
      return this->get() != nullptr;
    }

    /**
     * \brief  Value representing the current reference count of the current stored pointer.
     * \return -1 - if traits_type::use_count(get()) is not a valid expression,
     *          0 - if get() == nullptr,
     *          count - otherwise
     */
    [[nodiscard]]
    std::ptrdiff_t use_count() const noexcept
    {
      if constexpr (has_use_count_v)
      {
        return *this ? clamp_cast<std::ptrdiff_t>(traits_type::use_count(this->get())) : std::ptrdiff_t{ 0 };
      }
      else
      {
        return std::ptrdiff_t{ -1 };
      }
    }

    /**
     * \brief Releases the ownership of the managed object if any. get() returns nullptr after the call.
              The caller is responsible for deleting the object.
     * \return pointer to the managed object or nullptr. The value which would be returned by get() before the call
     */
    [[nodiscard]]
    pointer release() noexcept
    {
      auto* ptr = this->get();
      this->m_ptr = pointer{};
      return ptr;
    }

    /**
     * \brief replaces the managed object
     * \param p pointer to a new object to manage
     * \note Assigns p to the stored pointer, and then if the old value of stored pointer
     *       old_p, was not equal to nullptr, calls traits_type::decrement.
     *       Then if p is not equal to nullptr, traits_type::increment is called.
     */
    void reset(pointer p, retain_object_t)
    {
      *this = retain_ptr(p, retain_object);
    }

    /**
     * \brief replaces the managed object
     * \param p pointer to a new object to manage
     * \note Assigns p to the stored pointer, and then if the old value of stored pointer
     *         old_p, was not equal to nullptr, calls traits_type::decrement.
     */
    void reset(pointer p, adopt_object_t) noexcept
    {
      *this = retain_ptr(p, adopt_object);
    }

    /**
     * \brief replaces the managed object
     * \param p pointer to a new object to manage
     * \note Delegates assignment of p to the stored pointer via reset(p, traits_type::default_action())
     */
    void reset(pointer p = pointer{})
    {
      *this = retain_ptr(p, default_action());
    }

    /**
     * \brief Invokes swap on the stored pointers of *this and other.
     * \param other another retain_ptr object to swap the managed object with
     */
    void swap(retain_ptr& other) noexcept
    {
      using std::swap;
      swap(m_ptr, other.m_ptr);
    }

  private:
    pointer m_ptr{nullptr};
  };

  template<typename T, typename Traits>
  void swap(retain_ptr<T, Traits>& lhs, retain_ptr<T, Traits>& rhs)
  noexcept(noexcept(lhs.swap(rhs)))
  {
    lhs.swap(rhs);
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> static_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept
  {
    const auto ptr = static_cast<typename retain_ptr<T, Traits>::pointer>(other.get());
    return retain_ptr<T, Traits>(ptr, retain_object);
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> static_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept
  {
    return retain_ptr<T, Traits>(static_cast<typename retain_ptr<T, Traits>::pointer>(other.release()));
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> const_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept
  {
    const auto ptr = const_cast<typename retain_ptr<T, Traits>::pointer>(other.get());
    return retain_ptr<T, Traits>(ptr, retain_object);
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> const_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept
  {
    return retain_ptr<T, Traits>(const_cast<typename retain_ptr<T, Traits>::pointer>(other.release()));
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> dynamic_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept
  {
    if (const auto ptr = dynamic_cast<typename retain_ptr<T, Traits>::pointer>(other.get()); ptr)
    {
      return retain_ptr<T, Traits>(ptr, retain_object);
    }
    return {};
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> dynamic_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept
  {
    if (const auto ptr = dynamic_cast<typename retain_ptr<T, Traits>::pointer>(other.release()); ptr)
    {
      return retain_ptr<T, Traits>(ptr);
    }
    return {};
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> reinterpret_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept
  {
    const auto ptr = reinterpret_cast<typename retain_ptr<T, Traits>::pointer>(other.get());
    return retain_ptr<T, Traits>(ptr, retain_object);
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  retain_ptr<T, Traits> reinterpret_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept
  {
    return retain_ptr<T, Traits>(reinterpret_cast<typename retain_ptr<T, Traits>::pointer>(other.release()));
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator==(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return x.get() == y.get();
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator!=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return x.get() != y.get();
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator<(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return std::less<>()(x.get(), y.get());
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator<=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return !(y < x);
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator>(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return y < x;
  }

  template<typename T, typename Traits, typename U, typename UTraits>
  [[nodiscard]]
  bool operator>=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept
  {
    return !(x < y);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator==(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return !x;
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator==(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return !y;
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator!=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return static_cast<bool>(x);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator!=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return static_cast<bool>(y);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator<(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return std::less<typename retain_ptr<T, Traits>::pointer>()(x.get(), nullptr);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator<(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return std::less<typename retain_ptr<T, Traits>::pointer>()(nullptr, y.get());
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator<=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return !(nullptr < x);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator<=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return !(y < nullptr);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator>(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return nullptr < x;
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator>(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return y < nullptr;
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator>=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept
  {
    return !(x < nullptr);
  }

  template<typename T, typename Traits>
  [[nodiscard]]
  bool operator>=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept
  {
    return !(nullptr < y);
  }

  // FUNCTIONs which are not part of the proposal p0468r1
  template<typename T>
  struct is_retain_ptr : std::false_type
  {
  };

  template<typename T, typename Traits>
  struct is_retain_ptr<retain_ptr<T, Traits>> : std::true_type
  {
  };

  template<typename T>
  inline constexpr auto is_retain_ptr_v = is_retain_ptr<stdx::remove_cvref_t<T>>::value;

  template<typename T, typename... Args>
  [[nodiscard]]
  retain_ptr<T> make_retain(Args&&... args)
  {
    return retain_ptr<T>(new T(std::forward<Args>(args)...), adopt_object);
  }

  template<typename T, typename Traits, typename... Args>
  [[nodiscard]]
  retain_ptr<T, Traits> make_retain_with_traits(Args&&... args)
  {
    return retain_ptr<T, Traits>(new T(std::forward<Args>(args)...), adopt_object);
  }

  /**
   * \brief Inserts the value of the pointer managed by ptr into the output stream os.
   * \tparam CharT raw character type
   * \tparam Traits traits for CharT
   * \tparam T the type of the object managed by stdx::retain_ptr
   * \tparam TTraits the traits suitable for type T
   * \param os a std::basic_ostream to insert ptr into
   * \param ptr the pointer to be inserted into os
   * \return os
   */
  template<typename CharT, typename Traits, typename T, typename TTraits>
  std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const retain_ptr<T, TTraits>& ptr)
  {
    return os << ptr.get();
  }
} // end of namespace stdx

namespace std
{
  /**
   * \brief The template specialization of std::hash for stdx::retain_ptr<T, Traits> allows
   *        users to obtain hashes of objects of type stdx::retain_ptr<T, Traits>
   * \tparam T the type of the object managed by stdx::retain_ptr
   * \tparam Traits the traits suitable for type T
   */
  template<typename T, typename Traits>
  struct hash<stdx::retain_ptr<T, Traits>>
    : stdx::detail::conditionally_enabled_hash<
      stdx::retain_ptr<T, Traits>,
      std::is_default_constructible_v<std::hash<typename stdx::retain_ptr<T, Traits>::pointer>>>
  {
    static size_t do_hash(const stdx::retain_ptr<T, Traits>& key)
    noexcept(stdx::detail::is_nothrow_hashable_v<typename stdx::retain_ptr<T, Traits>::pointer>)
    {
      return std::hash<typename stdx::retain_ptr<T, Traits>::pointer>{}(key.get());
    }
  };
} // end of namespace std

#endif
