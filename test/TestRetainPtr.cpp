#include <memory.h>

//  GTEST
#include <gtest/gtest.h>

#ifdef _MSC_VER
#include <Unknwn.h>
#endif

namespace stdx::test
{
  //helper class to verify against the count of references
  template<typename T>
  struct instance_counter
  {
    inline static long count_of_instances = 0L;

    instance_counter()
    {
      ++count_of_instances;
    }

    ~instance_counter()
    {
      --count_of_instances;
    }

    instance_counter(const instance_counter&)
    {
      ++count_of_instances;
    }

    instance_counter(instance_counter&&) noexcept
    {
    }

    instance_counter& operator=(const instance_counter&)
    {
      ++count_of_instances;
      return *this;
    }

    instance_counter& operator=(instance_counter&&) noexcept
    {
      return *this;
    }
  };

  //the class needs to derive from reference_counter
  class Base : public stdx::reference_count<Base>, public instance_counter<Base>
  {};

  //the Base class is already derived from reference_counter
  class Derived : public Base
  {};

  //the class needs to derive from atomic_reference_counter
  class ThreadSafeBase : public stdx::atomic_reference_count<ThreadSafeBase>, public instance_counter<ThreadSafeBase>
  {};

  //the ThreadSafeBase class is already derived from reference_counter
  class ThreadSafeDerived : public ThreadSafeBase
  {};

  template<typename T>
  class StdX_Memory_retain_ptr_test : public ::testing::Test
  {
  };

  using test_types = ::testing::Types<Base, Derived, ThreadSafeBase, ThreadSafeDerived>;

  TYPED_TEST_SUITE(StdX_Memory_retain_ptr_test, test_types,);

  TYPED_TEST(StdX_Memory_retain_ptr_test, basic_usage)
  {
    using T = TypeParam;
    using TPtr = stdx::retain_ptr<T>;
    {
      TPtr ptr(new T);
      EXPECT_EQ(T::count_of_instances, 1);
      EXPECT_EQ(ptr.use_count(), 1);
      {
        // copy construction
        TPtr ptr2{ ptr };
        EXPECT_EQ(T::count_of_instances, 1);
        EXPECT_EQ(ptr.use_count(), 2);

        // move construction
        TPtr ptr3{ std::move(ptr2) };
        EXPECT_EQ(T::count_of_instances, 1);
        EXPECT_EQ(ptr.use_count(), 2);

        // copy assignment
        TPtr ptr4;
        ptr4 = ptr3;
        EXPECT_EQ(T::count_of_instances, 1);
        EXPECT_EQ(ptr.use_count(), 3);

        // move assignment
        TPtr ptr5;
        ptr5 = std::move(ptr4);
        EXPECT_EQ(T::count_of_instances, 1);
        EXPECT_EQ(ptr.use_count(), 3);
      }
      EXPECT_EQ(T::count_of_instances, 1);
      EXPECT_EQ(ptr.use_count(), 1);
    }
    EXPECT_EQ(T::count_of_instances, 0);
  }

#ifdef _MSC_VER
  // just an example of defined traits on for WIN COM objects

  interface ILookup : public IUnknown
  {
    virtual HRESULT _stdcall LookupByName(LPTSTR lpName, TCHAR **lplpNumber) = 0;
    virtual HRESULT _stdcall LookupByNumber(LPTSTR lpNUmber, TCHAR **lplpName) = 0;
  };

  //the traits need to implement at least increment and decrement methods
  struct com_traits
  {
    static void increment(IUnknown* ptr) { ptr->AddRef(); }
    static void decrement(IUnknown* ptr)
    {
      // coverity[free] - Intentional
      ptr->Release();
    }
  };

  template<typename T>
  using com_ptr = stdx::retain_ptr<T, com_traits>;

  struct LookupResource
  {
    explicit LookupResource(ILookup* resource)
      : m_resource(resource)
    {
    }

    LookupResource() = default;

    ILookup* operator->() noexcept { return this->m_resource.get(); }
    ILookup const* operator->() const noexcept { return this->m_resource.get(); }

