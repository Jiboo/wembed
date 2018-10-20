#include <gtest/gtest.h>

#include "wembed.hpp"

static uint64_t orc_sym_resolver(const char *name, void *ctx) {
  auto lRef = (LLVMOrcJITStackRef *) ctx;
  uint64_t lRet;
  LLVMOrcGetSymbolAddress(*lRef, &lRet, name);
  return lRet;
}

TEST(llvm, orc_global_ptr) {
  LLVMModuleRef lMod = LLVMModuleCreateWithName("my_module");
  LLVMBuilderRef lBuilder = LLVMCreateBuilder();
  LLVMTypeRef lI32 = LLVMInt32Type();

  LLVMValueRef lGlobal = LLVMAddGlobal(lMod, LLVMInt32Type(), "global");
  LLVMSetInitializer(lGlobal, LLVMConstInt(LLVMInt32Type(), 142, true));
//LLVMSetLinkage(lGlobal, LLVMInternalLinkage);

  LLVMValueRef lStore = LLVMAddFunction(lMod, "store", LLVMFunctionType(LLVMVoidType(), &lI32, 1, false));
  LLVMBasicBlockRef lStoreEntry = LLVMAppendBasicBlock(lStore, "entry");
  LLVMPositionBuilderAtEnd(lBuilder, lStoreEntry);
  LLVMValueRef lStoreValue = LLVMGetParam(lStore, 0);
  LLVMBuildStore(lBuilder, lStoreValue, lGlobal);
  LLVMBuildRetVoid(lBuilder);

  LLVMValueRef lLoad = LLVMAddFunction(lMod, "load", LLVMFunctionType(lI32, nullptr, 0, false));
  LLVMBasicBlockRef lLoadEntry = LLVMAppendBasicBlock(lLoad, "entry");
  LLVMPositionBuilderAtEnd(lBuilder, lLoadEntry);
  LLVMValueRef lLoadValue = LLVMBuildLoad(lBuilder, lGlobal, "load");
  LLVMBuildRet(lBuilder, lLoadValue);


  char *lTriple = LLVMGetDefaultTargetTriple();
  LLVMTargetRef lTarget;
  ASSERT_FALSE(LLVMGetTargetFromTriple(lTriple, &lTarget, nullptr));
  ASSERT_TRUE(LLVMTargetHasJIT(lTarget));

  LLVMTargetMachineRef lTMachine =
      LLVMCreateTargetMachine(lTarget, lTriple, "", "",
                              LLVMCodeGenLevelDefault,
                              LLVMRelocDefault,
                              LLVMCodeModelJITDefault);
  ASSERT_NE(nullptr, lTMachine);
  LLVMDisposeMessage(lTriple);

  LLVMOrcJITStackRef lOrc = LLVMOrcCreateInstance(lTMachine);
  LLVMOrcModuleHandle lHandle;
  LLVMOrcAddEagerlyCompiledIR(lOrc, &lHandle, lMod, orc_sym_resolver, &lOrc);

  uint32_t *global = nullptr;
  void (*store)(uint32_t) = nullptr;
  uint32_t (*load)() = nullptr;

  LLVMErrorRef lError = nullptr;

  lError = LLVMOrcGetSymbolAddress(lOrc, (uint64_t *) &global, "global");
  if (lError) {
    std::cout << LLVMGetErrorMessage(lError) << std::endl;
  }

  lError = LLVMOrcGetSymbolAddress(lOrc, (uint64_t *) &store, "store");
  if (lError) {
    std::cout << LLVMGetErrorMessage(lError) << std::endl;
  }

  lError = LLVMOrcGetSymbolAddress(lOrc, (uint64_t *) &load, "load");
  if (lError) {
    std::cout << LLVMGetErrorMessage(lError) << std::endl;
  }

  try {
    EXPECT_EQ(142, *global);
    EXPECT_EQ(142, load());
  }
  catch (...) {
    std::cout << LLVMOrcGetErrorMsg(lOrc) << std::endl;
  }

  store(1);
  EXPECT_EQ(1, *global); // Failure, Expected: 1, To be equal to: *global, Which is: 142
  EXPECT_EQ(1, load());

  store(2);
  EXPECT_EQ(2, load());
  EXPECT_EQ(2, *global); // Failure, Expected: 2, To be equal to: *global, Which is: 142

  *global = 3;
  EXPECT_EQ(3, *global);
  EXPECT_EQ(3, load()); // Failure, Expected: 3, To be equal to: load(), Which is: 2

  *global = 4;
  EXPECT_EQ(4, load()); // Failure, Expected: 4, To be equal to: load(), Which is: 2
  EXPECT_EQ(4, *global);

  LLVMDisposeBuilder(lBuilder);
  LLVMOrcDisposeInstance(lOrc);
}
