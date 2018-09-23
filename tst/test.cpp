#include <gtest/gtest.h>
#include "wembed.hpp"
#include "test.hpp"

using namespace wembed;

void spectest_print(uint8_t* base) {}
void spectest_print_i32(uint8_t* base, int32_t param) {}
void spectest_print_i32_f32(uint8_t* base, int32_t param, float param2) {}
void spectest_print_f64_f64(uint8_t* base, double param, double param2) {}
void spectest_print_f32(uint8_t* base, float param) {}
void spectest_print_f64(uint8_t* base, double param) {}

int32_t spectest_global_i32 = 0;
float spectest_global_f32 = 0;
double spectest_global_f64 = 0;

constexpr auto lPageSize = 64 * 1024;
wembed::virtual_mapping spectest_mem(lPageSize *10, lPageSize *10);

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