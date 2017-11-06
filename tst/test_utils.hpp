#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

#include "wembed.hpp"

void spectest_print(uint8_t* base, int32_t param) {}
int32_t spectest_global = 0;

void dump(const void* data, size_t size) {
  char ascii[17];
  size_t i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char*)data)[i]);
    if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char*)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i+1) % 8 == 0 || i+1 == size) {
      printf(" ");
      if ((i+1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i+1 == size) {
        ascii[(i+1) % 16] = '\0';
        if ((i+1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i+1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}

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
      lResult.mBits.mSignificand = 0;
      break;
    case 'N':
    case 'n': {
      assert(p[lIndex + 1] == 'a' || p[lIndex + 1] == 'A');
      assert(p[lIndex + 2] == 'n' || p[lIndex + 2] == 'N');
      lResult.mBits.mExponent = wembed::fp_bits<T>::sMaxExponent;
      if (p[lIndex + 3] == ':') {
        lResult.mBits.mSignificand = bits(strtoul(p + lIndex + 4, nullptr, 0));
      } else {
        lResult.mBits.mSignificand = wembed::fp_bits<T>::sQuietNan;
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
         && lComponents.mBits.mSignificand != 0
         && (lComponents.mBits.mSignificand & wembed::fp_bits<T>::sQuietNan);
}

template<typename T>
bool arithmetic_nan(T pInput) {
  wembed::fp_bits<T> lComponents(pInput);
  return lComponents.mBits.mExponent == wembed::fp_bits<T>::sMaxExponent
         && lComponents.mBits.mSignificand != 0;
}

namespace wembed {
  template<typename T>
  bool operator==(const fp_bits <T> &pLHS, const fp_bits<T> &pRHS) {
    return pLHS.mRaw == pRHS.mRaw;
  }

  template<typename T>
  bool operator==(const fp_bits<T> &pLHS, const T &pRHS) {
    return pLHS.mRaw == fp_bits<T>(pRHS).mRaw;
  }

  template<typename T>
  bool operator==(const T &pLHS, const fp_bits<T> &pRHS) {
    return fp_bits<T>(pLHS).mRaw == pRHS.mRaw;
  }

  template<typename T>
  std::ostream &operator<<(std::ostream &pOS, const fp_bits<T> &pVal) {
    if (pVal.mBits.mExponent == fp_bits<T>::sMaxExponent) {
      pOS << (pVal.mBits.mSign ? "-" : "+");
      if (pVal.mBits.mSignificand == 0)
        pOS << "inf";
      else
        pOS << "nan";
    } else {
      pOS << pVal.mValue;
    }
    return pOS << " (" << std::hex << pVal.mRaw << ", "
               << pVal.mBits.mSign << ", "
               << pVal.mBits.mExponent << ", "
               << pVal.mBits.mSignificand << std::dec << ')';
  }
}
