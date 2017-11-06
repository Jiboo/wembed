#include "wembed.hpp"

#ifdef WEMBED_NATIVE_CODE_DUMP
#include <llvm-c/Disassembler.h>
#endif

#ifdef VERBOSE
#include <iostream>
#endif

namespace wembed {

  context::context(module &pModule, mappings_t pMappings) : mModule(pModule) {
    char *lError = NULL;
    if(LLVMCreateExecutionEngineForModule(&mEngine, pModule.mModule, &lError) != 0) {
      std::string lErrorStr(lError);
      LLVMDisposeMessage(lError);
      throw std::runtime_error(lErrorStr);
    }

    for (const auto &lMapping : pMappings) {
      LLVMValueRef lValue = LLVMGetNamedFunction(pModule.mModule, lMapping.first.data());
      if (lValue != nullptr) {
        LLVMAddGlobalMapping(mEngine, lValue, lMapping.second);
      }
      else {
        lValue = LLVMGetNamedGlobal(pModule.mModule, lMapping.first.data());
        if (lValue != nullptr) {
          LLVMAddGlobalMapping(mEngine, lValue, lMapping.second);
        }
      }
    }

    if (pModule.mMemoryTypes.empty()) {
      mMemoryLimits.mInitial = 0;
      mMemoryLimits.mMaximum = 0;
      mMemoryLimits.mFlags = 0;
    }
    else {
      mMemoryLimits = pModule.mMemoryTypes[0].mLimits;
    }
    constexpr auto lPageSize = 64 * 1024;
    auto lAllocatedSize = (mMemoryLimits.mFlags & 0x1)
                          ? (mMemoryLimits.mMaximum + 1) * lPageSize : 256UL * 1024 * 1024;
    mMemory.emplace(mMemoryLimits.mInitial * lPageSize, lAllocatedSize);
    mBaseMemory = LLVMGetNamedGlobal(pModule.mModule, "baseMemory");
    if (mBaseMemory != nullptr)
      map_global(mBaseMemory, mMemory->data());
  #ifdef VERBOSE
    std::cout << "initial memory size: " << mMemory->size() << ", reserved "
              << mMemory->capacity() << " at " << (void*)mMemory->data() << std::endl;
  #endif

    LLVMValueRef lContextRef = LLVMGetNamedGlobal(pModule.mModule, "ctxRef");
    if (lContextRef != nullptr)
      map_global(lContextRef, this);

    map_intrinsic(pModule.mModule, "wembed.minnum.f32", (void*)&wembed::intrinsics::min<f32>);
    map_intrinsic(pModule.mModule, "wembed.minnum.f64", (void*)&wembed::intrinsics::min<f64>);
    map_intrinsic(pModule.mModule, "wembed.maxnum.f32", (void*)&wembed::intrinsics::max<f32>);
    map_intrinsic(pModule.mModule, "wembed.maxnum.f64", (void*)&wembed::intrinsics::max<f64>);
    map_intrinsic(pModule.mModule, "wembed.floor.f32", (void*)&wembed::intrinsics::floor<f32>);
    map_intrinsic(pModule.mModule, "wembed.floor.f64", (void*)&wembed::intrinsics::floor<f64>);
    map_intrinsic(pModule.mModule, "wembed.ceil.f32", (void*)&wembed::intrinsics::ceil<f32>);
    map_intrinsic(pModule.mModule, "wembed.ceil.f64", (void*)&wembed::intrinsics::ceil<f64>);
    map_intrinsic(pModule.mModule, "wembed.trunc.f32", (void*)&wembed::intrinsics::trunc<f32>);
    map_intrinsic(pModule.mModule, "wembed.trunc.f64", (void*)&wembed::intrinsics::trunc<f64>);
    map_intrinsic(pModule.mModule, "wembed.nearbyint.f32", (void*)&wembed::intrinsics::nearest<f32>);
    map_intrinsic(pModule.mModule, "wembed.nearbyint.f64", (void*)&wembed::intrinsics::nearest<f64>);
    map_intrinsic(pModule.mModule, "wembed.grow_memory.i32", (void*)&wembed::intrinsics::grow_memory);
    map_intrinsic(pModule.mModule, "wembed.current_memory.i32", (void*)&wembed::intrinsics::current_memory);

    const size_t lTableCount = pModule.mTables.size();
    mTables.resize(lTableCount);
    for (size_t i = 0; i < lTableCount; i++) {
      module::Table lTableSpec = pModule.mTables[i];
      mTables[i].resize(lTableSpec.mType.mLimits.mInitial);
      map_global(lTableSpec.mGlobal, mTables[i].data());
    }
    get_fn_internal<void>("__start")();
    // Replaces indices loaded in tables
    for (size_t i = 0; i < lTableCount; i++) {
      std::vector<void*> &lContainer = mTables[i];
      table_type &lType = pModule.mTables[i].mType;
      for (size_t ptr = 0; ptr < lType.mLimits.mInitial; ptr++) {
        void *lIndice = lContainer[ptr];
        const char *lName = LLVMGetValueName(pModule.mFunctions[(size_t)lIndice]);
        void *lAddress = (void*)LLVMGetFunctionAddress(mEngine, lName);
        if (!lAddress)
          throw std::runtime_error("func in table was opt out");
  #ifdef VERBOSE
        std::cout << "remap table element for " << i << ", " << ptr << " aka "
                  << lName << ": " << lAddress << std::endl;
  #endif
        lContainer[ptr] = lAddress;
      }
    }
  #if defined(WEMBED_NATIVE_CODE_DUMP) && defined(VERBOSE) && 0
    dump_native();
  #endif
  }

