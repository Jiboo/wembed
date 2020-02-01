#pragma once

#include <cstdint>
#include <limits>
#include <ostream>
#include <wembed/utils.hpp>

#include "wembed.hpp"

void spectest_print();
void spectest_print_i32(int32_t param);
void spectest_print_i32_f32(int32_t param, float param2);
void spectest_print_f64_f64(double param, double param2);
void spectest_print_f32(float param);
void spectest_print_f64(double param);

extern int32_t spectest_global_i32;
extern float spectest_global_f32;
extern double spectest_global_f64;
extern wembed::memory spectest_mem;
extern wembed::table spectest_tab;
void spectest_reset();

template<typename T>
wembed::fp_bits<T> fp(const char *p) {
  using bits = typename wembed::fp_bits<T>::bits;
  wembed::fp_bits<T> lResult(0);
  size_t lIndex = 0;
  if (p[0] == '-') {
    lResult.mBits.mSign = 1;
    lIndex++;
  } else if (p[0] == '+') {
    lIndex++;
  }
  switch (p[lIndex]) {
    case 'I':
    case 'i':
      lResult.mBits.mExponent = wembed::fp_bits<T>::sMaxExponent;
      lResult.mBits.mMantissa = 0;
      break;
    case 'N':
    case 'n': {
      assert(p[lIndex + 1] == 'a' || p[lIndex + 1] == 'A');
      assert(p[lIndex + 2] == 'n' || p[lIndex + 2] == 'N');
      lResult.mBits.mExponent = wembed::fp_bits<T>::sMaxExponent;
      if (strcmp(p, "nan:canonical") == 0) {
        lResult.mBits.mMantissa = wembed::fp_bits<T>::sQuietNan;
      } else if (strcmp(p, "nan:arithmetic") == 0) {
        lResult.mBits.mMantissa = wembed::fp_bits<T>::sArithmeticNan;
      } else if (p[lIndex + 3] == ':') {
        lResult.mBits.mMantissa = bits(strtoul(p + lIndex + 4, nullptr, 0));
      } else {
        lResult.mBits.mMantissa = wembed::fp_bits<T>::sQuietNan;
      }
    } break;
    default:
      lResult.mValue = static_cast<T>(strtod(p, nullptr));
      break;
  }
  return lResult;
}

template<typename T>
bool canonical_nan(T pInput) {
  wembed::fp_bits<T> lComponents(pInput);
  return lComponents.mBits.mExponent == wembed::fp_bits<T>::sMaxExponent
         && lComponents.mBits.mMantissa != 0
         && (lComponents.mBits.mMantissa & wembed::fp_bits<T>::sQuietNan);
}

template<typename T>
bool arithmetic_nan(T pInput) {
  wembed::fp_bits<T> lComponents(pInput);
  return lComponents.mBits.mExponent == wembed::fp_bits<T>::sMaxExponent
         && lComponents.mBits.mMantissa != 0;
}

namespace wembed {
  template<typename T>
  bool operator==(const fp_bits<T> &pLHS, const fp_bits<T> &pRHS) {
    // Hack to make +nan == -nan
    if (pLHS.mBits.mExponent == fp_bits<T>::sMaxExponent && pRHS.mBits.mExponent == fp_bits<T>::sMaxExponent)
      if (pLHS.mBits.mMantissa != 0 && pRHS.mBits.mMantissa != 0)
        return true;
    return pLHS.mRaw == pRHS.mRaw;
  }

  template<typename T>
  bool operator==(const fp_bits<T> &pLHS, const T &pRHS) {
    return pLHS == fp_bits<T>(pRHS);
  }

  template<typename T>
  bool operator==(const T &pLHS, const fp_bits<T> &pRHS) {
    return fp_bits<T>(pLHS) == pRHS;
  }

  template<typename T>
  std::ostream &operator<<(std::ostream &pOS, const fp_bits<T> &pVal) {
    if (pVal.mBits.mExponent == fp_bits<T>::sMaxExponent) {
      pOS << (pVal.mBits.mSign ? "-" : "+");
      if (pVal.mBits.mMantissa == 0)
        pOS << "inf";
      else
        pOS << "nan";
    } else {
      pOS << pVal.mValue;
    }
    return pOS << " (" << std::hex << pVal.mRaw << ", "
               << pVal.mBits.mSign << ", "
               << pVal.mBits.mExponent << ", "
               << pVal.mBits.mMantissa << std::dec << ')';
  }
}
