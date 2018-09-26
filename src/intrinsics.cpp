
#include <wembed/context.hpp>

#include "wembed.hpp"

namespace wembed {

  i32 intrinsics::memory_grow(uint8_t *pContext, uint32_t pDelta) {
    context *lCtx = (context*)pContext;
    assert(lCtx->mem() != nullptr);
    const size_t lPageBytes = 64 * 1024;
    size_t lPrevSize = lCtx->mem()->size() / lPageBytes;
    size_t lNewSize = lPrevSize + pDelta;
    bool lReachedLimit = (lCtx->mMemoryLimits.mFlags & 0x1)
                         && lNewSize > lCtx->mMemoryLimits.mMaximum;
    if (lNewSize > 0x10000 || lReachedLimit)
      return -1;

    try {
      if (pDelta > 0)
        lCtx->mem()->resize(lNewSize * lPageBytes);
    }
    catch(const std::bad_array_new_length &e) {
      return -1;
    }

    return static_cast<i32>(lPrevSize);
  }

  i32 intrinsics::memory_size(uint8_t *pContext) {
    context *lCtx = (context*)pContext;
    if (lCtx->mExternalMemory == nullptr && !lCtx->mSelfMemory.has_value())
      return 0;
    return static_cast<i32>(lCtx->mem()->size() / (64 * 1024));
  }

  void intrinsics::throw_unlinkable(const char *pErrorString) {
    throw unlinkable_exception(pErrorString);
  }

}  // namespace wembed
