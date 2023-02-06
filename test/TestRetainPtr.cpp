#include <memory.h>

#include <gtest/gtest.h>

#include <mutex>
#include <thread>

#ifdef _MSC_VER
#include <Unknwn.h>
#endif

namespace stdx::test
{
  struct Counter
  {
    inline static long instances = 0L;
  };

  //the class needs to derive from reference_count
  template<typename C>
  class Base : public stdx::reference_count<Base<C>>
  {
  public:
    Base()
    {
      ++C::instances;
    }

    virtual ~Base()
    {
      --C::instances;
    }

    Base(const Base&)
    {
      ++C::instances;
    }

    Base(Base&&) noexcept
    {
    }

    Base& operator=(const Base&)
    {
      ++C::instances;
      return *this;
    }

    Base& operator=(Base&&) noexcept
    {
      return *this;
    }
  };

  //the Base class is already derived from reference_count
  template<typename C>
  class Derived : public Base<C>
  {
  };

  //the class needs to derive from atomic_reference_count
  template<typename C>
  struct ThreadSafeBase : stdx::atomic_reference_count<ThreadSafeBase<C>>
  {
    inline static long count_of_instances = 0L;
    ThreadSafeBase()
    {
      ++C::instances;
    }

    virtual ~ThreadSafeBase()
    {
      --C::instances;
    }

    ThreadSafeBase(const ThreadSafeBase&)
    {
      ++C::instances;
    }

    ThreadSafeBase(ThreadSafeBase&&) noexcept
    {
    }

    ThreadSafeBase& operator=(const ThreadSafeBase&)
    {
      ++C::instances;
      return *this;
    }

    ThreadSafeBase& operator=(ThreadSafeBase&&) noexcept
    {
      return *this;
    }
  };

  //the ThreadSafeBase class is already derived from atomic_reference_count
  template<typename C>
  struct ThreadSafeDerived : ThreadSafeBase<C>
  {
  };

  template<typename T>
  class StdX_Memory_retain_ptr_test : public ::testing::Test
    {
    };

  using Base_Counted = Base<Counter>;
  using Derived_Counted = Derived<Counter>;
  using ThreadSafeBase_Counted = ThreadSafeBase<Counter>;
  using ThreadSafeDerived_Counted = ThreadSafeDerived<Counter>;

  using test_typelist = ::testing::Types<Base_Counted, Derived_Counted, ThreadSafeBase_Counted, ThreadSafeDerived_Counted>;

  TYPED_TEST_SUITE(StdX_Memory_retain_ptr_test, test_typelist, );

  TYPED_TEST(StdX_Memory_retain_ptr_test, basic_usage)
  {
    Counter::instances = 0L;
    using T = TypeParam;
    using TPtr = stdx::retain_ptr<T>;
    {
      TPtr ptr(new T);
      EXPECT_EQ(Counter::instances, 1);
      EXPECT_EQ(ptr.use_count(), 1);
      {
        // copy construction
        TPtr ptr2{ ptr };
        EXPECT_EQ(Counter::instances, 1);
        EXPECT_EQ(ptr.use_count(), 2);

        // move construction
        TPtr ptr3{ std::move(ptr2) };
        EXPECT_EQ(Counter::instances, 1);
        EXPECT_EQ(ptr.use_count(), 2);

        // copy assignment
        TPtr ptr4;
        ptr4 = ptr3;
        EXPECT_EQ(Counter::instances, 1);
        EXPECT_EQ(ptr.use_count(), 3);

        // move assignment
        TPtr ptr5;
        ptr5 = std::move(ptr4);
        EXPECT_EQ(Counter::instances, 1);
        EXPECT_EQ(ptr.use_count(), 3);
      }
      EXPECT_EQ(Counter::instances, 1);
      EXPECT_EQ(ptr.use_count(), 1);
    }
    EXPECT_EQ(Counter::instances, 0);
  }

#ifdef _MSC_VER
  // just an example of defined traits for WIN COM objects
  struct ILookup : IUnknown
  {
    virtual HRESULT __stdcall LookupByName(LPTSTR lpName, TCHAR** lplpNumber) = 0;
    virtual HRESULT __stdcall LookupByNumber(LPTSTR lpNUmber, TCHAR** lplpName) = 0;
    virtual ~ILookup() = default;
  };

