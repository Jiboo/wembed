#pragma once

#include <ostream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/endian/conversion.hpp>

#include <llvm-c/Core.h>

#include "lang.hpp"
#include "utils.hpp"

namespace wembed {

  class malformed_exception : public std::runtime_error {
  public:
    malformed_exception(const std::string &pCause) : runtime_error(pCause) {}
    virtual ~malformed_exception() throw () {}
  };

  class invalid_exception : public std::runtime_error {
  public:
    invalid_exception(const std::string &pCause) : runtime_error(pCause) {}
    virtual ~invalid_exception() throw () {}
  };

  class module {
    friend class context;
  public:
    module(uint8_t *pInput, size_t pLen);
    virtual ~module();

    void dump_ll(std::ostream &os);

    LLVMModuleRef mModule;

    LLVMValueRef symbol1(const std::string_view &pName);

  protected:
    uint8_t *mCurrent, *mEnd;
    size_t mImportFuncOffset = 0;
    size_t mUnreachableDepth = 0;

    LLVMBuilderRef mBuilder;
    std::vector<memory_type> mMemoryTypes;
    externsym mMemoryImport, mTableImport;
    LLVMValueRef mBaseMemory = nullptr;
    LLVMValueRef mContextRef;
    struct Table {
      table_type mType;
      LLVMValueRef mPointers;
      LLVMValueRef mTypes;

      inline Table() {}
      inline Table(table_type pType, LLVMValueRef pPointers, LLVMValueRef pTypes)
        : mType(pType), mPointers(pPointers), mTypes(pTypes) {}
    };
    std::vector<Table> mTables;
    LLVMValueRef mStartFunc;
    LLVMBasicBlockRef mStartInit, mStartUser;
    LLVMValueRef mMemCpy,
        mCtlz_i32, mCtlz_i64, mCttz_i32, mCttz_i64, mCtpop_i32, mCtpop_i64,
        mSqrt_f32, mSqrt_f64, mCeil_f32, mCeil_f64, mFloor_f32, mFloor_f64,
        mTrunc_f32, mTrunc_f64, mNearest_f32, mNearest_f64,
        mAbs_f32, mAbs_f64, mMin_f32, mMin_f64, mMax_f32, mMax_f64,
        mCopysign_f32, mCopysign_f64,
        mMemoryGrow, mMemorySize, mTableSize,
        mUAddWithOverflow_i32, mUAddWithOverflow_i64,
        mThrowUnlinkable, mThrowVMException;
    std::vector<LLVMValueRef> mEvalStack;
    enum CFInstr {
      cf_function, cf_block, cf_if, cf_else, cf_loop
    };
    struct CFEntry {
      CFInstr mInstr;
      LLVMTypeRef mSignature;
      LLVMBasicBlockRef mEnd;
      LLVMValueRef mPhi;
      LLVMBasicBlockRef mElse;
      size_t mOuterStackSize;
      size_t mOuterBlockDepth;
      bool mReachable;
      bool mElseReachable;

      inline CFEntry() {}
      inline CFEntry(CFInstr pInstr, LLVMTypeRef pSignature, LLVMBasicBlockRef pEnd, LLVMValueRef pPhi,
                     LLVMBasicBlockRef pElse, size_t pOuterStackSize, size_t pOuterBlockDepth,
                     bool pReachable, bool pElseReachable)
                     : mInstr(pInstr), mSignature(pSignature), mEnd(pEnd), mPhi(pPhi), mElse(pElse),
                       mOuterStackSize(pOuterStackSize), mOuterBlockDepth(pOuterBlockDepth),
                       mReachable(pReachable), mElseReachable(pElseReachable) {}
    };
    std::vector<CFEntry> mCFEntries;
    struct BlockEntry {
      LLVMTypeRef mSignature;
      LLVMBasicBlockRef mBlock;
      LLVMValueRef mPhi;

      inline BlockEntry() {}
      inline BlockEntry(LLVMTypeRef pSignature, LLVMBasicBlockRef pBlock, LLVMValueRef pPhi)
        : mSignature(pSignature), mBlock(pBlock), mPhi(pPhi) {}
    };
    std::vector<BlockEntry> mBlockEntries;

    std::vector<LLVMTypeRef> mTypes;
    std::vector<LLVMValueRef> mGlobals;

