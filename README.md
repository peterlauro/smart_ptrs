# smart_ptrs

-  retain_ptr - An Intrusive Smart Pointer based on the proposal [P0468R1](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0468r1.html) and the [reference implementation](https://github.com/bruxisma/retain-ptr)
-  added support of polymorphic types (pointer aliasing)
-  partially mimic the API of std::shared_ptr

## retain_ptr<T, Traits>
  A retain pointer is an object that extends the lifetime of another object
  (which in turn manages its own disposition) and manages that other object
  through a pointer. Specifically, a retain pointer is an object `r` that stores
  a pointer to a second object `p` and will cease to extend the lifetime of `p` when `r`
  is itself destroyed (e.g., when leaving a block scope).
  In this context, `r` is said to retain `p`, and `p` is said to be a self disposing object.
```c++
template<typename T, typename Traits>
struct retain_ptr
{
public:
  using element_type = T;
  using traits_type = Traits;

  retain_ptr() noexcept = default;
  explicit retain_ptr(pointer p);
  
  retain_ptr(pointer p, adopt_object_t) noexcept;
  retain_ptr(pointer p, retain_object_t);
  retain_ptr(const retain_ptr& other) noexcept;

  retain_ptr(retain_ptr&& other) noexcept;

  template<typename U, typename UTraits>
  retain_ptr(const retain_ptr<U, UTraits>& other) noexcept;

  template<typename U, typename UTraits>
  retain_ptr(retain_ptr<U, UTraits>&& other) noexcept;

  retain_ptr(std::nullptr_t) noexcept;

  retain_ptr& operator=(const retain_ptr& other);

  template<typename U, typename UTraits)
  retain_ptr& operator=(const retain_ptr<U, UTraits>& other);

  retain_ptr& operator=(retain_ptr&& other) noexcept;

  template<typename U, typename UTraits>
  retain_ptr& operator=(retain_ptr<U, UTraits>&& other) noexcept;

  retain_ptr& operator=(std::nullptr_t) noexcept;

  ~retain_ptr();

  element_type& operator*() const noexcept;

  pointer operator->() const noexcept;

  [[nodiscard]]
  pointer get() const noexcept;

  explicit operator pointer() const noexcept = delete;

  [[nodiscard]]
  explicit operator bool() const noexcept;

  [[nodiscard]]
  std::ptrdiff_t use_count() const noexcept;

  [[nodiscard]]
  pointer release() noexcept;

  void reset(pointer p, retain_object_t);

  void reset(pointer p, adopt_object_t) noexcept;

  void reset(pointer p = pointer{});

  void swap(retain_ptr& other) noexcept;
};

template<typename T, typename Traits>
void swap(retain_ptr<T, Traits>& lhs, retain_ptr<T, Traits>& rhs)
noexcept(noexcept(lhs.swap(rhs)));

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> static_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> static_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> const_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> const_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> dynamic_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> dynamic_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> reinterpret_pointer_cast(const retain_ptr<U, UTraits>& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
retain_ptr<T, Traits> reinterpret_pointer_cast(retain_ptr<U, UTraits>&& other) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator==(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator!=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator<(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator<=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator>(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits, typename U, typename UTraits>
[[nodiscard]]
bool operator>=(const retain_ptr<T, Traits>& x, const retain_ptr<U, UTraits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator==(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator==(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator!=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator!=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator<(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator<(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator<=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator<=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator>(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator>(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator>=(const retain_ptr<T, Traits>& x, std::nullptr_t) noexcept;

template<typename T, typename Traits>
[[nodiscard]]
bool operator>=(std::nullptr_t, const retain_ptr<T, Traits>& y) noexcept;

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

template<typename CharT, typename Traits, typename T, typename TTraits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const retain_ptr<T, TTraits>& ptr);

namespace std
{
  template<typename T, typename Traits>
  struct hash<retain_ptr<T, Traits>>;
}; 
```