  struct CLookup : ILookup
  {
    ULONG m_resource{};

    CLookup()
      : m_resource(1U)
    {
    }

    ULONG __stdcall AddRef() override
    {
      ++m_resource;
      return m_resource;
    }

    ULONG __stdcall Release() override
    {
      --m_resource;
      return m_resource;
    }

    HRESULT __stdcall QueryInterface(
      REFIID,
      _COM_Outptr_ void __RPC_FAR* __RPC_FAR*) override
    {
      return 0;
    }

    HRESULT __stdcall LookupByName(LPTSTR, TCHAR**) override
    {
      return 0;
    }

    HRESULT __stdcall LookupByNumber(LPTSTR, TCHAR**) override
    {
      return 0;
    }
  };

  //the traits type needs to implement at least:
  // - increment method
  // - decrement methods
  template<typename T>
  struct com_traits
  {
    static void increment(IUnknown* ptr)
    {
      static_cast<T*>(ptr)->AddRef();
    }

    static void decrement(IUnknown* ptr)
    {
      static_cast<T*>(ptr)->Release();
    }
  };

  template<typename T>
  using com_ptr = stdx::retain_ptr<T, com_traits<T>>;

  struct LookupResource
  {
    explicit LookupResource(ILookup* resource)
      : m_resource(resource)
    {
    }

    LookupResource() = default;
    ILookup* operator->() noexcept
    {
      return this->m_resource.get();
    }

    ILookup const* operator->() const noexcept
    {
      return this->m_resource.get();
    }

  protected:
    com_ptr<ILookup> m_resource{ };
  };

  TEST(StdX_Memory_retain_ptr, com_resource)
  {
    CLookup test_com{};
    LookupResource resource(&test_com);
    {
      [[maybe_unused]] LookupResource resource1 = resource; // implicit increment
      EXPECT_EQ(resource1->AddRef(), 3U); // explicit increment
      EXPECT_EQ(resource->Release(), 2U); // explicit decrement
    } // implicit decrement
    EXPECT_EQ(test_com.m_resource, 1U);
  }
#endif

  struct TypeWithParam : stdx::reference_count<TypeWithParam>
  {
    TypeWithParam(int in) : val(in) {}
    void add(int v)
    {
      val += v;
    }
    int val;
  };

  TEST(StdX_Memory_retain_ptr, construct_with_adopt_object)
  {
    Counter::instances = 0L;
    auto* p = new Derived_Counted();
    stdx::retain_ptr<Derived_Counted> ip{ p, stdx::adopt_object };
    EXPECT_TRUE(ip);
    EXPECT_EQ(ip.use_count(), 1);
    EXPECT_EQ(ip.get(), p);
  }

  TEST(StdX_Memory_retain_ptr, construct_with_retain_object)
  {
    Counter::instances = 0L;
    auto up = std::make_unique<Derived_Counted>();
    stdx::retain_ptr<Derived_Counted> ip(up.get(), stdx::retain_object);
    EXPECT_TRUE(ip);
    EXPECT_EQ(ip.use_count(), 2);
    EXPECT_EQ(ip.get(), up.get());
  }

  TEST(StdX_Memory_retain_ptr, hash)
  {
    Counter::instances = 0L;
    Derived_Counted* p = new Derived_Counted();
    stdx::retain_ptr<Derived_Counted> ip(p);
    EXPECT_EQ(std::hash<stdx::retain_ptr<Derived_Counted>>()(ip), std::hash<Derived_Counted*>()(p));
  }

