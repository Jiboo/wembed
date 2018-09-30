#include "wembed.hpp"

#ifdef WEMBED_NATIVE_CODE_DUMP
#include <llvm-c/Disassembler.h>
#endif

#if defined(WEMBED_VERBOSE) or defined(WEMBED_NATIVE_CODE_DUMP)
#include <iostream>
#endif

namespace wembed {

  context::context(module &pModule, const std::string_view &pMappingsNamespace, mappings_t pMappings) : mModule(pModule) {
    char *lError = NULL;
    if(LLVMCreateExecutionEngineForModule(&mEngine, pModule.mModule, &lError) != 0) {
      std::string lErrorStr(lError);
      LLVMDisposeMessage(lError);
      throw std::runtime_error(lErrorStr);
    }

    if (mModule.mImports.size() > 1)
      throw unlinkable_exception("wembed don't accept multi module yet");
    else if (mModule.mImports.size() >= 1) {
      if (mModule.mImports.begin()->first != pMappingsNamespace)
        throw unlinkable_exception("imports from other module");
      auto lImports = mModule.mImports[std::string(pMappingsNamespace)];
      for (const auto &lImport : lImports) {
        if (value_name(lImport.second.mValue).empty()) // Note: was optimized away
          continue;
#ifdef WEMBED_VERBOSE
        std::cout << "import " << lImport.first << " aka " << value_name(lImport.second.mValue) << " of type "
                  << LLVMTypeOf(lImport.second.mValue) << std::endl;
#endif
        auto lMapping = pMappings.find(lImport.first);
        if (lMapping == pMappings.end())
          throw unlinkable_exception("can't import symbol: "s + lImport.first);
        LLVMAddGlobalMapping(mEngine, lImport.second.mValue, lMapping->second);
      }
    }

    constexpr auto lPageSize = 64 * 1024;
    if (pModule.mMemoryTypes.size()) {
      mMemoryLimits = pModule.mMemoryTypes[0].mLimits;
      mBaseMemory = LLVMGetNamedGlobal(pModule.mModule, "baseMemory");
      if (!pModule.mMemoryImport.mModule.empty()) {
        if (mBaseMemory != nullptr) {
          if (pMappingsNamespace != pModule.mMemoryImport.mModule)
            throw unlinkable_exception("importing memory from different module");
          if (pMappings.find(pModule.mMemoryImport.mField) == pMappings.end())
            throw unlinkable_exception("can't find memory import in mapping list");
          mExternalMemory = static_cast<virtual_mapping *>(pMappings[pModule.mMemoryImport.mField]);
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

    // Execute __wstart, init table/memory
    auto lStartPtr = LLVMGetFunctionAddress(mEngine, "__wstart");
    if (lStartPtr) {
      auto lFnPtr = reinterpret_cast<void (*)()>(lStartPtr);
      try {
        lFnPtr();
      }
      catch(const std::system_error &pError) {
        throw vm_runtime_exception(pError.what());
      }
    }

    // Replaces indices loaded in tables
    for (size_t i = 0; i < lTableCount; i++) {
      RuntimeTable &lContainer = mTables[i];
      table_type &lType = pModule.mTables[i].mType;
      for (size_t ptr = 0; ptr < lType.mLimits.mInitial; ptr++) {
        void *lIndice = lContainer.mPointers[ptr];
        LLVMValueRef lFunc = pModule.mFunctions[(size_t)lIndice];
        void *lAddress = LLVMGetPointerToGlobal(mEngine, lFunc);
        if (!lAddress)
          throw unlinkable_exception("func in table was opt out");
        auto lTypeHash = hash_fn_type(LLVMGetElementType(LLVMTypeOf(lFunc)));
  #ifdef WEMBED_VERBOSE
        std::cout << "remap table element for " << i << ", " << ptr << " aka "
                  << value_name(lFunc) << ": " << lAddress << ", type: " << lTypeHash << std::endl;
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
      uint8_t* lAddress = (uint8_t*)LLVMGetFunctionAddress2(mEngine, lName.data(), lName.size());
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
