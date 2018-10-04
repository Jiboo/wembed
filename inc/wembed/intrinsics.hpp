#pragma once

#include <exception>
#include <string>

#include "utils.hpp"

namespace wembed {

  class vm_runtime_exception : public std::runtime_error {
  public:
    vm_runtime_exception(const std::string &pCause) : std::runtime_error(pCause) {}
    virtual ~vm_runtime_exception() throw () {}
  };

  class unlinkable_exception : public vm_runtime_exception {
  public:
    unlinkable_exception(const std::string &pCause) : vm_runtime_exception(pCause) {}
    virtual ~unlinkable_exception() throw () {}
  };

  namespace intrinsics {
    i32 memory_grow(uint8_t *pContext, uint32_t pDelta);
    i32 memory_size(uint8_t *pContext);
    i32 table_size(uint8_t *pContext);

    void throw_unlinkable(const char *pError);
    void throw_vm_exception(const char *pError);
  }

}  // namespace wembed
