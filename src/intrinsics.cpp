#include "wembed.hpp"

namespace wembed {

  i32 intrinsics::grow_memory(uint8_t *pContext, uint32_t pDelta) {
    context *lCtx = (context*)pContext;

    const size_t lPageBytes = 64 * 1024;
    size_t lPrevSize = lCtx->mMemory->size() / lPageBytes;
    size_t lNewSize = lPrevSize + pDelta;
    bool lReachedLimit = (lCtx->mMemoryLimits.mFlags & 0x1)
                         && lNewSize > lCtx->mMemoryLimits.mMaximum;
    if (lNewSize > 0x10000 || lReachedLimit)
      return -1;

    try {
      if (pDelta > 0)
        lCtx->mMemory->resize(lNewSize * lPageBytes);
    }
    catch(const std::bad_array_new_length &e) {
      return -1;
    }

    return static_cast<i32>(lPrevSize);
  }

  i32 intrinsics::current_memory(uint8_t *pContext) {
    context *lCtx = (context*)pContext;
    return static_cast<i32>(lCtx->mMemory->size() / (64 * 1024));
  }

}  // namespace wembed