  TEST(StdX_Memory_retain_ptr, swap)
  {
    using Ptr = Derived_Counted;
    using TPtr = stdx::retain_ptr<Ptr>;
    TPtr rp1(new Ptr);
    TPtr rp2(new Ptr);

    TPtr rp3 = rp1;
    EXPECT_EQ(rp1.use_count(), 2);
    EXPECT_EQ(rp2.use_count(), 1);
    EXPECT_EQ(rp1, rp3);
    EXPECT_NE(rp2, rp3);

    rp1.swap(rp2);
    EXPECT_EQ(rp1.use_count(), 1);
    EXPECT_EQ(rp2.use_count(), 2);
    EXPECT_EQ(rp2, rp3);
    EXPECT_NE(rp1, rp3);
  }

  TEST(StdX_Memory_retain_ptr, bool_converting_operator)
  {
    using Ptr = Derived_Counted;
    using TPtr = stdx::retain_ptr<Ptr>;
    TPtr rp1;
    TPtr rp2(new Ptr);

    EXPECT_TRUE(rp2);
    EXPECT_FALSE(rp1);
  }

  TEST(StdX_Memory_retain_ptr, release)
  {
    using Ptr = Derived_Counted;
    using TPtr = stdx::retain_ptr<Ptr>;
    auto* p = new Ptr;
    TPtr rp1(p);
    EXPECT_EQ(rp1.get(), p);
    EXPECT_EQ(rp1.release(), p);
    EXPECT_EQ(rp1.get(), nullptr);
    delete p;
  }

  TEST(StdX_Memory_retain_ptr, dereference_operators)
  {
    using Ptr = TypeWithParam;
    using TPtr = stdx::retain_ptr<Ptr>;

    TPtr rp1(new Ptr(5));
    EXPECT_EQ(rp1->val, 5);
    rp1->add(4);
    EXPECT_EQ(rp1->val, 9);

    TPtr rp2(new Ptr(10));
    EXPECT_EQ((*rp2).val, 10);
    (*rp2).add(-4);
    EXPECT_EQ((*rp2).val, 6);
  }

  TEST(StdX_Memory_retain_ptr, make_retain)
  {
    const auto ptr = stdx::make_retain<TypeWithParam>(5);

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_EQ(ptr->val, 5);
  }

  TEST(StdX_Memory_retain_ptr, is_retain_ptr)
  {
    auto rp = stdx::make_retain<TypeWithParam>(5);
    const auto crp = stdx::make_retain<TypeWithParam>(5);
    const auto up = std::make_unique<TypeWithParam>(5);

    EXPECT_TRUE(stdx::is_retain_ptr_v<decltype(rp)>);
    EXPECT_TRUE(stdx::is_retain_ptr_v<decltype(crp)>);
    EXPECT_FALSE(stdx::is_retain_ptr_v<decltype(up)>);
  }

  struct MyBase : stdx::reference_count<MyBase>
  {
    MyBase() = default;
    MyBase(const MyBase&) = default;
    MyBase(MyBase&&) = default;
    MyBase& operator=(const MyBase&) = default;
    MyBase& operator=(MyBase&&) = default;
    virtual ~MyBase() = default;
  };

  struct MySub : MyBase
  {
    explicit MySub(int x)
      : MyBase()
      , m_x(x)
    {
    }

    MySub(const MySub&) = default;
    MySub(MySub&&) = default;
    MySub& operator=(const MySub&) = default;
    MySub& operator=(MySub&&) = default;
    ~MySub() override = default;

    int m_x;
  };

  TEST(StdX_Memory_retain_ptr, test_shared)
  {
    using SharedBase = std::shared_ptr<MyBase>;
    using SharedSub = std::shared_ptr<MySub>;
    SharedBase base;
    SharedSub sub;
    base = sub; //compiles;
  }

  using RetainBase = stdx::retain_ptr<MyBase>;
  using RetainSub = stdx::retain_ptr<MySub>;

