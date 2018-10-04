#pragma once

#include <functional>
#include <sstream>

#ifdef WEMBED_VERBOSE
#include <iostream>
#endif

#include <llvm-c/ExecutionEngine.h>

#include "try_signal.hpp"

#include "lang.hpp"
#include "utils.hpp"
#include "module.hpp"
#include "intrinsics.hpp"

namespace wembed {

  // Runtime table/memory types
  class table {
  public:
    using pointer_t = void*;
    using types_t = uint64_t;

    table(const table_type &pType) : mType(pType), mPointers(pType.maximum() + 1), mTypes(pType.maximum() + 1) {
      mPointers.resize(pType.initial());
      mTypes.resize(pType.initial());
    }
    table(uint32_t pInitial, std::optional<uint32_t> pMaximum = {}) {
      mType.mLimits.mInitial = pInitial;
      if (pMaximum.has_value()) {
        mType.mLimits.mFlags |= 0x1;
        mType.mLimits.mMaximum = pMaximum.value();
      }
      mPointers.reserve(mType.maximum() + 1);
      mPointers.resize(mType.maximum() + 1);
      mTypes.reserve(mType.initial());
      mTypes.resize(mType.initial());
    }

    pointer_t* data_ptrs() { return mPointers.data(); }
    types_t* data_types() { return mTypes.data(); }

    size_t capacity() { return mType.maximum() - 1; }
    size_t size() { return mType.initial(); }
    resizable_limits limits() { return mType.mLimits; }

  protected:
    table_type mType;
    std::vector<pointer_t> mPointers;
    std::vector<types_t> mTypes;
  };

  class memory {
  public:
    memory(const memory_type &pType)
      : mType(pType), mImpl(pType.initial() * sPageSize, (pType.maximum() + 1) * sPageSize) {
    }
    memory(uint32_t pInitial, std::optional<uint32_t> pMaximum = {})
      : mImpl(pInitial * sPageSize, (pMaximum.has_value() ? (pMaximum.value() + 1) : sMaxPages) * sPageSize) {
      mType.mLimits.mInitial = pInitial;
      if (pMaximum.has_value()) {
        mType.mLimits.mFlags |= 0x1;
        mType.mLimits.mMaximum = pMaximum.value();
      }
    }

    uint8_t *data() { return mImpl.data(); }

    void resize(size_t pNewSize) { mImpl.resize(pNewSize * sPageSize); }
    size_t size() { return mImpl.size() / sPageSize; }
    size_t capacity() { return (mImpl.capacity() / sPageSize) - 1; }
    resizable_limits limits() { return mType.mLimits; }

  protected:
    memory_type mType;
    virtual_mapping mImpl;
  };

  struct resolve_result_t {
    void *mPointer;
    external_kind mKind;
    uint64_t mTypeHash;
  };
  using resolver_t = std::function<resolve_result_t(std::string_view)>;
  using resolvers_t = std::unordered_map<std::string_view, resolver_t>;

  class context {
  public:

    context(module &pModule, resolvers_t pResolvers = {});
    ~context();

    void map_intrinsic(LLVMModuleRef pModule, const char *pName, void *pPtr);
    void map_global(LLVMValueRef pDest, void *pSource);

    resolve_result_t get_export(const std::string &pName) {
      auto lValue = mModule.mExports.find(pName);
      if (lValue == mModule.mExports.end())
        return {nullptr, ek_global, 0};
      if (value_name(lValue->second.mValues[0]).empty())
        return {nullptr, ek_global, 0};
      resolve_result_t lResult;
      lResult.mKind = lValue->second.mKind;
      lResult.mTypeHash = lValue->second.mTypeHash;
      switch (lValue->second.mKind) {
        case ek_table: lResult.mPointer = tab(); break;
        case ek_memory: lResult.mPointer = mem(); break;
        default:
#if defined(WEMBED_VERBOSE) && 0
          std::cout << "Getting address for global " << pName << ", of type "
                    << LLVMPrintTypeToString(LLVMTypeOf(lValue->second.mValues[0]))
                    << ", " << LLVMPrintValueToString(lValue->second.mValues[0])
                    << std::endl;
#endif
          lResult.mPointer = LLVMGetPointerToGlobal(mEngine, lValue->second.mValues[0]);
      }
      return lResult;
    };

    template<typename TReturn, typename... TParams>
    std::function<TReturn(TParams...)> get_fn(const std::string &pName) {
      auto lResolve = get_export(pName);
      if (lResolve.mPointer == nullptr)
        throw std::runtime_error("function not found");
      if (lResolve.mKind != ek_function)
        throw std::runtime_error("function not found");
      auto lPointer = reinterpret_cast<TReturn (*)(TParams...)>(lResolve.mPointer);
      return [lPointer](TParams...pParams) -> TReturn {
        try {
          return sig::try_signal(lPointer, pParams...);
        }
        catch(const std::system_error &pError) {
          throw vm_runtime_exception(pError.what());
        }
      };
    };

    template<typename TGlobal>
    TGlobal *get_global(const std::string &pName) {
      auto lResolve = get_export(pName);
      if (lResolve.mPointer == nullptr)
        throw std::runtime_error("global not found");
      if (lResolve.mKind != ek_global)
        throw std::runtime_error("global not found");
      return reinterpret_cast<TGlobal*>(lResolve.mPointer);
    };

#ifdef WEMBED_NATIVE_CODE_DUMP
    void dump_native();
#endif

  protected:
    friend i32 intrinsics::memory_grow(uint8_t *pContext, uint32_t pDelta);
    friend i32 intrinsics::memory_size(uint8_t *pContext);
    friend i32 intrinsics::table_size(uint8_t *pContext);

    module &mModule;
    resizable_limits mMemoryLimits;
    LLVMValueRef mBaseMemory;

    LLVMExecutionEngineRef mEngine = nullptr;
    std::optional<memory> mSelfMemory;
    memory *mExternalMemory = nullptr;
    std::optional<table> mSelfTable;
    table *mExternalTable = nullptr;

  public:
    memory *mem();
    table *tab();
  };  // class context

}  // namespace wembed
