#include "wembed.hpp"

uint64_t wembed::hash_type(LLVMTypeRef pType, bool pConst) {
  uint64_t lSeed = 0;
  boost::hash_combine(lSeed, uint64_t(pType));
  boost::hash_combine(lSeed, pConst);
  return lSeed;
}

uint64_t wembed::hash_fn_type(LLVMTypeRef pType) {
  uint64_t lSeed = 1;
  boost::hash_combine(lSeed, hash_type(LLVMGetReturnType(pType)));
  size_t lArgCount = LLVMCountParamTypes(pType);
  std::vector<LLVMTypeRef> lArgTypes(lArgCount);
  LLVMGetParamTypes(pType, lArgTypes.data());
  for (const auto &lArgType : lArgTypes)
    boost::hash_combine(lSeed, hash_type(lArgType));
  return lSeed;
}

void wembed::llvm_init() {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
}
