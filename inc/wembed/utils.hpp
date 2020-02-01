#pragma once

#include <cstddef>
#include <cstdint>

#include <bit>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

#include <llvm-c/Core.h>

namespace wembed {

  template <class T>
  inline void hash_combine(std::size_t& seed, const T& v)
  {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
  }

  template<typename T>
  constexpr T endian_reverse(const T &x) {
    static_assert(std::is_arithmetic_v<T>);
    union adapter {
      T whole;
      uint8_t bytes[sizeof(T)];
    };
    adapter a{x};
    std::reverse(std::begin(a.bytes), std::end(a.bytes));
    return a.whole;
  }

  template<class T>
  constexpr T lendian_to_native(const T& v) {
    if constexpr (std::endian::native == std::endian::little)
      return v;
    return endian_reverse(v);
  }

  using namespace std::literals::string_literals;

  void llvm_init();

  using hrclock = std::chrono::high_resolution_clock;
  std::ostream &operator<<(std::ostream &pOS, const hrclock::duration &pDur);
  void profile_step(const char *pName);

  void dump_hex(const void* data, size_t size);

  // Zeroed 4GB memory segment
  class virtual_mapping {
  public:
    virtual_mapping(size_t pInitialSize);
    virtual ~virtual_mapping();

    void resize(size_t pNewSize);
    uint8_t *data() { return mAddress; }
    size_t size() { return mCurSize; }
    size_t capacity() { return 4UL*1024*1024*1024; }

  protected:
    uint8_t *mAddress = nullptr;
    size_t mCurSize = 0;
  };

  // FP info and bit manipulation
  template<typename T>
  struct fp_bits;

  template<>
  struct fp_bits<float> {
    using bits = uint32_t;
    static constexpr size_t sSignificandBits = 23;
    static constexpr bits sMaxExponent   = 0xff;
    static constexpr bits sQuietNan      = bits(1) << (sSignificandBits - 1);
    static constexpr bits sArithmeticNan = bits(3) << (sSignificandBits - 2);
    static constexpr bits sSignMask      = 0b10000000'00000000'00000000'00000000;
    static constexpr bits sExponentMask  = 0b01111111'10000000'00000000'00000000;
    static constexpr bits sMantissaMask  = 0b00000000'01111111'11111111'11111111;
    union {
      struct {
        bits mMantissa : 23;
        bits mExponent : 8;
        bits mSign : 1;
      } mBits;
      float mValue;
      bits mRaw;
    };
    fp_bits(float pValue) : mValue(pValue) {}
    operator float() const { return mValue; }
  };

  template<>
  struct fp_bits<double> {
    using bits = uint64_t;
    static constexpr size_t sSignificandBits = 52;
    static constexpr bits sMaxExponent   = 0x7ff;
    static constexpr bits sQuietNan      = bits(1) << (sSignificandBits - 1);
    static constexpr bits sArithmeticNan = bits(3) << (sSignificandBits - 2);
    static constexpr bits sSignMask      = 0b10000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000;
    static constexpr bits sExponentMask  = 0b01111111'11110000'00000000'00000000'00000000'00000000'00000000'00000000;
    static constexpr bits sMantissaMask  = 0b00000000'00001111'11111111'11111111'11111111'11111111'11111111'11111111;
    union {
      struct {
        bits mMantissa : 52;
        bits mExponent : 11;
        bits mSign : 1;
      } mBits;
      double mValue;
      bits mRaw;
    };
    fp_bits() {}
    fp_bits(double pValue) : mValue(pValue) {}
    operator double() const { return mValue; }
  };

  template<typename T>
  LLVMTypeRef map_ctype() {
    throw std::runtime_error("can't map ctype to llvm");
  }

  template<> inline LLVMTypeRef map_ctype<void>()    { return LLVMVoidType(); }
  template<> inline LLVMTypeRef map_ctype<void*>()    { return LLVMPointerType(LLVMInt8Type(), 0); }
  template<> inline LLVMTypeRef map_ctype<int8_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<uint8_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<int16_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<uint16_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<int32_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<uint32_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<int64_t>() { return LLVMInt64Type(); }
  template<> inline LLVMTypeRef map_ctype<uint64_t>() { return LLVMInt64Type(); }
  template<> inline LLVMTypeRef map_ctype<float>()   { return LLVMFloatType(); }
  template<> inline LLVMTypeRef map_ctype<double>()  { return LLVMDoubleType(); }
  template<> inline LLVMTypeRef map_ctype<const int32_t>() { return LLVMInt32Type(); }
  template<> inline LLVMTypeRef map_ctype<const int64_t>() { return LLVMInt64Type(); }
  template<> inline LLVMTypeRef map_ctype<const float>()   { return LLVMFloatType(); }
  template<> inline LLVMTypeRef map_ctype<const double>()  { return LLVMDoubleType(); }

  template <typename TFunc>
  struct __map_fn_ctype {
    LLVMTypeRef operator()() {
      throw std::runtime_error("invalid type in hash_fn_ctype");
    }
  };

  template <typename TReturn, typename...TParams>
  struct __map_fn_ctype<TReturn (TParams...)> {
    LLVMTypeRef operator()() {
      LLVMTypeRef param_types[] = {map_ctype<TParams>()...};
      return LLVMFunctionType(map_ctype<TReturn>(), param_types, sizeof(param_types) / sizeof(LLVMTypeRef), false);
    }
  };

  template <typename T>
  inline LLVMTypeRef map_fn_ctype() {
    return __map_fn_ctype<T>{}();
  }

  uint64_t hash_type(LLVMTypeRef pType, bool pConst = false);
  uint64_t hash_fn_type(LLVMTypeRef pType);

  template <typename T>
  inline uint64_t hash_ctype() {
    if constexpr(std::is_enum<T>::value)
      return hash_type(map_ctype<typename std::underlying_type<T>::type>(), std::is_const<T>::value);
    else
      return hash_type(map_ctype<typename std::remove_const<T>::type>(), std::is_const<T>::value);
  }

  template <typename TLast>
  void typehash_combine(uint64_t &pSeed) {
    hash_combine(pSeed, hash_ctype<TLast>());
  }

  template <typename TFirst, typename TSecond, typename...TRest>
  void typehash_combine(uint64_t &pSeed) {
    hash_combine(pSeed, hash_ctype<TFirst>());
    typehash_combine<TSecond, TRest...>(pSeed);
  }

  template <typename TFunc>
  struct __hash_fn_ctype {
    uint64_t operator()() {
      throw std::runtime_error("invalid type in hash_fn_ctype");
    }
  };

  template <typename TReturn, typename...TParams>
  struct __hash_fn_ctype<TReturn (TParams...)> {
    uint64_t operator()() {
      uint64_t lSeed = 1;
      typehash_combine<TReturn, TParams...>(lSeed);
      return lSeed;
    }
  };

  template <typename T>
  inline uint64_t hash_fn_ctype() {
    return __hash_fn_ctype<T>{}();
  }

  template <typename TReturn, typename...TParams>
  inline uint64_t hash_fn_ctype_ptr(TReturn (*)(TParams...)) {
    return __hash_fn_ctype<TReturn(TParams...)>{}();
  }

}  // namespace wembed
