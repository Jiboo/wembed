#include <iostream>

#include <boost/functional/hash.hpp>

#include "wembed/utils.hpp"

uint64_t wembed::hash_fn_type(LLVMTypeRef pType) {
  uint64_t lSeed = 0;
  boost::hash_combine(lSeed, uint64_t(LLVMGetReturnType(pType)));
  size_t lArgCount = LLVMCountParamTypes(pType);
  std::vector<LLVMTypeRef> lArgTypes(lArgCount);
  LLVMGetParamTypes(pType, lArgTypes.data());
  for (const auto &lArgType : lArgTypes)
    boost::hash_combine(lSeed, uint64_t(lArgType));
  return lSeed;
}
