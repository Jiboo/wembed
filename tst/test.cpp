#include <iostream>

#include <gtest/gtest.h>

#include "wembed.hpp"
#include "test.hpp"

using namespace wembed;

void spectest_print() {
  std::cout << "spectest_print: " << std::endl;
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
  LLVMLinkInMCJIT();
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();

#ifdef WEMBED_NATIVE_CODE_DUMP
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllDisassemblers();
#endif

  return RUN_ALL_TESTS();
}

void dump(const void* data, size_t size) {
  char ascii[17];
  size_t i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char*)data)[i]);
    if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char*)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i+1) % 8 == 0 || i+1 == size) {
      printf(" ");
      if ((i+1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i+1 == size) {
        ascii[(i+1) % 16] = '\0';
        if ((i+1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i+1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}