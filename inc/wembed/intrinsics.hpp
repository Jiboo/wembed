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
    template<typename T>
    T quiet_nan(T pInput) {
      fp_bits<T> lInput(pInput);
      lInput.mBits.mSignificand = fp_bits<T>::sQuietNan;
      return lInput;
    }
    template<typename T>
    T min(T pLHS, T pRHS) {
      if (pLHS != pLHS) return quiet_nan(pLHS);
      else if (pRHS != pRHS) return quiet_nan(pRHS);
      else if (pLHS < pRHS) return pLHS;
      else if (pLHS > pRHS) return pRHS;
      else {
        return fp_bits<T>(pLHS).mRaw > fp_bits<T>(pRHS).mRaw ? pLHS : pRHS;
      }
    }
    template<typename T>
    T max(T pLHS, T pRHS) {
      if (pLHS != pLHS) return quiet_nan(pLHS);
      else if (pRHS != pRHS) return quiet_nan(pRHS);
      else if (pLHS > pRHS) return pLHS;
      else if (pLHS < pRHS) return pRHS;
      else {
        return fp_bits<T>(pLHS).mRaw < fp_bits<T>(pRHS).mRaw ? pLHS : pRHS;
      }
    }
    template<typename T>
    T ceil(T pVal) {
      if (pVal != pVal) return quiet_nan(pVal);
      else return std::ceil(pVal);
    }
    template<typename T>
    T floor(T pVal) {
      if (pVal != pVal) return quiet_nan(pVal);
      else return std::floor(pVal);
    }
    template<typename T>
    T trunc(T pVal) {
      if (pVal != pVal) return quiet_nan(pVal);
      else return std::trunc(pVal);
    }
    template<typename T>
    T nearest(T pVal) {
      if (pVal != pVal) return quiet_nan(pVal);
      else return std::nearbyint(pVal);
    }

    i32 memory_grow(uint8_t *pContext, uint32_t pDelta);
    i32 memory_size(uint8_t *pContext);

    void throw_unlinkable(const char *pError);
    void throw_vm_exception(const char *pError);
  }

}  // namespace wembed
