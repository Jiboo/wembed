#include "wembed/utils.hpp"

#include <new>

#include <sys/mman.h>

namespace wembed {

  virtual_mapping::virtual_mapping(size_t pInitialSize)
      : mCurSize(0) {
    mAddress = (uint8_t*)mmap(nullptr, capacity(), 0, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mAddress == MAP_FAILED)
      throw std::bad_alloc();
    if (pInitialSize > 0)
      resize(pInitialSize);
  }

  virtual_mapping::~virtual_mapping() {
    if (mAddress != nullptr)
      munmap(mAddress, capacity());
  }

  void virtual_mapping::resize(size_t pNewSize) {
    if (mprotect(mAddress, pNewSize, PROT_READ | PROT_WRITE))
      throw std::bad_alloc();
    mCurSize = pNewSize;
  }

}  // namespace wembed