    struct FuncDef {
      LLVMValueRef mValue;
      std::string mName;
      uint64_t mType;

      FuncDef(LLVMValueRef pValue, const std::string &pName, uint64_t pTypeHash)
        : mValue(pValue), mName(pName), mType(pTypeHash) {}
    };
    std::vector<FuncDef> mFunctions;

    struct symbol_t {
      external_kind mKind;
      uint64_t mTypeHash;
      std::vector<LLVMValueRef> mValues; // Only tables have 2 values
      std::vector<std::string> mValueNames;
      symbol_t() {}
      symbol_t(external_kind pKind, uint64_t pTypeHash, LLVMValueRef pValue) : mKind(pKind), mTypeHash(pTypeHash) {
        mValues.emplace_back(pValue);
        mValueNames.emplace_back(LLVMGetValueName(pValue));
      }
      symbol_t(external_kind pKind, uint64_t pTypeHash, std::initializer_list<LLVMValueRef> pValues)
        : mKind(pKind), mTypeHash(pTypeHash), mValues(pValues) {
        for (const auto &lValue : mValues)
          mValueNames.emplace_back(LLVMGetValueName(lValue));
      }
    };
    std::unordered_map<std::string, symbol_t> mExports;
    std::unordered_map<std::string, std::unordered_multimap<std::string, symbol_t>> mImports;

    void pushCFEntry(CFInstr pInstr, LLVMTypeRef pType, LLVMBasicBlockRef pEnd, LLVMValueRef pPhi,
                     LLVMBasicBlockRef pElse = nullptr);
    void pushBlockEntry(LLVMTypeRef pType, LLVMBasicBlockRef pBlock, LLVMValueRef pPhi);
    const BlockEntry &branch_depth(size_t pDepth);
    LLVMValueRef top();
    LLVMValueRef top(LLVMTypeRef pDesired);
    void push(LLVMValueRef lVal);
    LLVMValueRef pop();
    LLVMValueRef pop_int();
    LLVMValueRef pop(LLVMTypeRef pDesired);

    template<typename T> T parse() {
      T lResult = *reinterpret_cast<T*>(mCurrent);
      mCurrent += sizeof(T);
      return boost::endian::little_to_native(lResult);
    }

    std::string_view parse_str(size_t pSize);

    template<typename T> T parse_uleb128() {
      T lResult = 0;
      size_t lShift = 0;
      uint8_t lByte;
      do {
        lByte = *mCurrent++;
        lResult |= T(lByte & (uint8_t) 0x7f) << lShift;
        lShift += 7;
      } while (lByte & 0x80);
      return boost::endian::little_to_native(lResult);
    }

    template<typename T> T parse_sleb128() {
      T lResult = 0;
      size_t lShift = 0;
      uint8_t lByte;
      do {
        lByte = *mCurrent++;
        lResult |= T(lByte & (uint8_t) 0x7f) << lShift;
        lShift += 7;
      } while (lByte & 0x80);
      const size_t lSize = sizeof(T) * 8;
      if ((lShift < lSize) && (lByte & 0x40))
        lResult |= - (1 << lShift);
      return boost::endian::little_to_native(lResult);
    }

    elem_type parse_elem_type();
    resizable_limits parse_resizable_limits();
    table_type parse_table_type();
    external_kind parse_external_kind();

    void parse_sections();
    void parse_custom_section(const std::string_view &pName, size_t pInternalSize);
    void parse_names(size_t pInternalSize);

    LLVMValueRef get_const(bool value) { return LLVMConstInt(LLVMInt1Type(), ull_t(value), false); }
    LLVMValueRef get_const(int32_t value) { return LLVMConstInt(LLVMInt32Type(), ull_t(value), true); }
    LLVMValueRef get_const(int64_t value) { return LLVMConstInt(LLVMInt64Type(), ull_t(value), true); }
    LLVMValueRef get_const(uint32_t value) { return LLVMConstInt(LLVMInt32Type(), ull_t(value), false); }
    LLVMValueRef get_const(uint64_t value) { return LLVMConstInt(LLVMInt64Type(), ull_t(value), false); }
    LLVMValueRef get_const(float value) { return LLVMConstReal(LLVMFloatType(), value); }
    LLVMValueRef get_const(double value) { return LLVMConstReal(LLVMDoubleType(), value); }
    LLVMValueRef get_zero(LLVMTypeRef pType);
    LLVMValueRef get_string(const char *pStart);

