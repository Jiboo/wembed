#include <iostream>
#include <iomanip>

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

std::ostream &wembed::operator<<(std::ostream &pOS, const hrclock::duration &pDur) {
  auto lNano = pDur.count();
  if (lNano < 1000) {
    return pOS << lNano << "ns";
  }
  if (lNano < 1'000'000) {
    return pOS << lNano / 1000.0 << "µs";
  }
  if (lNano < 1'000'000'000) {
    return pOS << lNano / 1'000'000.0 << "ms";
  }
  return pOS << lNano / 1'000'000'000.0 << "s";
}

void wembed::profile_step(const char *pName) {
#ifdef WEMBED_PROFILE
  static bool sInitialized = false;
  static hrclock::time_point sFirst;
  static hrclock::time_point sLast;

  auto lNow = hrclock::now();
  if (!sInitialized) {
    sFirst = sLast = lNow;
    sInitialized = true;
  }

  std::ios_base::fmtflags lIOFlags(std::cout.flags());
  std::chrono::duration<double, std::milli> lElapsed = lNow - sFirst;
  std::cout << "[" << std::fixed << std::setw(12) << std::setprecision(6) << lElapsed.count();
  std::cout.flags(lIOFlags);
  std::cout << "] " << pName << " (diff: " << (lNow - sLast) << ")" << std::endl;

  sLast = lNow;
#endif
}
