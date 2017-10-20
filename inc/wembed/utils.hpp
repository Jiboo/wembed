#pragma once

#include <cstddef>
#include <cstdint>

namespace wembed {

  // Zeroed huge chunk of memory mapped to host virtual address space
  class virtual_mapping {
  public:
    virtual_mapping() {}
    virtual_mapping(size_t pInitialSize, size_t pMaximumSize);
    ~virtual_mapping();

    void resize(size_t pNewSize);
    uint8_t *data() { return mAddress; }
    size_t size() { return mCurSize; }
    size_t capacity() { return mAllocatedSize; }
  protected:
    uint8_t *mAddress = nullptr;
    size_t mCurSize = 0, mAllocatedSize = 0;
  };

  // FP info and bit manipulation
  template<typename T>
  struct fp_bits;

  template<>
  struct fp_bits<float> {
    using bits = uint32_t;
    static constexpr size_t sSignificandBits = 23;
    static constexpr bits sMaxExponent = 0xff;
    static constexpr bits sQuietNan = bits(1) << (sSignificandBits - 1);
    union {
      struct {
        bits mSignificand : 23;
        bits mExponent : 8;
        bits mSign : 1;
      } mBits;
      float mValue;
      bits mRaw;
    };
    fp_bits(const float pValue) : mValue(pValue) {}
    operator float() const { return mValue; }
  };

  template<>
  struct fp_bits<double> {
    using bits = uint64_t;
    static constexpr size_t sSignificandBits = 52;
    static constexpr bits sMaxExponent = 0x7ff;
    static constexpr bits sQuietNan = bits(1) << (sSignificandBits - 1);
    union {
      struct {
        bits mSignificand : 52;
        bits mExponent : 11;
        bits mSign : 1;
      } mBits;
      double mValue;
      bits mRaw;
    };
    fp_bits() {}
    fp_bits(const double pValue) : mValue(pValue) {}
    operator double() const { return mValue; }
  };

}  // namespace wembed