  context::~context() {
    if (mEngine) {
      char *lError = nullptr;
      LLVMRemoveModule(mEngine, mModule.mModule, &mModule.mModule, &lError);
      /*if (lError)
        throw std::runtime_error(lError);*/
      LLVMDisposeExecutionEngine(mEngine);
    }
  }

  #ifdef WEMBED_NATIVE_CODE_DUMP
  void context::dump_native() {
    LLVMDisasmContextRef dc = LLVMCreateDisasm(LLVMGetDefaultTargetTriple(), NULL, 0, NULL, NULL);
    if (dc == NULL)
      throw std::runtime_error("Could not create disassembler");
    LLVMSetDisasmOptions(dc, LLVMDisassembler_Option_PrintImmHex);

    char lOutput[2048];
    for (size_t lIndex = mModule.mImportFuncOffset; lIndex < mModule.mFunctions.size(); lIndex++) {
      const char *lName = LLVMGetValueName(mModule.mFunctions[lIndex]);
      if (lName == nullptr || strlen(lName) == 0)
        continue;
      uint8_t* lAddress = (uint8_t*)LLVMGetFunctionAddress(mEngine, lName);
      std::cout << "Function " << lName << " at " << (void*)lAddress << std::endl;

      uint8_t *lEnd = lAddress + 128;

      while (lAddress < lEnd) {
        size_t lSize = LLVMDisasmInstruction(dc, lAddress, lEnd - lAddress, (uint64_t)lAddress,
                                             lOutput, sizeof(lOutput));
        std::cout << (void*)lAddress << ": ";
        if (!lSize)
          std::cout << ".byte 0x" << std::hex << (int)*lAddress++ << std::dec << std::endl;
        else {
          lAddress += lSize;
          std::cout << lOutput << std::endl;
        }

        if (lSize == 1 && (*(lAddress - 1) == 0xc3))
          break;
      }
    }
    LLVMDisasmDispose(dc);
  }
  #endif

  void context::map_intrinsic(LLVMModuleRef pModule, const char *pName, void *pPtr) {
    LLVMValueRef lIntrinsic = LLVMGetNamedFunction(pModule, pName);
    if (lIntrinsic != nullptr) {
      map_global(lIntrinsic, pPtr);
    }
  }

  void context::map_global(LLVMValueRef pDest, void *pSource) {
    const char *lName = LLVMGetValueName(pDest); // If no name, probably optimised out
    if (lName != nullptr && strlen(lName) > 0) {
      LLVMAddGlobalMapping(mEngine, pDest, pSource);
  #ifdef VERBOSE
      std::cout << "remap " << lName << " to " << pSource << std::endl;
  #endif
    }
  }

  uint8_t *context::memory() {
    return mMemory->data();
  }

}  // namespace wembed