    uint8_t bit_count(LLVMTypeRef pType);

    LLVMValueRef emit_shift_mask(LLVMTypeRef pType, LLVMValueRef pCount) ;
    LLVMValueRef emit_rotl(LLVMTypeRef pType, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_rotr(LLVMTypeRef pType, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_udiv(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_sdiv(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_urem(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_srem(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_quiet_nan(LLVMValueRef pInput);
    LLVMValueRef emit_quiet_nan_or_intrinsic(LLVMValueRef pInput, LLVMValueRef pF32Intr, LLVMValueRef pF64Intr);
    LLVMValueRef emit_min(LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMValueRef emit_max(LLVMValueRef pLHS, LLVMValueRef pRHS);

    LLVMTypeRef parse_llvm_btype();
    LLVMTypeRef parse_llvm_vtype();

    std::function<LLVMValueRef()> parse_llvm_init(LLVMTypeRef pType);

    void init_intrinsics();

    LLVMValueRef i32_to_bool(LLVMValueRef i32);
    LLVMValueRef bool_to_i32(LLVMValueRef b);

    LLVMValueRef create_phi(LLVMTypeRef pType, LLVMBasicBlockRef pBlock);
    LLVMValueRef init_intrinsic(const std::string &pName, LLVMTypeRef pReturnType,
                                const std::initializer_list<LLVMTypeRef> &pArgTypes);
    LLVMValueRef call_intrinsic(LLVMValueRef pIntrinsic, const std::initializer_list<LLVMValueRef> &pArgs);
    LLVMValueRef init_mv_intrinsic(const std::string &pName, const std::initializer_list<LLVMTypeRef> &pReturnTypes,
                                const std::initializer_list<LLVMTypeRef> &pArgTypes);
    template<size_t TCount>
    std::array<LLVMValueRef, TCount> call_mv_intrinsic(LLVMValueRef pIntrinsic, const std::initializer_list<LLVMValueRef> &pArgs) {
      LLVMValueRef *lArgs = const_cast<LLVMValueRef*>(pArgs.begin());
      LLVMValueRef lCallResult = LLVMBuildCall(mBuilder, pIntrinsic, lArgs, pArgs.size(), "res");
      std::array<LLVMValueRef, TCount> lResult;
      for (size_t lIndex = 0; lIndex < TCount; lIndex++)
        lResult[lIndex] = LLVMBuildExtractValue(mBuilder, lCallResult, lIndex, "elem");
      return lResult;
    }
    LLVMBasicBlockRef trap_if(LLVMValueRef lFunc, LLVMValueRef pCondition, LLVMValueRef pIntrinsic,
                                   const std::initializer_list<LLVMValueRef> &pArgs);
    LLVMBasicBlockRef trap_data_copy(LLVMValueRef lFunc, LLVMValueRef pOffset, size_t pSize);
    LLVMBasicBlockRef trap_elem_copy(LLVMValueRef lFunc, LLVMValueRef pOffset);
    LLVMBasicBlockRef trap_zero_div(LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);
    LLVMBasicBlockRef trap_szero_div(LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS);

    LLVMValueRef clear_nan(LLVMValueRef pInput);
    LLVMValueRef clear_nan_internal(LLVMTypeRef pInputType, LLVMTypeRef pIntType, LLVMValueRef pInput,
                            LLVMValueRef pConstMaskSignificand, LLVMValueRef pConstMaskExponent,
                            LLVMValueRef pConstNanBit);

    void parse_types();
    void parse_imports();
    void parse_functions();
    void parse_section_table(uint32_t pSectionSize);
    void parse_section_memory(uint32_t pSectionSize);
    void parse_globals();
    void parse_exports();
    void parse_section_start(uint32_t pSectionSize);
    void parse_section_element(uint32_t pSectionSize);
    void skip_unreachable(uint8_t *pPastEnd);
    void parse_section_code(uint32_t pSectionSize);
    void parse_section_data(uint32_t pSectionSize);

    void finalize();
  };  // class module

}  // namespace wembed
