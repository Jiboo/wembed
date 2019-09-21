#pragma once

#include <functional>
#include <sstream>

#ifdef WEMBED_VERBOSE
#include <iostream>
#endif

#include <llvm-c/OrcBindings.h>

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
      : mType(pType), mImpl(pType.initial() * sPageSize) {
    }
    memory(uint32_t pInitial, std::optional<uint32_t> pMaximum = {})
      : mImpl(pInitial * sPageSize) {
      mType.mLimits.mInitial = pInitial;
      if (pMaximum.has_value()) {
        mType.mLimits.mFlags |= 0x1;
        mType.mLimits.mMaximum = pMaximum.value();
      }
    }

    uint8_t *data() { return mImpl.data(); }

    void resize(size_t pNewSize) { mImpl.resize(pNewSize * sPageSize); }
    size_t size() { return mImpl.size() / sPageSize; }
    size_t capacity() { return mType.maximum(); }
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

  template<typename T>
  resolve_result_t expose_glob(T* pAddr) {
    return {(void*) pAddr, wembed::ek_global, wembed::hash_ctype<T>()};
  }

  template<typename T>
  resolve_result_t expose_cglob(T* pAddr) {
    return {(void*) pAddr, wembed::ek_global, wembed::hash_ctype<const T>()};
  }

  template<typename TReturn, typename...TParams>
  resolve_result_t expose_func(TReturn (*pAddr)(TParams...)) {
    return {(void*) pAddr, wembed::ek_function, wembed::hash_fn_ctype_ptr(pAddr)};
  }

  inline resolve_result_t expose_table(table* pTable) {
    return {(void*) pTable, wembed::ek_table, WEMBED_HASH_TABLE};
  }

  inline resolve_result_t expose_memory(memory* pMem) {
    return {(void*) pMem, wembed::ek_memory, WEMBED_HASH_MEMORY};
  }

  class context {
  public:
    friend uint64_t orc_sym_resolver(const char *pName, void *pCtx);

    context(module &pModule, const resolvers_t &pResolvers = {});
    ~context();

    template<typename T = void>
    T *get_pointer(const char *pName) {
      auto lPredefined = mSymbols.find(pName);
      if (lPredefined != mSymbols.end())
        return (T*)lPredefined->second;

      uint64_t lAddr;
      LLVMErrorRef lError = LLVMOrcGetSymbolAddress(mEngine, &lAddr, pName);
      if (lError) {
        char *lErrorMsg = LLVMGetErrorMessage(lError);
        std::string lErrorStr(lErrorMsg);
        LLVMDisposeErrorMessage(lErrorMsg);
        throw std::runtime_error(lErrorStr);
      }
      return reinterpret_cast<T*>(lAddr);
    }

    resolve_result_t get_export(const std::string &pName) {
      auto lValue = mModule.mExports.find(pName);
      if (lValue == mModule.mExports.end())
        return {nullptr, ek_global, 0};
      resolve_result_t lResult;
      lResult.mKind = lValue->second.mKind;
      lResult.mTypeHash = lValue->second.mTypeHash;
      switch (lValue->second.mKind) {
        case ek_table: lResult.mPointer = tab(); break;
        case ek_memory: lResult.mPointer = mem(); break;
        default: lResult.mPointer = get_pointer(lValue->second.mValueNames[0].c_str());
      }
      return lResult;
    }

    template <typename TFunc>
    struct __get_fn;

    template <typename TReturn, typename...TParams>
    struct __get_fn<TReturn (TParams...)> {
      std::function<TReturn(TParams...)> operator()(context *pThis, const std::string &pName) {
        auto lResolve = pThis->get_export(pName);
        if (lResolve.mPointer == nullptr)
          throw std::runtime_error(std::string("function not found: ") + pName);
        if (lResolve.mKind != ek_function)
          throw std::runtime_error(std::string(pName) + " is not a function");
        auto lPointer = reinterpret_cast<TReturn (*)(TParams...)>(lResolve.mPointer);
        return [lPointer](TParams...pParams) -> TReturn {
          try {
            return sig::try_signal(lPointer, pParams...);
          }
          catch(const std::system_error &pError) {
            throw vm_runtime_exception(pError.what());
          }
        };
      }
    };

    template<typename TReturn, typename... TParams>
    std::function<TReturn(TParams...)> get_fn(const std::string &pName) {
      return __get_fn<TReturn (TParams...)>{}(this, pName);
    }

    template<typename T>
    std::function<T> get_fn(const std::string &pName) {
      return __get_fn<T>{}(this, pName);
    }

    template<typename TGlobal>
    TGlobal *get_global(const std::string &pName) {
      auto lResolve = get_export(pName);
      if (lResolve.mPointer == nullptr)
        throw std::runtime_error("global not found");
      if (lResolve.mKind != ek_global)
        throw std::runtime_error("global not found");
      return reinterpret_cast<TGlobal*>(lResolve.mPointer);
    }

  protected:
    friend i32 intrinsics::memory_grow(uint8_t *pContext, uint32_t pDelta);
    friend i32 intrinsics::memory_size(uint8_t *pContext);
    friend i32 intrinsics::table_size(uint8_t *pContext);

    module &mModule;

    LLVMOrcJITStackRef mEngine = nullptr;
    LLVMOrcModuleHandle mHandle = 0;

    std::optional<memory> mSelfMemory;
    memory *mExternalMemory = nullptr;
    std::optional<table> mSelfTable;
    table *mExternalTable = nullptr;

    std::unordered_map<std::string, void*> mSymbols;

    void replace_tables_indices();

  public:
    memory *mem();
    table *tab();
  };  // class context

}  // namespace wembed
