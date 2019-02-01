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
      int a = 0;
      std::cout << (11 / a);
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
      int a = -1;
      std::cout << (std::numeric_limits<int>::min() / a);
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
      int *lTarget = nullptr;
      std::cout << *lTarget;
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
      std::cout << *lTarget;
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
      std::cout << *lTarget;
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
      std::cout << *lTarget;
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

void loop(size_t i) {
  while (i > 0)
    loop(i--);
}

TEST(try_signal, trap_so) {
  try {
    sig::try_signal([] {
      loop(std::numeric_limits<size_t>::max());
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
