

#pragma once

#include <functional>
#include <sstream>

#include <llvm-c/ExecutionEngine.h>

#include "lang.hpp"
#include "utils.hpp"
#include "module.hpp"
#include "intrinsics.hpp"

namespace wembed {

  class context {
  public:
    using mappings_t = std::unordered_map<std::string_view, void*>;

    context(module &pModule, mappings_t pMappings = {});
    ~context();

    void map_intrinsic(LLVMModuleRef pModule, const char *pName, void *pPtr);
    void map_global(LLVMValueRef pDest, void *pSource);

    template<typename TReturn, typename... TParams>
    std::function<TReturn(TParams...)> get_fn(const std::string &pName) {
#ifdef WEMBED_PREFIX_EXPORTED_FUNC
      std::stringstream lPrefixed;
      lPrefixed << "__wexport_" <<  pName;
      return get_fn_internal<TReturn, TParams...>(lPrefixed.str());
#else
      return get_fn_internal<TReturn, TParams...>(pName);
#endif
    };

#ifdef WEMBED_NATIVE_CODE_DUMP
    void dump_native();
#endif

  protected:
    friend i32 intrinsics::grow_memory(uint8_t *pContext, uint32_t pDelta);
    friend i32 intrinsics::current_memory(uint8_t *pContext);

    template<typename TReturn, typename... TParams>
    std::function<TReturn(TParams...)> get_fn_internal(const std::string &pName) {
      uint64_t lAddress = LLVMGetFunctionAddress(mEngine, pName.c_str());
      if (!lAddress)
        throw std::runtime_error("function not found");
      return reinterpret_cast<TReturn (*)(TParams...)>(lAddress);
    };

    module &mModule;
    resizable_limits mMemoryLimits;
    LLVMValueRef mBaseMemory;

    LLVMExecutionEngineRef mEngine = nullptr;
    std::optional<virtual_mapping> mMemory;
    std::vector<std::vector<void*>> mTables;
  };  // class context

}  // namespace wembed