  protected:
    com_ptr<ILookup> m_resource{};
  };
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
    Derived* p = new Derived();
    stdx::retain_ptr<Derived> ip(p, stdx::adopt_object);
    EXPECT_TRUE(ip);
    EXPECT_EQ(ip.use_count(), 1);
    EXPECT_EQ(ip.get(), p);
  }

  TEST(StdX_Memory_retain_ptr, construct_with_retain_object)
  {
    auto up = std::make_unique<Derived>();
    stdx::retain_ptr<Derived> ip(up.get(), stdx::retain_object);
    EXPECT_TRUE(ip);
    EXPECT_EQ(ip.use_count(), 2);
    EXPECT_EQ(ip.get(), up.get());
  }

  TEST(StdX_Memory_retain_ptr, hash)
  {
    Derived* p = new Derived();
    stdx::retain_ptr<Derived> ip(p);
    EXPECT_EQ(std::hash<stdx::retain_ptr<Derived>>()(ip), std::hash<Derived*>()(p));
  }

  TEST(StdX_Memory_retain_ptr, swap)
  {
    using Ptr = Derived;
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
    using Ptr = Derived;
    using TPtr = stdx::retain_ptr<Ptr>;
    TPtr rp1;
    TPtr rp2(new Ptr);

    EXPECT_TRUE(rp2);
    EXPECT_FALSE(rp1);
  }

  TEST(StdX_Memory_retain_ptr, release)
  {
    using Ptr = Derived;
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
    MyBase()
    {
      //std::cout << __FUNCTION__ << std::endl;
    }
    virtual ~MyBase()
    {
      //std::cout << __FUNCTION__ << std::endl;
    }
  };

  struct MySub : MyBase
  {
    explicit MySub(int x)
      : MyBase()
      , m_x(x)
    {
      //std::cout << __FUNCTION__ << std::endl;
    }

    ~MySub() override
    {
      //std::cout << __FUNCTION__ << std::endl;
    }

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
      EXPECT_FALSE(sub2);
      EXPECT_EQ(sub1.use_count(), 1);
      EXPECT_EQ(sub2.use_count(), 0);
    }
  }

  TEST(StdX_Memory_retain_ptr, test_retain_assign_from_lvalue)
  {
    {
      auto base = RetainBase(new MyBase());
      EXPECT_TRUE(base);
      EXPECT_EQ(base.use_count(), 1);
      const auto sub = RetainSub(new MySub(42));
      EXPECT_TRUE(sub);
      EXPECT_EQ(sub.use_count(), 1);
      base = sub; //copy-assignment
      EXPECT_EQ(base.use_count(), 2);
      EXPECT_EQ(sub.use_count(), 2);
    }

    {
      auto sub1 = RetainSub(new MySub(24));
      EXPECT_TRUE(sub1);
      EXPECT_EQ(sub1.use_count(), 1);
      auto sub2 = RetainSub(new MySub(42));
      EXPECT_TRUE(sub2);
      EXPECT_EQ(sub2.use_count(), 1);
      sub1 = sub2; //copy-assignment
      EXPECT_EQ(sub1.use_count(), 2);
      EXPECT_EQ(sub2.use_count(), 2);
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

      auto sp = stdx::dynamic_pointer_cast<MySub>(rp);
      EXPECT_TRUE(rp);
      EXPECT_EQ(rp.use_count(), 2);
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 2);
    }

    {
      auto sp = stdx::dynamic_pointer_cast<MySub>(func_return_subclass());
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

      auto bp = stdx::static_pointer_cast<MyBase>(sp);
      EXPECT_TRUE(bp);
      EXPECT_EQ(bp.use_count(), 2);
      EXPECT_EQ(sp.use_count(), 2);
    }

    {
      auto sp = RetainSub(new MySub(42));
      EXPECT_TRUE(sp);
      EXPECT_EQ(sp.use_count(), 1);

      auto bp = stdx::static_pointer_cast<MyBase>(std::move(sp));
      EXPECT_FALSE(sp);
      EXPECT_TRUE(bp);
      EXPECT_EQ(bp.use_count(), 1);
    }
  }
} // end of namespace stdx::test
