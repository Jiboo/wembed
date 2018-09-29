#include "wembed.hpp"

#ifdef WEMBED_NATIVE_CODE_DUMP
#include <llvm-c/Disassembler.h>
#endif

#if defined(WEMBED_VERBOSE) or defined(WEMBED_NATIVE_CODE_DUMP)
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

    // Probably need a blacklist here, for values like "baseMemory"
    for (const auto &lMapping : pMappings) {
      LLVMValueRef lValue = LLVMGetNamedFunction(pModule.mModule, lMapping.first.data());
      if (lValue != nullptr) {
        LLVMAddGlobalMapping(mEngine, lValue, lMapping.second);
#ifdef WEMBED_VERBOSE
        std::cout << "mapped " << lMapping.first << " function to " << lMapping.second << std::endl;
#endif
      }
      else {
        lValue = LLVMGetNamedGlobal(pModule.mModule, lMapping.first.data());
        if (lValue != nullptr) {
          LLVMAddGlobalMapping(mEngine, lValue, lMapping.second);
#ifdef WEMBED_VERBOSE
          std::cout << "mapped " << lMapping.first << " global to " << lMapping.second << std::endl;
#endif
        }
      }
    }

    constexpr auto lPageSize = 64 * 1024;
    if (pModule.mMemoryTypes.size()) {
      mMemoryLimits = pModule.mMemoryTypes[0].mLimits;
      mBaseMemory = LLVMGetNamedGlobal(pModule.mModule, "baseMemory");
      if (!pModule.mMemoryImport.empty()) {
        if (mBaseMemory != nullptr) {
          if (pMappings.find(pModule.mMemoryImport) == pMappings.end())
            throw std::runtime_error("can't find memory import in mapping list");
          mExternalMemory = static_cast<virtual_mapping *>(pMappings[pModule.mMemoryImport]);
          mExternalMemory->resize(std::max(mExternalMemory->size(), size_t(mMemoryLimits.mInitial * lPageSize)));
          map_global(mBaseMemory, mExternalMemory->data());
#ifdef WEMBED_VERBOSE
          std::cout << "bound external memory: " << mExternalMemory->size() << ", reserved "
                    << mExternalMemory->capacity() << " at " << (void *) mExternalMemory->data() << std::endl;
#endif
        }
      }
      else {
        auto lAllocatedSize = (mMemoryLimits.mFlags & 0x1)
                              ? (mMemoryLimits.mMaximum + 1) * lPageSize : 256UL * 1024 * 1024;
        mSelfMemory.emplace(mMemoryLimits.mInitial * lPageSize, lAllocatedSize);
        if (mBaseMemory != nullptr)
          map_global(mBaseMemory, mSelfMemory->data());
#ifdef WEMBED_VERBOSE
        std::cout << "initial memory size: " << mSelfMemory->size() << ", reserved "
                  << mSelfMemory->capacity() << " at " << (void *) mSelfMemory->data() << std::endl;
#endif
      }
    }

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

    map_intrinsic(pModule.mModule, "wembed.memory.grow.i32", (void*) &wembed::intrinsics::memory_grow);
    map_intrinsic(pModule.mModule, "wembed.memory.size.i32", (void*) &wembed::intrinsics::memory_size);
    map_intrinsic(pModule.mModule, "wembed.throw.unlinkable", (void*) &wembed::intrinsics::throw_unlinkable);
    map_intrinsic(pModule.mModule, "wembed.throw.vm_exception", (void*) &wembed::intrinsics::throw_vm_exception);

    const size_t lTableCount = pModule.mTables.size();
    mTables.resize(lTableCount);
    for (size_t i = 0; i < lTableCount; i++) {
      module::Table lTableSpec = pModule.mTables[i];
      mTables[i].mPointers.resize(lTableSpec.mType.mLimits.mInitial, nullptr);
      mTables[i].mTypes.resize(lTableSpec.mType.mLimits.mInitial, 0);
      map_global(lTableSpec.mPointers, mTables[i].mPointers.data());
      map_global(lTableSpec.mTypes, mTables[i].mTypes.data());
    }
    get_fn_internal<void>("__start")();
    // Replaces indices loaded in tables
    for (size_t i = 0; i < lTableCount; i++) {
      RuntimeTable &lContainer = mTables[i];
      table_type &lType = pModule.mTables[i].mType;
      for (size_t ptr = 0; ptr < lType.mLimits.mInitial; ptr++) {
        void *lIndice = lContainer.mPointers[ptr];
        LLVMValueRef lFunc = pModule.mFunctions[(size_t)lIndice];
        auto lName = value_name(lFunc);
        void *lAddress = (void*)LLVMGetFunctionAddress(mEngine, lName.data());
        if (!lAddress)
          throw std::runtime_error("func in table was opt out");
        auto lTypeHash = hash_fn_type(LLVMGetElementType(LLVMTypeOf(lFunc)));
  #ifdef WEMBED_VERBOSE
        std::cout << "remap table element for " << i << ", " << ptr << " aka "
                  << lName << ": " << lAddress << ", type: " << lTypeHash << std::endl;
  #endif
        lContainer.mPointers[ptr] = lAddress;
        lContainer.mTypes[ptr] = lTypeHash;
      }
    }
  #if defined(WEMBED_NATIVE_CODE_DUMP) && defined(WEMBED_VERBOSE)
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
      auto lName = value_name(mModule.mFunctions[lIndex]);
      if (lName == nullptr || lName.size() == 0)
        continue;
      uint8_t* lAddress = (uint8_t*)LLVMGetFunctionAddress(mEngine, lName.data());
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
    auto lName = value_name(pDest); // If no name, probably optimised out
    if (lName != nullptr && lName.size() > 0) {
      LLVMAddGlobalMapping(mEngine, pDest, pSource);
  #ifdef WEMBED_VERBOSE
      std::cout << "remap " << lName << " to " << pSource << std::endl;
  #endif
    }
  }

  virtual_mapping *context::mem() {
    return mExternalMemory ? mExternalMemory : &(mSelfMemory.value());
  }

}  // namespace wembed
