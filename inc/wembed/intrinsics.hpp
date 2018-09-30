#pragma once

#include <exception>
#include <string>

#include "utils.hpp"

namespace wembed {

  class vm_runtime_exception : std::runtime_error {
  public:
    vm_runtime_exception(const std::string &pCause) : runtime_error(pCause) {}
  };

  class unlinkable_exception : vm_runtime_exception {
  public:
    unlinkable_exception(const std::string &pCause) : vm_runtime_exception(pCause) {}
  };

  namespace intrinsics {
    i32 memory_grow(uint8_t *pContext, uint32_t pDelta);
    i32 memory_size(uint8_t *pContext);

    void throw_unlinkable(const char *pError);
    void throw_vm_exception(const char *pError);
  }

}  // namespace wembed
