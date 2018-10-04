#if defined(WEMBED_VERBOSE) or defined(WEMBED_NATIVE_CODE_DUMP)
#include <iostream>
#endif

#ifdef WEMBED_NATIVE_CODE_DUMP
#include <llvm-c/Disassembler.h>
#endif

#include "try_signal.hpp"

#include "wembed.hpp"


namespace wembed {

  context::context(module &pModule, resolvers_t pResolver) : mModule(pModule) {
    char *lError = NULL;
    if(LLVMCreateExecutionEngineForModule(&mEngine, pModule.mModule, &lError) != 0) {
      std::string lErrorStr(lError);
      LLVMDisposeMessage(lError);
      throw std::runtime_error(lErrorStr);
    }

    LLVMValueRef lContextRef = LLVMGetNamedGlobal(pModule.mModule, "ctxRef");
    if (lContextRef != nullptr)
      map_global(lContextRef, this);

    map_intrinsic(pModule.mModule, "wembed.memory.grow.i32", (void*) &wembed::intrinsics::memory_grow);
    map_intrinsic(pModule.mModule, "wembed.memory.size.i32", (void*) &wembed::intrinsics::memory_size);
    map_intrinsic(pModule.mModule, "wembed.table.size.i32", (void*) &wembed::intrinsics::table_size);
    map_intrinsic(pModule.mModule, "wembed.throw.unlinkable", (void*) &wembed::intrinsics::throw_unlinkable);
    map_intrinsic(pModule.mModule, "wembed.throw.vm_exception", (void*) &wembed::intrinsics::throw_vm_exception);

    if (mModule.mImports.size() >= 1) {
      for (const auto &lImportModule : mModule.mImports) {
        auto lFields = lImportModule.second;
        auto lResolverSearch = pResolver.find(lImportModule.first);
        if (lResolverSearch == pResolver.end())
          throw unlinkable_exception("import from unknown module: "s + std::string(lImportModule.first));
        for (const auto &lImportField : lFields) {
          auto lResolverResult = lResolverSearch->second(lImportField.first);
          if (lResolverResult.mPointer == nullptr || lResolverResult.mKind != lImportField.second.mKind || lResolverResult.mTypeHash != lImportField.second.mTypeHash)
            throw unlinkable_exception(
                "can't import symbol: "s + lImportField.first + " in module " + std::string(lImportModule.first));
          if (value_name(lImportField.second.mValues[0]).empty())
            continue;
#ifdef WEMBED_VERBOSE
          std::cout << "import " << lImportModule.first << "::" << lImportField.first << ", aka " << value_name(lImportField.second.mValues[0]) << " of type "
                    << LLVMPrintTypeToString(LLVMTypeOf(lImportField.second.mValues[0])) << ", at " << lResolverResult.mPointer << std::endl;
#endif
          LLVMAddGlobalMapping(mEngine, lImportField.second.mValues[0], lResolverResult.mPointer);
        }
      }
    }

    if (pModule.mMemoryTypes.size()) {
      auto &lMemory = pModule.mMemoryTypes[0];
      mMemoryLimits = lMemory.mLimits;
      mBaseMemory = LLVMGetNamedGlobal(pModule.mModule, "baseMemory");
      if (!pModule.mMemoryImport.mModule.empty()) {
        auto &lImport = pModule.mMemoryImport;
        if (mBaseMemory != nullptr) {
          auto lResolverSearch = pResolver.find(lImport.mModule);
          if (lResolverSearch == pResolver.end())
            throw unlinkable_exception("import memory from unknown module: "s + std::string(lImport.mModule));
          auto lResolverResult = lResolverSearch->second(lImport.mField);
          if (lResolverResult.mPointer == nullptr)
            throw unlinkable_exception("can't memory symbol: "s + lImport.mField + " in module " + std::string(lImport.mModule));
          mExternalMemory = static_cast<memory*>(lResolverResult.mPointer);
          if (lMemory.initial() > mExternalMemory->size())
            throw unlinkable_exception("can't import memory: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too small");
          if (lMemory.mLimits.mFlags & 0x1 && lMemory.maximum() < mExternalMemory->capacity())
            throw unlinkable_exception("can't import memory: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too large");
          map_global(mBaseMemory, mExternalMemory->data());
#ifdef WEMBED_VERBOSE
          std::cout << "bound external memory: " << mExternalMemory->size() << ", reserved "
                    << mExternalMemory->capacity() << " at " << (void *) mExternalMemory->data() << std::endl;
#endif
        }
      }
      else {
        mSelfMemory.emplace(lMemory);
        if (mBaseMemory != nullptr)
          map_global(mBaseMemory, mSelfMemory->data());
#ifdef WEMBED_VERBOSE
        std::cout << "initial memory size: " << mSelfMemory->size() << ", reserved "
                  << mSelfMemory->capacity() << " at " << (void *) mSelfMemory->data() << std::endl;
#endif
      }
    }

    const auto lTableCount = pModule.mTables.size();
    if (lTableCount > 1)
      throw unlinkable_exception("multiple tables not supproted");
    if (lTableCount > 0) {
      if (mModule.mTableImport.mModule.empty()) {
        module::Table lTableSpec = pModule.mTables[0];
        mSelfTable.emplace(lTableSpec.mType);
        map_global(lTableSpec.mPointers, mSelfTable->data_ptrs());
        map_global(lTableSpec.mTypes, mSelfTable->data_types());
#ifdef WEMBED_VERBOSE
        std::cout << "initial table size: " << mSelfTable->size() << ", at " << (void *) mSelfTable->data_ptrs() << std::endl;
#endif
      }
      else {
        auto &lImport = pModule.mTableImport;
        auto &lTable = mModule.mTables[0];
        auto lResolverSearch = pResolver.find(lImport.mModule);
        if (lResolverSearch == pResolver.end())
          throw unlinkable_exception("import table from unknown module: "s + std::string(lImport.mModule));
        auto lResolverResult = lResolverSearch->second(lImport.mField);
        if (lResolverResult.mPointer == nullptr)
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule));
        mExternalTable = static_cast<table*>(lResolverResult.mPointer);
        if (lTable.mType.initial() > mExternalTable->size())
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too small");
        if (lTable.mType.mLimits.mFlags & 0x1 && lTable.mType.maximum() < mExternalTable->capacity())
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too large");
        map_global(lTable.mPointers, mExternalTable->data_ptrs());
        map_global(lTable.mTypes, mExternalTable->data_types());
#ifdef WEMBED_VERBOSE
        std::cout << "bound external table: " << mExternalTable->size() << ", at " << (void *) mExternalTable->data_ptrs() << std::endl;
#endif
      }
    }

