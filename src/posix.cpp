#include "wembed/utils.hpp"

#include <new>

#include <sys/mman.h>

namespace wembed {

  virtual_mapping::virtual_mapping(size_t pInitialSize, size_t pMaximumSize)
      : mCurSize(0), mAllocatedSize(pMaximumSize) {
    mAddress = (uint8_t*)mmap(nullptr, pMaximumSize, 0, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mAddress == MAP_FAILED)
      throw std::bad_alloc();
    if (pInitialSize > 0)
      resize(pInitialSize);
  }

  virtual_mapping::~virtual_mapping() {
    if (mAddress != nullptr)
      munmap(mAddress, mAllocatedSize);
  }

  void virtual_mapping::resize(size_t pNewSize) {
    if (pNewSize > mAllocatedSize)
      throw std::bad_array_new_length();
    mCurSize = pNewSize;
    if (mprotect(mAddress, mCurSize, PROT_READ | PROT_WRITE))
      throw std::bad_alloc();
  }

}  // namespace wembed
