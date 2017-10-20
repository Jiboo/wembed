#pragma once

#include "utils.hpp"

namespace wembed {

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

    i32 grow_memory(uint8_t *pContext, uint32_t pDelta);
    i32 current_memory(uint8_t *pContext);
  }

}  // namespace wembed
