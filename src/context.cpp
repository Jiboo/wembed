#include <llvm-c/ExecutionEngine.h>

#include "try_signal.hpp"

#include "wembed.hpp"

#ifdef WEMBED_VERBOSE
#include <fstream>
#include <iostream>
#endif

namespace wembed {

  uint64_t orc_sym_resolver(const char *pName, void *pCtx) {
    auto lRef = (context*)pCtx;

    auto lPredefined = lRef->mSymbols.find(pName);
    if (lPredefined != lRef->mSymbols.end()) {
#ifdef WEMBED_VERBOSE
      std::cout << "Resolved builtin " << pName << " to " << std::hex << lPredefined->second << std::dec << std::endl;
#endif
      return (uint64_t) lPredefined->second;
    }

    uint64_t lRet;
    LLVMOrcGetSymbolAddress(lRef->mEngine, &lRet, pName);

#ifdef WEMBED_VERBOSE
    std::cout << "Resolved " << pName << " to " << std::hex << lRet << std::dec << std::endl;
#endif

    return lRet;
  }

  context::context(module &pModule, const resolvers_t &pResolver, void *pUserData)
    : mModule(pModule), mUserData(pUserData) {
    profile_step("  context/ctr");

    char *lTriple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef lTarget;
    if (LLVMGetTargetFromTriple(lTriple, &lTarget, nullptr))
      throw std::runtime_error("can't get triple for host");
    if (!LLVMTargetHasJIT(lTarget))
      throw std::runtime_error("can't jit on this host");

    LLVMCodeGenOptLevel lOptLevel;
    if (pModule.mOptLevel > 3)
      lOptLevel = LLVMCodeGenLevelAggressive;
    else {
      switch (pModule.mOptLevel) {
        default:
        case 0: lOptLevel = LLVMCodeGenLevelNone; break;
        case 1: lOptLevel = LLVMCodeGenLevelLess; break;
        case 2: lOptLevel = LLVMCodeGenLevelDefault; break;
        case 3: lOptLevel = LLVMCodeGenLevelAggressive; break;
      }
    }

    LLVMTargetMachineRef lTMachine = LLVMCreateTargetMachine(lTarget, lTriple, "", "", lOptLevel, LLVMRelocStatic, LLVMCodeModelJITDefault);
    assert(lTMachine != nullptr);
    LLVMDisposeMessage(lTriple);

    mSymbols.emplace("wembed.memory.grow", (void*)&wembed::intrinsics::memory_grow);
    mSymbols.emplace("wembed.memory.size", (void*)&wembed::intrinsics::memory_size);
    mSymbols.emplace("wembed.table.size", (void*)&wembed::intrinsics::table_size);
    mSymbols.emplace("wembed.throw.unlinkable", (void*)&wembed::intrinsics::throw_unlinkable);
    mSymbols.emplace("wembed.throw.vm_exception", (void*)&wembed::intrinsics::throw_vm_exception);
    mSymbols.emplace("wembed.ctxRef", (void*)this);

    float(*sCeil32)(float) = &ceilf;
    double(*sCeil64)(double) = &ceil;
    float(*sFloor32)(float) = &floorf;
    double(*sFloor64)(double) = &floor;
    float(*sTrunc32)(float) = &truncf;
    double(*sTrunc64)(double) = &trunc;
    float(*sNearByInt32)(float) = &nearbyintf;
    double(*sNearByInt64)(double) = &nearbyint;

    mSymbols.emplace("floorf", (void*)sFloor32);
    mSymbols.emplace("floor", (void*)sFloor64);
    mSymbols.emplace("truncf", (void*)sTrunc32);
    mSymbols.emplace("trunc", (void*)sTrunc64);
    mSymbols.emplace("nearbyintf", (void*)sNearByInt32);
    mSymbols.emplace("nearbyint", (void*)sNearByInt64);
    mSymbols.emplace("ceilf", (void*)sCeil32);
    mSymbols.emplace("ceil", (void*)sCeil64);

    mSymbols.emplace("memcpy", (void*)&memcpy);

    profile_step("  context/prep");

    if (mModule.mImports.size() >= 1) {
      for (const auto &lImportModule : mModule.mImports) {
        auto lFields = lImportModule.second;
        auto lResolverSearch = pResolver.find(lImportModule.first);
        if (lResolverSearch == pResolver.end())
          throw unlinkable_exception("import from unknown module: "s + std::string(lImportModule.first));
        for (const auto &lImportField : lFields) {
          auto lResolverResult = lResolverSearch->second(*this, lImportField.first);
          if (lResolverResult.mPointer == nullptr || lResolverResult.mKind != lImportField.second.mKind || lResolverResult.mTypeHash != lImportField.second.mTypeHash)
            throw unlinkable_exception(
                "can't import symbol: "s + lImportField.first + " in module " + std::string(lImportModule.first));
#ifdef WEMBED_VERBOSE
          std::cout << "import " << lImportModule.first << "::" << lImportField.first << ", aka " << lImportField.second.mValueNames[0] /*<< " of type "
                    << LLVMPrintTypeToString(LLVMTypeOf(lImportField.second.mValues[0]))*/ << ", at " << lResolverResult.mPointer << std::endl;
#endif
          switch(lResolverResult.mKind) {
            // mem/tab imports are handled below
            case ek_memory: break;
            case ek_table: break;
            default:
              mSymbols.emplace(lImportField.second.mValueNames[0], lResolverResult.mPointer);
          }
        }
      }
    }
    profile_step("  context/imports");

    if (pModule.mMemoryTypes.size()) {
      auto &lMemory = pModule.mMemoryTypes[0];
      if (!pModule.mMemoryImport.mModule.empty()) {
        auto &lImport = pModule.mMemoryImport;
        auto lResolverSearch = pResolver.find(lImport.mModule);
        if (lResolverSearch == pResolver.end())
          throw unlinkable_exception("import memory from unknown module: "s + std::string(lImport.mModule));
        auto lResolverResult = lResolverSearch->second(*this, lImport.mField);
        if (lResolverResult.mPointer == nullptr)
          throw unlinkable_exception("can't memory symbol: "s + lImport.mField + " in module " + std::string(lImport.mModule));
        mExternalMemory = static_cast<memory*>(lResolverResult.mPointer);
        if (lMemory.initial() > mExternalMemory->size())
          throw unlinkable_exception("can't import memory: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too small");
        if (lMemory.mLimits.mFlags & 0x1 && lMemory.maximum() < mExternalMemory->capacity())
          throw unlinkable_exception("can't import memory: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too large");
#ifdef WEMBED_VERBOSE
        std::cout << "bound external memory: " << mExternalMemory->size() << ", reserved "
                  << mExternalMemory->capacity() << " at " << (void *) mExternalMemory->data() << std::endl;
#endif
      }
      else {
        mSelfMemory.emplace(lMemory);
#ifdef WEMBED_VERBOSE
        std::cout << "initial memory size: " << mSelfMemory->size() << ", reserved "
                  << mSelfMemory->capacity() << " at " << (void *) mSelfMemory->data() << std::endl;
#endif
      }

      mSymbols.emplace("wembed.baseMemory", (void*)mem()->data());
    }
    profile_step("  context/memories");

    const auto lTableCount = pModule.mTables.size();
    if (lTableCount > 1)
      throw unlinkable_exception("multiple tables not supproted");
    if (lTableCount > 0) {
      if (mModule.mTableImport.mModule.empty()) {
        module::Table lTableSpec = pModule.mTables[0];
        mSelfTable.emplace(lTableSpec.mType);
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
        auto lResolverResult = lResolverSearch->second(*this, lImport.mField);
        if (lResolverResult.mPointer == nullptr)
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule));
        mExternalTable = static_cast<table*>(lResolverResult.mPointer);
        if (lTable.mType.initial() > mExternalTable->size())
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too small");
        if (lTable.mType.mLimits.mFlags & 0x1 && lTable.mType.maximum() < mExternalTable->capacity())
          throw unlinkable_exception("can't import table: "s + lImport.mField + " in module " + std::string(lImport.mModule) + ", too large");
#ifdef WEMBED_VERBOSE
        std::cout << "bound external table: " << mExternalTable->size() << ", at " << (void *) mExternalTable->data_ptrs() << std::endl;
#endif
      }

