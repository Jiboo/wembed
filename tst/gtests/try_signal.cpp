#include <cstring>

#include "gtest/gtest.h"

#include "try_signal.hpp"
#include "wembed.hpp"

TEST(try_signal, voidvoid) {
  try {
    sig::try_signal([] {});
    ASSERT_FALSE(false); // Should be reachable
  }
  catch (std::system_error const& e)
  {
    ASSERT_FALSE(true); // Should be unreachable
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, voidparams) {
  try {
    sig::try_signal([](int a, int b) {
      EXPECT_EQ(a, 1);
      EXPECT_EQ(b, 2);
    }, 1, 2);
    ASSERT_FALSE(false); // Should be reachable
  }
  catch (std::system_error const& e)
  {
    ASSERT_FALSE(true); // Should be unreachable
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, returnvoid) {
  try {
    int a = sig::try_signal([]()-> int { return 42; });
    EXPECT_EQ(a, 42);
  }
  catch (std::system_error const& e)
  {
    ASSERT_FALSE(true); // Should be unreachable
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, returnparams) {
  try {
    int res = sig::try_signal([](int a, int b) {
      return a + b;
    }, 1, 2);
    EXPECT_EQ(res, 3);
  }
  catch (std::system_error const& e)
  {
    ASSERT_FALSE(true); // Should be unreachable
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, trap_fpe) {
  try {
    sig::try_signal([] {
      std::cout << (11 / std::stoi("0")) << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::arithmetic_exception));
  }
  ASSERT_FALSE(false); // Should be executed

  try {
    sig::try_signal([] {
      std::cout << (std::numeric_limits<int>::min() / std::stoi("-1")) << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::arithmetic_exception));
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, trap_segv) {
  try {
    sig::try_signal([] {
      int *lTarget = (int*)std::stol("0");
      std::cout << *lTarget << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::segmentation));
  }
  ASSERT_FALSE(false); // Should be executed
}

TEST(try_signal, trap_segv_map) {
  wembed::memory lMemory(0);

  try {
    sig::try_signal([&lMemory] {
      uint8_t *lTarget = lMemory.data();
      std::cout << *lTarget << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::segmentation));
  }
  ASSERT_FALSE(false); // Should be executed

  lMemory.resize(1);

  try {
    sig::try_signal([&lMemory] {
      uint8_t *lTarget = lMemory.data();
      std::cout << *lTarget << std::endl;
      ASSERT_FALSE(false); // Should be reachable
    });
    ASSERT_FALSE(false); // Should be reachable
  }
  catch (std::system_error const& e)
  {
    ASSERT_FALSE(true); // Should be unreachable
  }
  ASSERT_FALSE(false); // Should be executed


  try {
    sig::try_signal([&lMemory] {
      uint8_t *lTarget = lMemory.data() + wembed::sPageSize;
      std::cout << *lTarget << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::segmentation));
  }
  ASSERT_FALSE(false); // Should be executed
}

void loop(size_t i, int *dst) {
  while (i > 0) {
    *dst = rand() + *dst * i;
    loop(i--, dst);
  }
}

TEST(try_signal, trap_so) {
  try {
    sig::try_signal([] {
      int dst;
      loop(std::numeric_limits<size_t>::max(), &dst);
      std::cout << dst << std::endl;
      ASSERT_FALSE(true); // Should be unreachable
    });
    ASSERT_FALSE(true); // Should be unreachable
  }
  catch (std::system_error const& e)
  {
    EXPECT_EQ(e.code(), std::error_condition(sig::errors::segmentation));
  }
  ASSERT_FALSE(false); // Should be executed
}
