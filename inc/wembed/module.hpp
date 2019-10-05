#pragma once

#include <ostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <boost/endian/conversion.hpp>

#include <llvm-c/Core.h>

#include <llvm/BinaryFormat/Dwarf.h>

#include "lang.hpp"
#include "utils.hpp"

namespace wembed {
  // Special type hashs for table/memory imports/exports
  constexpr uint64_t WEMBED_HASH_TABLE = 0x10;
  constexpr uint64_t WEMBED_HASH_MEMORY = 0x11;

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
    module(uint8_t *pInput, size_t pLen, std::initializer_list<std::string_view> pContextImportNs = {}, bool pDebugSupport = false);
    virtual ~module();

    void optimize(uint8_t pOptLevel = 4);

    void dump_ll(std::ostream &os);

    LLVMModuleRef mModule;

    LLVMValueRef symbol1(const std::string_view &pName);

  protected:
    bool mDebugSupport;
    uint8_t mOptLevel = 0;
    uint8_t *mCurrent, *mEnd;
    size_t mImportFuncOffset = 0;
    size_t mUnreachableDepth = 0;

    struct Section {
      uint8_t *mStart;
      size_t mSize;
    };
    std::unordered_map<std::string_view, Section> mCustomSections;
    Section get_custom_section(const std::string_view &pName);

    struct externsym {
      std::string mModule, mField;
    };
    externsym mMemoryImport, mTableImport;

    LLVMBuilderRef mBuilder;
    std::vector<memory_type> mMemoryTypes;
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
      bool mWithContext;

      FuncDef(LLVMValueRef pValue, uint64_t pTypeHash, bool pWithContext = false)
        : mValue(pValue), mType(pTypeHash), mWithContext(pWithContext) {}

