#include "gtest/gtest.h"

#include "wembed/utils.hpp"

using namespace wembed;

LLVMTypeRef LLVMFuncType(LLVMTypeRef pReturnType, std::initializer_list<LLVMTypeRef> pParamTypes) {
  return LLVMFunctionType(pReturnType, const_cast<LLVMTypeRef*>(pParamTypes.begin()), uint32_t(pParamTypes.size()), false);
}

LLVMTypeRef Void() { return LLVMVoidType(); }
LLVMTypeRef I32() { return LLVMInt32Type(); }
LLVMTypeRef I64() { return LLVMInt64Type(); }
LLVMTypeRef F32() { return LLVMFloatType(); }
LLVMTypeRef F64() { return LLVMDoubleType(); }

void test_v_v() {}
int32_t test_i32_v() { return 0; }
int32_t test_i32_i32(int32_t) { return 0; }
void test_v_i32(int32_t) {}
int32_t test_i32_i32i32(int32_t, int32_t) { return 0; }

TEST(hashtype, cvsllvm) {
  EXPECT_EQ(hash_type(Void()), hash_type(Void()));
  EXPECT_EQ(hash_type(I32()), hash_type(I32()));
  EXPECT_EQ(hash_type(I64()), hash_type(I64()));
  EXPECT_EQ(hash_type(F32()), hash_type(F32()));
  EXPECT_EQ(hash_type(F64()), hash_type(F64()));

  EXPECT_EQ(hash_type(Void()), hash_ctype<void>());
  EXPECT_EQ(hash_type(I32()), hash_ctype<int32_t>());
  EXPECT_EQ(hash_type(I64()), hash_ctype<int64_t>());
  EXPECT_EQ(hash_type(F32()), hash_ctype<float>());
  EXPECT_EQ(hash_type(F64()), hash_ctype<double>());

  EXPECT_EQ(hash_type(I32(), false), hash_ctype<int32_t>());
  EXPECT_EQ(hash_type(I32(), true), hash_ctype<const int32_t>());
  EXPECT_NE(hash_type(I32(), false), hash_ctype<const int32_t>());
  EXPECT_NE(hash_type(I32(), true), hash_ctype<int32_t>());

  EXPECT_NE(hash_type(LLVMFuncType(I32(), {I64(), I32()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());

  EXPECT_THROW(hash_ctype<bool>(), std::runtime_error);

  EXPECT_EQ(hash_fn_type(LLVMFuncType(Void(), {})), hash_fn_ctype<void()>());
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {})), hash_fn_ctype<int32_t()>());
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {I32()})), hash_fn_ctype<int32_t(int32_t)>());
  EXPECT_EQ(hash_fn_type(LLVMFuncType(Void(), {I32()})), hash_fn_ctype<void(int32_t)>());
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {I32(), I32()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());

  EXPECT_NE(hash_fn_type(LLVMFuncType(I32(), {I64(), I32()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());
  EXPECT_NE(hash_fn_type(LLVMFuncType(I32(), {I32(), I64()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());
  EXPECT_NE(hash_fn_type(LLVMFuncType(I64(), {I32(), I64()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());
  EXPECT_NE(hash_fn_type(LLVMFuncType(Void(), {I32(), I64()})), hash_fn_ctype<int32_t(int32_t, int32_t)>());

  EXPECT_EQ(hash_fn_type(LLVMFuncType(Void(), {})), hash_fn_ctype_ptr(test_v_v));
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {})), hash_fn_ctype_ptr(test_i32_v));
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {I32()})), hash_fn_ctype_ptr(test_i32_i32));
  EXPECT_EQ(hash_fn_type(LLVMFuncType(Void(), {I32()})), hash_fn_ctype_ptr(test_v_i32));
  EXPECT_EQ(hash_fn_type(LLVMFuncType(I32(), {I32(), I32()})), hash_fn_ctype_ptr(test_i32_i32i32));
}