  TEST(StdX_Memory_retain_ptr, test_retain_assign_from_rvalue)
  {
    {
      auto base = RetainBase(new MyBase());
      EXPECT_TRUE(base);
      EXPECT_EQ(base.use_count(), 1);
      auto baseCopy = base;  //copy-construction
      EXPECT_TRUE(baseCopy);
      EXPECT_EQ(base.use_count(), 2);
      EXPECT_EQ(baseCopy.use_count(), 2);
      auto sub = RetainSub(new MySub(42));
      EXPECT_EQ(sub.use_count(), 1);
      auto sub2 = sub;
      EXPECT_EQ(sub2.use_count(), 2);
      EXPECT_EQ(sub.use_count(), 2);
      base = std::move(sub2);  //move-assignment
      EXPECT_TRUE(base);
      EXPECT_EQ(base.use_count(), 2);
      EXPECT_EQ(baseCopy.use_count(), 1);
    }

    {
      auto sub1 = RetainSub(new MySub(42));
      EXPECT_TRUE(sub1);
      EXPECT_EQ(sub1.use_count(), 1);
      auto sub2 = RetainSub(new MySub(24));
      EXPECT_TRUE(sub2);
      EXPECT_EQ(sub2.use_count(), 1);

      sub1 = std::move(sub2);  // move-assignment
      EXPECT_TRUE(sub1);
      // coverity[use_after_move] - Intentional
      EXPECT_FALSE(sub2); // NOLINT(bugprone-use-after-move)
      EXPECT_EQ(sub1.use_count(), 1);
      EXPECT_EQ(sub2.use_count(), 0);
    }
  }

  TEST(StdX_Memory_retain_ptr, test_retain_copy_construct_from_lvalue)
  {
    auto sub = RetainSub(new MySub(42));
    EXPECT_TRUE(sub);
    EXPECT_EQ(sub.use_count(), 1);
    {
      auto base = RetainBase(sub);
      EXPECT_TRUE(base);
      EXPECT_EQ(sub.use_count(), 2);
      EXPECT_EQ(base.use_count(), 2);
    }
    EXPECT_EQ(sub.use_count(), 1);
  }

  TEST(StdX_Memory_retain_ptr, test_retain_move_construct_from_rvalue)
  {
    auto sub = RetainSub(new MySub(42));
    EXPECT_TRUE(sub);
    EXPECT_EQ(sub.use_count(), 1);
    auto base = RetainBase(std::move(sub));
    EXPECT_TRUE(base);
    // coverity[use_after_move] - Intentional
    EXPECT_EQ(sub.use_count(), 0);  // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(base.use_count(), 1);
  }

  TEST(StdX_Memory_retain_ptr, test_retain_assign_from_lvalue)
  {
    {
      auto base = RetainBase(new MyBase());
      EXPECT_TRUE(base);
      EXPECT_EQ(base.use_count(), 1);
      {
        const auto sub = RetainSub(new MySub(42));
        EXPECT_TRUE(sub);
        EXPECT_EQ(sub.use_count(), 1);
        EXPECT_NE(base.get(), sub.get());
        base = sub; //copy-assignment
        EXPECT_EQ(base.use_count(), 2);
        EXPECT_EQ(sub.use_count(), 2);
        EXPECT_EQ(base.get(), sub.get());
      }
    }

    {
      auto sub1 = RetainSub(new MySub(24));
      EXPECT_TRUE(sub1);
      EXPECT_EQ(sub1.use_count(), 1);
      {
        const auto sub2 = RetainSub(new MySub(42));
        EXPECT_TRUE(sub2);
        EXPECT_EQ(sub2.use_count(), 1);
        EXPECT_NE(sub1.get(), sub2.get());
        sub1 = sub2; //copy-assignment
        EXPECT_EQ(sub1.use_count(), 2);
        EXPECT_EQ(sub2.use_count(), 2);
        EXPECT_EQ(sub1.get(), sub2.get());
      }
    }
  }

  RetainBase func_return_subclass()
  {
    return RetainSub(new MySub(42));
  }