      mSymbols.emplace("wembed.tablePtrs", (void*)tab()->data_ptrs());
      mSymbols.emplace("wembed.tableTypes", (void*)tab()->data_types());
    }
    profile_step("  context/tables");

    mEngine = LLVMOrcCreateInstance(lTMachine);
#ifdef WEMBED_GDB_DEBUG
    LLVMOrcRegisterJITEventListener(mEngine, LLVMCreateGDBRegistrationListener());
#endif
    profile_step("  context/orc init");

    LLVMOrcAddEagerlyCompiledIR(mEngine, &mHandle, pModule.mModule, orc_sym_resolver, this);
    LLVMDisposeTargetMachine(lTMachine); // NOTE JB: LLVMOrcCreateInstance documentation says that TargetMachine shouldn't be freed
    profile_step("  context/jit");

#if defined(WEMBED_VERBOSE) && 0
    std::ifstream lMemSegments("/proc/self/maps");
    if (lMemSegments.is_open())
      std::cout << lMemSegments.rdbuf();
#endif

    // Execute __wstart, init table/memory
    auto lStartPtr = get_pointer("wembed.start");
    if (lStartPtr) {
      auto lFnPtr = reinterpret_cast<void (*)()>(lStartPtr);
      try {
        sig::try_signal(lFnPtr);
      }
      catch(const unlinkable_exception &pError) {
        // invalid module init, rethrow
        throw pError;
      }
      catch(const vm_runtime_exception &pError) {
        // invalid module, although, carry on with tables init
        replace_tables_indices();
        throw pError;
      }
      catch(const std::system_error &pError) {
        throw vm_runtime_exception(pError.what());
      }
    }
    profile_step("  context/start");
    replace_tables_indices();
  }

  context::~context() {
    if (mEngine) {
      if (mHandle)
        LLVMOrcRemoveModule(mEngine, mHandle);
      LLVMOrcDisposeInstance(mEngine);
    }
  }

  void context::replace_tables_indices() {
    const auto lTableCount = mModule.mTables.size();
    for (size_t i = 0; i < lTableCount; i++) {
      table *lTable = tab();
      table_type &lType = mModule.mTables[i].mType;
      for (size_t lIndex = 0; lIndex < lType.mLimits.mInitial; lIndex++) {
        void *lIndice = lTable->data_ptrs()[lIndex];
        if ((size_t)lIndice == 0 || (size_t)lIndice > mModule.mFunctions.size())
          continue;
        const auto &lFuncDef = mModule.mFunctions[(size_t)lIndice - 1];
        void *lAddress = get_pointer(lFuncDef.mName.c_str());
        if (!lAddress)
          throw unlinkable_exception("func in table was opt out");
        auto lTypeHash = lFuncDef.mType;
#ifdef WEMBED_VERBOSE
        std::cout << "remap table element for " << i << ", " << lIndex << " aka "
                  << lFuncDef.mName << ": " << lAddress << ", type: " << lTypeHash << std::endl;
#endif
        lTable->data_ptrs()[lIndex] = lAddress;
        lTable->data_types()[lIndex] = lTypeHash;
      }
    }
    profile_step("  context/table replace");
  }

  memory *context::mem() {
    return mExternalMemory ? mExternalMemory : &(mSelfMemory.value());
  }

  table *context::tab() {
    return mExternalTable ? mExternalTable : &(mSelfTable.value());
  }

}  // namespace wembed