#ifdef WEMBED_VERBOSE
    for (const auto &lExportField : mModule.mExports) {
      if (value_name(lExportField.second.mValues[0]).empty())
        continue;
      auto lExport = get_export(lExportField.first);
      std::cout << "export " << lExportField.first << ", aka " << value_name(lExportField.second.mValues[0])
                << " of type " << LLVMPrintTypeToString(LLVMTypeOf(lExportField.second.mValues[0])) << ", hash "
                << lExport.mTypeHash << ", at " << lExport.mPointer << std::endl;
    }
#endif

    // Execute __wstart, init table/memory
    auto lStartPtr = LLVMGetFunctionAddress(mEngine, "__wstart");
    if (lStartPtr) {
      auto lFnPtr = reinterpret_cast<void (*)()>(lStartPtr);
      try {
        sig::try_signal(lFnPtr);
      }
      catch(const std::system_error &pError) {
        throw vm_runtime_exception(pError.what());
      }
    }

    // Replaces indices loaded in tables
    for (size_t i = 0; i < lTableCount; i++) {
      table *lTable = tab();
      table_type &lType = pModule.mTables[i].mType;
      for (size_t lIndex = 0; lIndex < lType.mLimits.mInitial; lIndex++) {
        void *lIndice = lTable->data_ptrs()[lIndex];
        if ((size_t)lIndice == 0 || (size_t)lIndice > pModule.mFunctions.size())
          continue;
        LLVMValueRef lFunc = pModule.mFunctions[(size_t)lIndice - 1];
        void *lAddress = LLVMGetPointerToGlobal(mEngine, lFunc);
        if (!lAddress)
          throw unlinkable_exception("func in table was opt out");
        auto lTypeHash = hash_fn_type(LLVMGetElementType(LLVMTypeOf(lFunc)));
  #ifdef WEMBED_VERBOSE
        std::cout << "remap table element for " << i << ", " << lIndex << " aka "
                  << value_name(lFunc) << ": " << lAddress << ", type: " << lTypeHash << std::endl;
  #endif
        lTable->data_ptrs()[lIndex] = lAddress;
        lTable->data_types()[lIndex] = lTypeHash;
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
    if (!value_name(pDest).empty()) {
      LLVMAddGlobalMapping(mEngine, pDest, pSource);
  #ifdef WEMBED_VERBOSE
      std::cout << "remap " << value_name(pDest) << " to " << pSource << std::endl;
  #endif
    }
  }

  memory *context::mem() {
    return mExternalMemory ? mExternalMemory : &(mSelfMemory.value());
  }

  table *context::tab() {
    return mExternalTable ? mExternalTable : &(mSelfTable.value());
  }

}  // namespace wembed