      void retreiveName() {
        size_t lSize;
        const char *lName = LLVMGetValueName2(mValue, &lSize);
        mName = std::string(lName, lSize);
      }
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
      }
      symbol_t(external_kind pKind, uint64_t pTypeHash, std::initializer_list<LLVMValueRef> pValues)
        : mKind(pKind), mTypeHash(pTypeHash), mValues(pValues) {
      }
      void retreiveNames() {
        for (const auto &lValue : mValues) {
          size_t lSize;
          const char *lName = LLVMGetValueName2(lValue, &lSize);
          mValueNames.emplace_back(std::string(lName, lSize));
        }
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
    LLVMValueRef push(LLVMValueRef lVal);
    LLVMValueRef pop();
    LLVMValueRef pop_int();
    LLVMValueRef pop(LLVMTypeRef pDesired);

    template<typename T> T parse(uint8_t *&pPointer) {
      T lResult = *reinterpret_cast<T*>(pPointer);
      pPointer += sizeof(T);
      return boost::endian::little_to_native(lResult);
    }

    template<typename T> T parse() {
      return parse<T>(mCurrent);
    }

    std::string_view parse_str(size_t pSize);

    template<typename T> T parse_uleb128(uint8_t *&pPointer) {
      T lResult = 0;
      size_t lShift = 0;
      uint8_t lByte;
      do {
        lByte = *pPointer++;
        lResult |= T(lByte & (uint8_t) 0x7f) << lShift;
        lShift += 7;
      } while (lByte & 0x80);
      return boost::endian::little_to_native(lResult);
    }

    template<typename T> T parse_uleb128() {
      return parse_uleb128<T>(mCurrent);
    }

    template<typename T> T parse_sleb128(uint8_t *&pPointer) {
      T lResult = 0;
      size_t lShift = 0;
      uint8_t lByte;
      do {
        lByte = *pPointer++;
        lResult |= T(lByte & (uint8_t) 0x7f) << lShift;
        lShift += 7;
      } while (lByte & 0x80);
      const size_t lSize = sizeof(T) * 8;
      if ((lShift < lSize) && (lByte & 0x40))
        lResult |= (T(~0ULL) << lShift);
      return boost::endian::little_to_native(lResult);
    }

    template<typename T> T parse_sleb128() {
      return parse_sleb128<T>(mCurrent);
    }

    elem_type parse_elem_type();
    resizable_limits parse_resizable_limits();
    table_type parse_table_type();
    external_kind parse_external_kind();

    void parse_sections(std::initializer_list<std::string_view> pContextImportNs);
    void parse_custom_section(const std::string_view &pName, size_t pInternalSize);
    void parse_names(size_t pInternalSize);

    struct LineFileEntry {
      std::string_view mFilename;
      uint64_t mDirectory;
      uint64_t mLastModification;
      uint64_t mByteSize;
    };
    struct LineState {
      uint32_t mAddress;
      uint32_t mOpIndex;
      uint32_t mFile;
      uint32_t mLine;
      uint32_t mColumn;
      uint32_t mISA;
      uint32_t mDiscriminator;
      bool mIsStmt;
      bool mBasicBlock;
      bool mEndSequence;
      bool mPrologueEnd;
      bool mEpilogueBeg;
      void reset(bool pDefaultIsStmt);
    };
    struct LineItem {
      LLVMMetadataRef mLoc = nullptr;
      uint32_t mAddress;
      uint32_t mFile;
      uint32_t mLine;
      uint32_t mColumn;
    };
    struct LineSequence {
      uint32_t mStartAddress;
      uint32_t mEndAddress;
      std::vector<LineItem> mLines;
    };
    struct LineContext {
      std::vector<std::string_view> mDirectories;
      std::vector<LineFileEntry> mFiles;
      std::vector<LineSequence> mSequences;
      std::vector<LineItem> mCurrentBuffer;
      void append_row(const LineState &pState);
      LineSequence &find_sequence(uint32_t pStart, uint32_t pEnd);
    };
    std::unordered_map<size_t, LineContext> mParsedLines;

    struct AbbrevAttr {
      llvm::dwarf::Attribute mAttribute;
      llvm::dwarf::Form mForm;
    };
    using AbbrevAttrs = std::vector<AbbrevAttr>;
    struct AbbrevDecl {
      llvm::dwarf::Tag mTag;
      bool mHasChild;
      AbbrevAttrs mAttributes;
    };
    using AbbrevDecls = std::unordered_map<size_t, AbbrevDecl>;
    std::unordered_map<size_t, AbbrevDecls> mParsedAbbrevs;

    using AttrValue = std::variant<uint64_t, std::string_view>;
    using AttrValues = std::unordered_map<llvm::dwarf::Attribute, AttrValue>;
    struct DIE {
      size_t mOffset;
      LLVMMetadataRef mMetadata = nullptr;
      LLVMValueRef mValue = nullptr;
      llvm::dwarf::Tag mTag;
      AttrValues mAttributes;
      std::list<DIE> mChildren;
      DIE* mParent;

      template<typename T>
      T attr(llvm::dwarf::Attribute pAttr, T pDefault = {}) {
        auto lFound = mAttributes.find(pAttr);
        if (lFound == mAttributes.end())
          return pDefault;
        return std::get<T>(lFound->second);
      }

      // List all children with the requested tag
      std::vector<DIE*> children(llvm::dwarf::Tag pTag);

      template<typename T>
      std::vector<T> children_attr(llvm::dwarf::Tag pTag, llvm::dwarf::Attribute pAttr, T pDefault = {}) {
        std::vector<T> lResult;
        for (auto &lChild : mChildren) {
          if (lChild.mTag == pTag) {
            lResult.push_back(lChild.attr<T>(pAttr));
          }
        }
        return lResult;
      }

      // Climbs up the parent hierarchy until finding the requested tag
      DIE* parent(llvm::dwarf::Tag pTag);
    };
    std::list<DIE> mParsedDI;
    std::unordered_map<size_t, DIE*> mDIOffsetCache;

    Section mDebugStrSection;
    std::unordered_set<DIE*> mEvaluatingDIE;
    unsigned mDbgKind;

    /* FIXME JBL: Because of the ordering of debug sections (after everything), we need to bookkeep some infos to later
     * attach debug infos to them. This is uneffective, we might need to forget about the "single pass" principle and
     * parse debug info before, so that we can attach when constructing the IR.
     * This will be required when LLVM lands new way to track variables, that may point to index in the eval stack.
     */
    std::unordered_map<size_t, LLVMValueRef> mInstructions;

    struct FuncLocalsTracking {
      std::vector<LLVMValueRef> mLocals;
      struct Change {
        uint64_t mIndex;
        LLVMValueRef mNewValue;

        Change(uint64_t pIndex, LLVMValueRef pValue) : mIndex(pIndex), mNewValue(pValue) {}
      };
      std::vector<Change> mChanges;
    };
    std::unordered_map<LLVMValueRef, FuncLocalsTracking> mLocalsTracking;

    struct FuncRange {
      uint32_t mStartAddress, mEndAddress;
      LLVMValueRef mFunc;
    };
    std::vector<FuncRange> mFuncRanges;

    void parse_debug_infos();
    void parse_debug_abbrev_entries();
    void parse_debug_lines();
    std::string_view get_debug_string(size_t pOffset);
    LLVMMetadataRef get_debug_rel_file(LLVMDIBuilderRef pDIBuilder, const std::string_view &pFile, const std::string_view &pPath);
    LLVMMetadataRef get_debug_abs_file(LLVMDIBuilderRef pDIBuilder, const std::string_view &pAbsPath);
    // Find the requested file for a DW_AT_decl_file
    LLVMMetadataRef get_debug_file(LLVMDIBuilderRef pDIBuilder, DIE *pContext, uint64_t pIndex);
    void apply_die_metadata(LLVMDIBuilderRef pDIBuilder, DIE &pDIE);
    void apply_die_metadata_on_child(LLVMDIBuilderRef pDIBuilder, DIE &pDIE);
    LLVMValueRef find_func_by_range(uint32_t pStart, uint32_t pEnd);

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
    void parse_imports(std::initializer_list<std::string_view> pContextImportNs);
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
  };  // class module

}  // namespace wembed
