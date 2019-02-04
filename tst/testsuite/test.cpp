#include <iostream>

#include <gtest/gtest.h>

#include <llvm-c/Support.h>

#include "wembed.hpp"
#include "test.hpp"

using namespace wembed;

void spectest_print() {
  std::cout << "spectest_print" << std::endl;
}
void spectest_print_i32(int32_t param) {
  std::cout << "spectest_print_i32: " << param << std::endl;
}
void spectest_print_i32_f32(int32_t param1, float param2) {
  std::cout << "spectest_print_i32_f32: " << param1 << ", " << param2 << std::endl;
}
void spectest_print_f64_f64(double param1, double param2) {
  std::cout << "spectest_print_f64_f64: " << param1 << ", " << param2 << std::endl;
}
void spectest_print_f32(float param) {
  std::cout << "spectest_print_f32: " << param << std::endl;
}
void spectest_print_f64(double param) {
  std::cout << "spectest_print_f64: " << param << std::endl;
}

int32_t spectest_global_i32 = 666;
float spectest_global_f32 = 0;
double spectest_global_f64 = 0;

wembed::memory spectest_mem(1, 2);
wembed::table spectest_tab(10, 20);

void spectest_reset() {
  spectest_global_i32 = 666;
  spectest_global_f32 = 0;
  spectest_global_f64 = 0;

  spectest_mem.resize(1);
  memset(spectest_mem.data(), 0, spectest_mem.size() * sPageSize);
  memset(spectest_tab.data_ptrs(), 0, spectest_tab.size() * sizeof(void*));
  memset(spectest_tab.data_types(), 0, spectest_tab.size() * sizeof(void*));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  wembed::llvm_init();
  return RUN_ALL_TESTS();
}
