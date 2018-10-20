#include "wembed.hpp"

namespace wembed {

  i32 intrinsics::memory_grow(uint8_t *pContext, uint32_t pDelta) {
    context *lCtx = (context*)pContext;
    assert(lCtx->mem() != nullptr);
    memory *lMem = lCtx->mem();
    resizable_limits lLimits = lMem->limits();
    size_t lPrevSize = lMem->size();
    size_t lNewSize = lPrevSize + pDelta;
    bool lReachedLimit = (lLimits.mFlags & 0x1) && lNewSize > lLimits.mMaximum;
    if (lNewSize > sMaxPages || lReachedLimit)
      return -1;

    try {
      if (pDelta > 0)
        lCtx->mem()->resize(lNewSize);
    }
    catch(const std::bad_array_new_length &e) {
      return -1;
    }

    return static_cast<i32>(lPrevSize);
  }

  i32 intrinsics::memory_size(uint8_t *pContext) {
    context *lCtx = (context*)pContext;
    assert(lCtx->mem() != nullptr);
    if (lCtx->mExternalMemory == nullptr && !lCtx->mSelfMemory.has_value())
      return 0;
    return static_cast<i32>(lCtx->mem()->size());
  }

  i32 intrinsics::table_size(uint8_t *pContext) {
    context *lCtx = (context*)pContext;
    assert(lCtx->tab() != nullptr);
    if (lCtx->mExternalTable == nullptr && !lCtx->mSelfTable.has_value())
      return 0;
    return static_cast<i32>(lCtx->tab()->size());
  }

  void intrinsics::throw_unlinkable(const char *pErrorString) {
    throw unlinkable_exception(pErrorString);
  }

  void intrinsics::throw_vm_exception(const char *pErrorString) {
    throw vm_runtime_exception(pErrorString);
  }

}  // namespace wembed