  TEST(StdX_Memory_retain_ptr, test_return_subclass)
  {
    auto rp = func_return_subclass();
    EXPECT_TRUE(rp);
    EXPECT_EQ(rp.use_count(), 1);
  }

  TEST(StdX_Memory_retain_ptr, test_dynamic_pointer_cast)
  {
    {
      auto rp = func_return_subclass();
      EXPECT_TRUE(rp);
      EXPECT_EQ(rp.use_count(), 1);

      auto sp = stdx::dynamic_pointer_cast<MySub, stdx::retain_traits<MySub>>(rp);
      EXPECT_TRUE(rp);
      EXPECT_EQ(rp.use_count(), 2);
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 2);
    }

    {
      auto sp = stdx::dynamic_pointer_cast<MySub, stdx::retain_traits<MySub>>(func_return_subclass());
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 1);
      EXPECT_EQ(sp->m_x, 42);
    }
  }

  TEST(StdX_Memory_retain_ptr, test_static_pointer_cast)
  {
    {
      auto sp = RetainSub(new MySub(42));
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 1);

      auto bp = stdx::static_pointer_cast<MyBase, stdx::retain_traits<MyBase>>(sp);
      EXPECT_TRUE(bp);
      EXPECT_TRUE(sp);
      EXPECT_EQ(bp.use_count(), 2);
      EXPECT_EQ(sp.use_count(), 2);
    }

    {
      auto sp = RetainSub(new MySub(42));
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 1);

      auto bp = stdx::static_pointer_cast<MyBase, stdx::retain_traits<MyBase>>(std::move(sp));
      EXPECT_TRUE(bp);
      // coverity[use_after_move] - Intentional
      EXPECT_FALSE(sp);  // NOLINT(bugprone-use-after-move)
      EXPECT_EQ(bp.use_count(), 1);
    }
  }

  struct BaseTS : stdx::atomic_reference_count<BaseTS>
  {
    BaseTS() = default;
    // Note: non-virtual destructor is OK here
     ~BaseTS() = default;
  };

  struct DerivedTS : BaseTS
  {
    DerivedTS() = default;
    ~DerivedTS() = default;
  };

  TEST(StdX_Memory_retain_ptr, thread_safety)
  {
    using namespace std::chrono_literals;
    auto thr = [](stdx::retain_ptr<BaseTS> p, std::intptr_t addr) {
      std::this_thread::sleep_for(1s);
      auto count = p.use_count();
      EXPECT_TRUE(count > 0) << count;
      {
        const stdx::retain_ptr<BaseTS> lp = p;
        count = lp.use_count();
        EXPECT_EQ(addr, reinterpret_cast<std::intptr_t>(lp.get()));
        EXPECT_TRUE(count > 0) << count;
        // shared use_count is incremented
        {
          static std::mutex io_mutex;
          std::lock_guard lk(io_mutex);
          std::cout << "local pointer in a thread(" << std::this_thread::get_id() << "): \n"
            << "  lp.get() = " << lp.get()
            << ", lp.use_count() = " << lp.use_count() << '\n';
        }
      }
    };

    // created a retain DerivedTS (as a pointer to BaseTS)
    stdx::retain_ptr<BaseTS> p = stdx::make_retain<DerivedTS>();

    ASSERT_TRUE(p);
    EXPECT_EQ(p.use_count(), 1);
    std::thread t1(thr, p, reinterpret_cast<std::intptr_t>(p.get()));
    std::thread t2(thr, p, reinterpret_cast<std::intptr_t>(p.get()));
    std::thread t3(thr, p, reinterpret_cast<std::intptr_t>(p.get()));
    p.reset(); // release ownership from the main thread
    ASSERT_FALSE(p);
    EXPECT_EQ(p.use_count(), 0);
    t1.join(); t2.join(); t3.join();
    // All threads completed, the last one deleted DerivedTS
  }
} // end of namespace stdx::test
