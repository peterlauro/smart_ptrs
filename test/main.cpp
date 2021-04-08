#include <gtest/gtest.h>
#include <exception>

int main(int argc, char* argv[])
{
  try
  {
    testing::InitGoogleTest(&argc, argv);

    //  check
    return RUN_ALL_TESTS();
  }
  catch (const std::exception& x)
  {
    auto const* why = x.what();
    printf("%s\n", why);
  }
  catch (...)
  {
  }

  return -1;
}
