#include <filesystem>
#include <sstream>
#include <stack>

#include <llvm-c/Analysis.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Transforms/AggressiveInstCombine.h>
#include <llvm-c/Transforms/InstCombine.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Utils.h>
#include <llvm-c/Transforms/Vectorize.h>

#include "wembed.hpp"

#ifdef WEMBED_VERBOSE
#include <iostream>
#include <iomanip>
#endif

namespace wembed {

  module::module(uint8_t *pInput, size_t pLen, std::initializer_list<std::string_view> pContextImportNs, bool pDebugSupport) : mDebugSupport(pDebugSupport) {
    profile_step("  module/ctr");
    if (pInput == nullptr || pLen < 4)
      throw malformed_exception("invalid input");

    mCurrent = pInput;
    mEnd = pInput + pLen;

    if (parse<uint32_t>() != 0x6d736100)
      throw malformed_exception("unexpected magic code");

    LLVMTypeRef lVoidFuncT = LLVMFunctionType(LLVMVoidType(), nullptr, 0, false);
    mModule = LLVMModuleCreateWithName("module");
    mBaseMemory = LLVMAddGlobal(mModule, LLVMInt8Type(), "wembed.baseMemory");
    mContextRef = LLVMAddGlobal(mModule, LLVMInt8Type(), "wembed.ctxRef");
    mBuilder = LLVMCreateBuilder();
    mStartFunc = LLVMAddFunction(mModule, "wembed.start", lVoidFuncT);
    mStartInit = LLVMAppendBasicBlock(mStartFunc, "entry");
    mStartUser = LLVMAppendBasicBlock(mStartFunc, "callStart");
    LLVMPositionBuilderAtEnd(mBuilder, mStartInit);
    init_intrinsics();

    char *lTriple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef lTarget;
    if (LLVMGetTargetFromTriple(lTriple, &lTarget, nullptr))
      throw std::runtime_error("can't get triple for host");
    LLVMTargetMachineRef lTMachine = LLVMCreateTargetMachine(lTarget, lTriple, "", "", LLVMCodeGenLevelNone, LLVMRelocStatic, LLVMCodeModelJITDefault);
    LLVMTargetDataRef lDataLayoutRef = LLVMCreateTargetDataLayout(lTMachine);
    LLVMSetModuleDataLayout(mModule, lDataLayoutRef);
    LLVMDisposeTargetData(lDataLayoutRef);
    LLVMSetTarget(mModule, lTriple);
    LLVMDisposeMessage(lTriple);
    LLVMDisposeTargetMachine(lTMachine);

    profile_step("  module/init");

    switch(parse<uint32_t>()) { // version
      case 1: parse_sections(pContextImportNs); break;
      default: throw malformed_exception("unexpected version");
    }
    profile_step("  module/parsed");

#ifdef WEMBED_VERBOSE
    std::cout << "Finalizing module..." << std::endl;
#endif

    for(auto &lNamespace : mImports)
      for(auto &lImports : lNamespace.second)
        lImports.second.retreiveNames();

    for(auto &lExports : mExports)
      lExports.second.retreiveNames();

    for (auto &lFunc : mFunctions)
      lFunc.retreiveName();

    profile_step("  module/rename");

    if (pDebugSupport)
      parse_debug_infos();

    profile_step("  module/debug");

    // finish __wstart
    LLVMPositionBuilderAtEnd(mBuilder, mStartInit);
    LLVMBuildBr(mBuilder, mStartUser);
    LLVMPositionBuilderAtEnd(mBuilder, mStartUser);
    LLVMBuildRetVoid(mBuilder);
    LLVMDisposeBuilder(mBuilder);

    char *lError = nullptr;
    if (LLVMVerifyModule(mModule, LLVMReturnStatusAction, &lError)) {
      std::stringstream lMessage;
      lMessage << "module failed verification: " << lError << "\n\n";
      dump_ll(lMessage);
      LLVMDisposeMessage(lError);
      throw invalid_exception(lMessage.str());
    }
    LLVMDisposeMessage(lError);
    profile_step("  module/verified");
  }

  module::~module() {
    //LLVMDisposeModule(mModule);
  }

  void module::dump_ll(std::ostream &os) {
    char *lCode = LLVMPrintModuleToString(mModule);
    os << lCode;
    LLVMDisposeMessage(lCode);
  }

  LLVMValueRef module::symbol1(const std::string_view &pName) {
    auto lFound = mExports.find(std::string(pName));
    if (lFound == mExports.end())
      throw std::runtime_error("can't find symbol: "s + pName.data());
    if (lFound->second.mValues.size() > 1)
      throw std::runtime_error("more than one value in: "s + pName.data());
    return mExports[std::string(pName)].mValues[0];
  }

  void module::pushCFEntry(CFInstr pInstr, LLVMTypeRef pType, LLVMBasicBlockRef pEnd, LLVMValueRef pPhi,
                           LLVMBasicBlockRef pElse) {
    if (mCFEntries.size())
      assert(mCFEntries.back().mReachable);

    mCFEntries.emplace_back(pInstr, pType, pEnd, pPhi, pElse, mEvalStack.size(), mBlockEntries.size(), true, true);
  }

  void module::pushBlockEntry(LLVMTypeRef pType, LLVMBasicBlockRef pBlock, LLVMValueRef pPhi) {
    mBlockEntries.emplace_back(pType, pBlock, pPhi);
  }

  const module::BlockEntry &module::branch_depth(size_t pDepth) {
    if (pDepth >= mBlockEntries.size())
      throw invalid_exception("invalid branch target");
    return mBlockEntries[mBlockEntries.size() - pDepth - 1];
  }

  LLVMValueRef module::top() {
    if (mEvalStack.empty())
      throw invalid_exception("topping empty stack");
    else if (!mCFEntries.empty() && mEvalStack.size() <= mCFEntries.back().mOuterStackSize)
      throw invalid_exception("topping stack outside bounds of current context");
    return mEvalStack.back();
  }

  LLVMValueRef module::top(LLVMTypeRef pDesired) {
    LLVMValueRef lVal = top();
    if(LLVMTypeOf(lVal) != pDesired)
      throw invalid_exception("topping wrong type");
    return lVal;
  }

  LLVMValueRef module::push(LLVMValueRef pVal) {
    assert(pVal);
    mEvalStack.emplace_back(pVal);
    return pVal;
  }

  LLVMValueRef module::pop() {
    if (mEvalStack.empty())
      throw invalid_exception("popping empty stack");
    else if (!mCFEntries.empty() && mEvalStack.size() <= mCFEntries.back().mOuterStackSize)
      throw invalid_exception("popping stack outside bounds of current context");
    auto lVal = mEvalStack.back();
    mEvalStack.pop_back();
    return lVal;
  }

  LLVMValueRef module::pop(LLVMTypeRef pDesired) {
    LLVMValueRef lVal = pop();
    if(LLVMTypeOf(lVal) != pDesired) {
#ifdef WEMBED_VERBOSE
      char *lExpected = LLVMPrintTypeToString(pDesired);
      char *lFound = LLVMPrintTypeToString(LLVMTypeOf(lVal));
      std::stringstream lError;
      lError << "popping wrong type, expected " << lExpected << ", got " << lFound;
      LLVMDisposeMessage(lExpected);
      LLVMDisposeMessage(lFound);
      throw invalid_exception(lError.str());
#else
      throw invalid_exception("popping wrong type");
#endif
    }
    return lVal;
  }

  LLVMValueRef module::pop_int() {
    LLVMValueRef lVal = pop();
    LLVMTypeRef lType = LLVMTypeOf(lVal);
    if(lType != LLVMInt32Type() && lType != LLVMInt64Type()) {
      throw invalid_exception("popping wrong type");
    }
    return lVal;
  }

  LLVMValueRef module::init_intrinsic(const std::string &pName,
                                      LLVMTypeRef pReturnType,
                                      const std::initializer_list<LLVMTypeRef> &pArgTypes) {

    LLVMTypeRef *lArgTypes = const_cast<LLVMTypeRef*>(pArgTypes.begin());
    LLVMTypeRef lType = LLVMFunctionType(pReturnType, lArgTypes, pArgTypes.size(), false);
    return LLVMAddFunction(mModule, pName.c_str(), lType);
  }

  LLVMValueRef module::call_intrinsic(LLVMValueRef pIntrinsic, const std::initializer_list<LLVMValueRef> &pArgs) {
    LLVMValueRef *lArgs = const_cast<LLVMValueRef*>(pArgs.begin());
    return LLVMBuildCall(mBuilder, pIntrinsic, lArgs, pArgs.size(), "");
  }

  LLVMValueRef module::init_mv_intrinsic(const std::string &pName,
                                         const std::initializer_list<LLVMTypeRef> &pReturnTypes,
                                         const std::initializer_list<LLVMTypeRef> &pArgTypes) {
    LLVMTypeRef *lArgTypes = const_cast<LLVMTypeRef*>(pArgTypes.begin());
    LLVMTypeRef *lReturnTypes = const_cast<LLVMTypeRef*>(pReturnTypes.begin());
    LLVMTypeRef lReturnType = LLVMStructType(lReturnTypes, pReturnTypes.size(), false);
    LLVMTypeRef lType = LLVMFunctionType(lReturnType, lArgTypes, pArgTypes.size(), false);
    return LLVMAddFunction(mModule, pName.c_str(), lType);
  }

  LLVMBasicBlockRef module::trap_if(LLVMValueRef pFunc, LLVMValueRef pCondition, LLVMValueRef pIntrinsic,
                                         const std::initializer_list<LLVMValueRef> &pArgs) {
    LLVMBasicBlockRef lTrapThen = LLVMAppendBasicBlock(pFunc, "trapThen");
    LLVMBasicBlockRef lTrapSkip = LLVMAppendBasicBlock(pFunc, "trapSkip");

    LLVMBuildCondBr(mBuilder, pCondition, lTrapThen, lTrapSkip);

    auto lPrevBlock = LLVMGetInsertBlock(mBuilder);
    LLVMMoveBasicBlockAfter(lTrapSkip, lPrevBlock);
    LLVMMoveBasicBlockAfter(lTrapThen, lPrevBlock);

    LLVMPositionBuilderAtEnd(mBuilder, lTrapThen);
    call_intrinsic(pIntrinsic, pArgs);
    LLVMBuildUnreachable(mBuilder);

    LLVMPositionBuilderAtEnd(mBuilder, lTrapSkip);
    return lTrapSkip;
  }

  LLVMBasicBlockRef module::trap_data_copy(LLVMValueRef pFunc, LLVMValueRef pOffset, size_t pSize) {
    LLVMValueRef lMemorySizePage = LLVMBuildCall(mBuilder, mMemorySize, &mContextRef, 1, "curMemPage");
    LLVMValueRef lMemorySizeBytes = LLVMBuildMul(mBuilder, lMemorySizePage, get_const(64 * 1024), "curMemByte");
    LLVMValueRef lUnderflow = LLVMBuildICmp(mBuilder, LLVMIntSLT, pOffset, get_zero(LLVMTypeOf(pOffset)), "testOverflow");
    LLVMValueRef lTotalOffset = LLVMBuildAdd(mBuilder, pOffset, get_const(i32(pSize)), "endByteOffset");
    LLVMValueRef lOverflow = LLVMBuildICmp(mBuilder, LLVMIntUGT, lTotalOffset, lMemorySizeBytes, "testOverflow");
    LLVMValueRef lBoundsError = LLVMBuildOr(mBuilder, lUnderflow, lOverflow, "boundsError");
    static const char *lErrorString = "data segment does not fit";
    return trap_if(pFunc, lBoundsError, mThrowUnlinkable, {get_string(lErrorString)});
  }

  LLVMBasicBlockRef module::trap_elem_copy(LLVMValueRef lFunc, LLVMValueRef pOffset) {
    LLVMValueRef lTabSize = LLVMBuildCall(mBuilder, mTableSize, &mContextRef, 1, "curTabSize");
    LLVMValueRef lOverflow = LLVMBuildICmp(mBuilder, LLVMIntUGE, pOffset, lTabSize, "testOverflow");
    static const char *lErrorString = "elements segment does not fit trap_elem_copy";
    return trap_if(lFunc, lOverflow, mThrowUnlinkable, {get_string(lErrorString)});
  }

  LLVMBasicBlockRef module::trap_zero_div(LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    LLVMValueRef lDivZero = LLVMBuildICmp(mBuilder, LLVMIntEQ, pRHS, get_zero(LLVMTypeOf(pRHS)), "divZero");
    static const char *lErrorString = "int div by zero";
    return trap_if(lFunc, lDivZero, mThrowVMException, {get_string(lErrorString)});
  }

  LLVMBasicBlockRef module::trap_szero_div(LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    LLVMValueRef lMin, lMinus1;
    LLVMTypeRef lLType = LLVMTypeOf(pLHS);
    if (lLType == LLVMInt32Type()) {
      lMin = get_const(std::numeric_limits<int32_t>::min());
      lMinus1 = get_const(int32_t(-1));
    }
    else if (lLType == LLVMInt64Type()) {
      lMin = get_const(std::numeric_limits<int64_t>::min());
      lMinus1 = get_const(int64_t(-1));
    }
    else {
      throw std::runtime_error("unexpected type in trap_szero_div");
    }
    LLVMValueRef lLHSIsMin = LLVMBuildICmp(mBuilder, LLVMIntEQ, pLHS, lMin, "lhsMin");
    LLVMValueRef lRHSIsMinus1 = LLVMBuildICmp(mBuilder, LLVMIntEQ, pRHS, lMinus1, "rhsMinus1");
    LLVMValueRef lOverflow = LLVMBuildAnd(mBuilder, lLHSIsMin, lRHSIsMinus1, "overflow");
    LLVMValueRef lDivZero = LLVMBuildICmp(mBuilder, LLVMIntEQ, pRHS, get_zero(LLVMTypeOf(pRHS)), "divZero");
    LLVMValueRef lSZeroDiv = LLVMBuildOr(mBuilder, lDivZero, lOverflow, "szero_div");
    static const char *lErrorString = "int div by zero or overflow";
    return trap_if(lFunc, lSZeroDiv, mThrowVMException, {get_string(lErrorString)});
  }

  void module::init_intrinsics() {
    mMemCpy = init_intrinsic("llvm.memcpy.p0i8.p0i8.i32", LLVMVoidType(), {
        LLVMPointerType(LLVMInt8Type(), 0), // dest
        LLVMPointerType(LLVMInt8Type(), 0), // src
        LLVMInt32Type(), // len
        LLVMInt1Type() // volatile
    });

    mCtlz_i32 = init_intrinsic("llvm.ctlz.i32", LLVMInt32Type(), {
        LLVMInt32Type(), // src
        LLVMInt1Type() // is_zero_undef
    });
    mCtlz_i64 = init_intrinsic("llvm.ctlz.i64", LLVMInt64Type(), {
        LLVMInt64Type(), // src
        LLVMInt1Type() // is_zero_undef
    });

    mCttz_i32 = init_intrinsic("llvm.cttz.i32", LLVMInt32Type(), {
        LLVMInt32Type(), // src
        LLVMInt1Type() // is_zero_undef
    });
    mCttz_i64 = init_intrinsic("llvm.cttz.i64", LLVMInt64Type(), {
        LLVMInt64Type(), // src
        LLVMInt1Type() // is_zero_undef
    });

    mCtpop_i32 = init_intrinsic("llvm.ctpop.i32", LLVMInt32Type(), {LLVMInt32Type()});
    mCtpop_i64 = init_intrinsic("llvm.ctpop.i64", LLVMInt64Type(), {LLVMInt64Type()});
    mSqrt_f32 = init_intrinsic("llvm.sqrt.f32", LLVMFloatType(), {LLVMFloatType()});
    mSqrt_f64 = init_intrinsic("llvm.sqrt.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mAbs_f32 = init_intrinsic("llvm.fabs.f32", LLVMFloatType(), {LLVMFloatType()});
    mAbs_f64 = init_intrinsic("llvm.fabs.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mCopysign_f32 = init_intrinsic("llvm.copysign.f32", LLVMFloatType(), {LLVMFloatType(), LLVMFloatType()});
    mCopysign_f64 = init_intrinsic("llvm.copysign.f64", LLVMDoubleType(), {LLVMDoubleType(), LLVMDoubleType()});
    mCeil_f32 = init_intrinsic("llvm.ceil.f32", LLVMFloatType(), {LLVMFloatType()});
    mCeil_f64 = init_intrinsic("llvm.ceil.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mFloor_f32 = init_intrinsic("llvm.floor.f32", LLVMFloatType(), {LLVMFloatType()});
    mFloor_f64 = init_intrinsic("llvm.floor.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mTrunc_f32 = init_intrinsic("llvm.trunc.f32", LLVMFloatType(), {LLVMFloatType()});
    mTrunc_f64 = init_intrinsic("llvm.trunc.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mNearest_f32 = init_intrinsic("llvm.nearbyint.f32", LLVMFloatType(), {LLVMFloatType()});
    mNearest_f64 = init_intrinsic("llvm.nearbyint.f64", LLVMDoubleType(), {LLVMDoubleType()});
    mMin_f32 = init_intrinsic("llvm.minnum.f32", LLVMFloatType(), {LLVMFloatType(), LLVMFloatType()});
    mMin_f64 = init_intrinsic("llvm.minnum.f64", LLVMDoubleType(), {LLVMDoubleType(), LLVMDoubleType()});
    mMax_f32 = init_intrinsic("llvm.maxnum.f32", LLVMFloatType(), {LLVMFloatType(), LLVMFloatType()});
    mMax_f64 = init_intrinsic("llvm.maxnum.f64", LLVMDoubleType(), {LLVMDoubleType(), LLVMDoubleType()});

    mUAddWithOverflow_i32 = init_mv_intrinsic("llvm.uadd.with.overflow.i32", {LLVMInt32Type(), LLVMInt1Type()}, {LLVMInt32Type(), LLVMInt32Type()});
    mUAddWithOverflow_i64 = init_mv_intrinsic("llvm.uadd.with.overflow.i64", {LLVMInt64Type(), LLVMInt1Type()}, {LLVMInt64Type(), LLVMInt64Type()});

    mMemoryGrow = init_intrinsic("wembed.memory.grow", LLVMInt32Type(), {
        LLVMPointerType(LLVMInt8Type(), 0),
        LLVMInt32Type()
    });
    mMemorySize = init_intrinsic("wembed.memory.size", LLVMInt32Type(), {
        LLVMPointerType(LLVMInt8Type(), 0)
    });
    mTableSize = init_intrinsic("wembed.table.size", LLVMInt32Type(), {
        LLVMPointerType(LLVMInt8Type(), 0)
    });
    mThrowUnlinkable = init_intrinsic("wembed.throw.unlinkable", LLVMVoidType(), {LLVMPointerType(LLVMInt8Type(), 0)});
    mThrowVMException = init_intrinsic("wembed.throw.vm_exception", LLVMVoidType(), {LLVMPointerType(LLVMInt8Type(), 0)});
  }

  LLVMValueRef module::i32_to_bool(LLVMValueRef i32) {
    LLVMValueRef lZero32 = LLVMConstInt(LLVMInt32Type(), 0, true);
    return LLVMBuildICmp(mBuilder, LLVMIntNE, i32, lZero32, "boolAsI32");
  }

  LLVMValueRef module::bool_to_i32(LLVMValueRef b) {
    return LLVMBuildZExt(mBuilder, b, LLVMInt32Type(), "i32AsBool");
  }

  LLVMValueRef module::create_phi(LLVMTypeRef pType, LLVMBasicBlockRef pBlock) {
    if (pType == LLVMVoidType())
      return nullptr;
    auto lBefore = LLVMGetInsertBlock(mBuilder);
    LLVMPositionBuilderAtEnd(mBuilder, pBlock);
    auto lInstr = LLVMBuildPhi(mBuilder, pType, "phi");
    if(lBefore)
      LLVMPositionBuilderAtEnd(mBuilder, lBefore);
    return lInstr;
  }

  std::string_view module::parse_str(size_t pSize) {
    auto lResult = std::string_view((char*)mCurrent, pSize);
    mCurrent += pSize;
    return lResult;
  }

  elem_type module::parse_elem_type() {
    return (elem_type)parse<int8_t>();
  }

  resizable_limits module::parse_resizable_limits() {
    resizable_limits lResult;
    lResult.mFlags = parse<uint8_t>();
    lResult.mInitial = parse_uleb128<uint32_t>();
    if (lResult.mFlags & 0x1) {
      lResult.mMaximum = parse_uleb128<uint32_t>();
      if (lResult.mMaximum < lResult.mInitial)
        throw invalid_exception("maximum shouldn't be smaller than initial");
    }
    return lResult;
  }

  table_type module::parse_table_type() {
    table_type lResult;
    lResult.mType = parse_elem_type();
    lResult.mLimits = parse_resizable_limits();
    return lResult;
  }

  external_kind module::parse_external_kind() {
    return (external_kind)parse<uint8_t>();
  }

  void module::parse_sections(std::initializer_list<std::string_view> pContextImportNs) {
    while(mCurrent < mEnd) {
      uint8_t lId = parse<uint8_t>();
      uint32_t lPayloadSize = parse_uleb128<uint32_t>();

      switch(lId) {
        case 0: {
          uint8_t *lBefore = mCurrent;
          uint32_t lNameSize = parse_uleb128<uint32_t>();
          size_t lNameSizeBytes = mCurrent - lBefore;
          std::string_view lName = parse_str(lNameSize);
          size_t lInternalSize = lPayloadSize - lNameSizeBytes - lName.size();
          mCustomSections[lName] = {mCurrent, lInternalSize};
          parse_custom_section(lName, lInternalSize);
        } break;
        case 1: parse_types(); break;
        case 2: parse_imports(pContextImportNs); break;
        case 3: parse_functions(); break;
        case 4: parse_section_table(lPayloadSize); break;
        case 5: parse_section_memory(lPayloadSize); break;
        case 6: parse_globals(); break;
        case 7: parse_exports(); break;
        case 8: parse_section_start(lPayloadSize); break;
        case 9: parse_section_element(lPayloadSize); break;
        case 10: parse_section_code(lPayloadSize); break;
        case 11: parse_section_data(lPayloadSize); break;
        default:
          throw malformed_exception("unknown section ID");
      }
    }
  }

  void module::parse_custom_section(const std::string_view &pName, size_t pInternalSize) {
    if (pName == "name") {
      // "name" http://webassembly.org/docs/binary-encoding/#name-section
      parse_names(pInternalSize);
      mCurrent += pInternalSize;
    }
    else if (pName == "dylink") {
      // TODO https://github.com/WebAssembly/tool-conventions/blob/master/DynamicLinking.md
      mCurrent += pInternalSize;
    }
    else {
      mCurrent += pInternalSize;
#ifdef WEMBED_VERBOSE
      if (!mDebugSupport || (mDebugSupport && pName.substr(0, strlen(".debug_")) != ".debug_"))
        std::cout << "Ignoring custom section " << pName << " of size " << pInternalSize << std::endl;
#endif
    }
  }

  void module::parse_names(size_t pInternalSize) {
    // TODO
    uint8_t *lEnd = mCurrent + pInternalSize;
    while (mCurrent < lEnd) {
      uint8_t lType = parse<uint8_t>();
      uint32_t lSectionSize = parse_uleb128<uint32_t>();
      switch (lType) {
        default:
          mCurrent += lSectionSize;
          break;
        case 1: {
          uint32_t lCount = parse_uleb128<uint32_t>();
          for (uint32_t lName = 0; lName < lCount; lName++) {
            uint32_t lIndex = parse_uleb128<uint32_t>();
            uint32_t lNameSize = parse_uleb128<uint32_t>();
            auto lNameStr(parse_str(lNameSize));
            if (lIndex >= mFunctions.size())
              throw invalid_exception("function index out of bounds");
            std::stringstream lNewName;
            lNewName << "wasm.names.func." << lNameStr;
            std::string lNewNameStr = lNewName.str();
#ifdef WEMBED_VERBOSE
            std::cout << "Renaming " << mFunctions[lIndex].mValue << " aka "
                      << LLVMGetValueName(mFunctions[lIndex].mValue) << " to " << lNewNameStr << std::endl;
#endif
            LLVMSetValueName2(mFunctions[lIndex].mValue, lNewNameStr.data(), lNewNameStr.size());
          }
        } break;
      }
    }
  }

  module::Section module::get_custom_section(const std::string_view &pName) {
    auto it = mCustomSections.find(pName);
    if (it == mCustomSections.end())
      return {nullptr, 0};
    return it->second;
  }

  std::string_view module::get_debug_string(size_t pOffset) {
    return std::string_view((char*)mDebugStrSection.mStart + pOffset);
  }

  LLVMMetadataRef module::get_debug_rel_file(LLVMDIBuilderRef pDIBuilder, const std::string_view &pFile, const std::string_view &pPath) {
    auto lAbsPath = std::filesystem::path(pPath) / pFile;
    auto lDir = lAbsPath.parent_path().string();
    auto lFilename = lAbsPath.filename().string();
    LLVMMetadataRef lRef = LLVMDIBuilderCreateFile(pDIBuilder, lFilename.data(), lFilename.size(),
                                                   lDir.data(), lDir.size());
    return lRef;
  }

  LLVMMetadataRef module::get_debug_abs_file(LLVMDIBuilderRef pDIBuilder, const std::string_view &pAbsPath) {
    // FIXME This is bugged if pAbsPath is a URI
    auto lAbsPath = std::filesystem::path(pAbsPath);
    auto lDir = lAbsPath.parent_path().string();
    auto lFilename = lAbsPath.filename().string();
    LLVMMetadataRef lRef = LLVMDIBuilderCreateFile(pDIBuilder, lFilename.data(), lFilename.size(),
                                                   lDir.data(), lDir.size());
    return lRef;
  }

  void module::parse_debug_infos() {
    using namespace llvm::dwarf;
    // As debug infos are not in dependency order (for example debug_info needs debug_abbrev)
    // We defer parsing here, after parsing the module

    mDbgKind = LLVMGetMDKindID("dbg", 3);
    mDebugStrSection = get_custom_section(".debug_str");
    parse_debug_abbrev_entries();
    parse_debug_lines();

    LLVMValueRef lDIVersionValue = LLVMConstInt(LLVMInt32Type(), LLVMDebugMetadataVersion(), false);
    LLVMMetadataRef lDIVersionMD = LLVMValueAsMetadata(lDIVersionValue);
    LLVMAddModuleFlag(mModule, LLVMModuleFlagBehaviorWarning,
        "Debug Info Version", strlen("Debug Info Version"), lDIVersionMD);

    LLVMValueRef lDwarfVersionValue = LLVMConstInt(LLVMInt32Type(), 4, false);
    LLVMMetadataRef lDwarfVersionMD = LLVMValueAsMetadata(lDwarfVersionValue);
    LLVMAddModuleFlag(mModule, LLVMModuleFlagBehaviorWarning,
                      "Dwarf Version", strlen("Dwarf Version"), lDwarfVersionMD);

    Section lDebugInfo = get_custom_section(".debug_info");
    uint8_t *lDebugInfoStart = lDebugInfo.mStart;
    uint8_t *lDebugInfoEnd = lDebugInfoStart + lDebugInfo.mSize;
    uint8_t *lPointer = lDebugInfoStart;
    while (lPointer < lDebugInfoEnd) {
      uint8_t *lSectionStart = lPointer;
      ptrdiff_t lSectionOffset = lSectionStart - lDebugInfoStart;
      uint32_t lSectionSize = parse<uint32_t>(lPointer);
      uint8_t *lSectionEnd = lPointer + lSectionSize;
      uint16_t lDwarfVersion = parse<uint16_t>(lPointer);
      uint32_t lAbbrevOffset = parse<uint32_t>(lPointer);
      AbbrevDecls &lAbbrevDecls = mParsedAbbrevs[lAbbrevOffset];
      uint8_t lAddrSize = *lPointer++;

      /*std::cout << (void*)(lSectionOffset) << ": Compile Unit: length = "
                << lSectionSize << " version = " << lDwarfVersion << " abbr_offset = " << lAbbrevOffset
                << " addr_size = " << (unsigned)lAddrSize << std::endl;*/

      // Expect 32bit debug info, and DWARF4
      if (lAddrSize != 4 || lDwarfVersion != 4)
        return;

      std::stack<DIE*> lHierarchy;

      while (lPointer < lSectionEnd) {
        uint8_t *lDieStart = lPointer;
        size_t lAbbrevCode = parse_uleb128<size_t>(lPointer);
        AbbrevDecl &lDecl = lAbbrevDecls[lAbbrevCode];
        DIE lCurrent;
        lCurrent.mTag = lDecl.mTag;
        lCurrent.mOffset = lDieStart - lDebugInfoStart;

        /*std::cout << (void*)(lDieStart - lDebugInfoStart) << ": " << TagString(lDecl.mTag).data() << " "
                  << ChildrenString((unsigned)lDecl.mHasChild).data() << std::endl;*/

        for (const auto &lAttr : lDecl.mAttributes) {
          /*std::cout << "\t" << AttributeString(lAttr.mAttribute).data() << "\t"
                    << FormEncodingString(lAttr.mForm).data() << "\t";*/
          switch (lAttr.mForm) {
            case DW_FORM_addr: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint32_t>(lPointer); break;
            case DW_FORM_block1: {
              uint8_t lSize = parse<uint8_t>(lPointer);
              std::string_view lData((char*)lPointer, lSize);
              lPointer += lSize;
              lCurrent.mAttributes[lAttr.mAttribute] = lData;
            } break;
            case DW_FORM_block2: {
              uint16_t lSize = parse<uint16_t>(lPointer);
              std::string_view lData((char*)lPointer, lSize);
              lPointer += lSize;
              lCurrent.mAttributes[lAttr.mAttribute] = lData;
            } break;
            case DW_FORM_block4: {
              uint32_t lSize = parse<uint32_t>(lPointer);
              std::string_view lData((char*)lPointer, lSize);
              lPointer += lSize;
              lCurrent.mAttributes[lAttr.mAttribute] = lData;
            } break;
            case DW_FORM_block: {
              uint64_t lSize = parse_uleb128<uint64_t>(lPointer);
              std::string_view lData((char*)lPointer, lSize);
              lPointer += lSize;
              lCurrent.mAttributes[lAttr.mAttribute] = lData;
            } break;
            case DW_FORM_data1: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint8_t>(lPointer); break;
            case DW_FORM_data2: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint16_t>(lPointer); break;
            case DW_FORM_data4: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint32_t>(lPointer); break;
            case DW_FORM_data8: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint64_t>(lPointer); break;
            case DW_FORM_sdata: lCurrent.mAttributes[lAttr.mAttribute] = parse_sleb128<int64_t>(lPointer); break;
            case DW_FORM_udata: lCurrent.mAttributes[lAttr.mAttribute] = parse_uleb128<uint64_t>(lPointer); break;
            case DW_FORM_exprloc: {
              // FIXME Can't find in doc what's the type of the first byte in expressions
              uint64_t lSize = parse_uleb128<uint64_t>(lPointer);
              std::string_view lExpr((char*)lPointer, lSize);
              lPointer += lSize;
              lCurrent.mAttributes[lAttr.mAttribute] = lExpr;
            } break;
            case DW_FORM_flag: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint8_t>(lPointer); break;
            case DW_FORM_flag_present: lCurrent.mAttributes[lAttr.mAttribute] = 1; break;
            case DW_FORM_sec_offset: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint32_t>(lPointer); break;
            case DW_FORM_ref1: lCurrent.mAttributes[lAttr.mAttribute] = lSectionOffset + parse<uint8_t>(lPointer); break;
            case DW_FORM_ref2: lCurrent.mAttributes[lAttr.mAttribute] = lSectionOffset + parse<uint16_t>(lPointer); break;
            case DW_FORM_ref4: lCurrent.mAttributes[lAttr.mAttribute] = lSectionOffset + parse<uint32_t>(lPointer); break;
            case DW_FORM_ref8: lCurrent.mAttributes[lAttr.mAttribute] = lSectionOffset + parse<uint64_t>(lPointer); break;
            case DW_FORM_ref_udata: lCurrent.mAttributes[lAttr.mAttribute] = lSectionOffset + parse_uleb128<uint64_t>(lPointer); break;
            case DW_FORM_ref_addr: lCurrent.mAttributes[lAttr.mAttribute] = parse<uint32_t>(lPointer); break;
            case DW_FORM_string: {
              std::string_view lString((char*)lPointer);
              lPointer += lString.size() + 1;
              lCurrent.mAttributes[lAttr.mAttribute] = lString;
            } break;
            case DW_FORM_strp: {
              auto lString = get_debug_string(parse<uint32_t>(lPointer));
              lCurrent.mAttributes[lAttr.mAttribute] = lString;
            } break;
            default:
              throw std::runtime_error(std::string("unknown form type ") + FormEncodingString(lAttr.mForm).data());
          }
          /*if (std::holds_alternative<std::string_view>(lCurrent.mAttributes[lAttr.mAttribute]))
            std::cout << std::get<std::string_view>(lCurrent.mAttributes[lAttr.mAttribute]);
          else
            std::cout << std::hex << std::get<uint64_t>(lCurrent.mAttributes[lAttr.mAttribute]) << std::dec;
          std::cout << std::endl;*/
        }

        DIE *lCurentPtr;
        if (lDecl.mTag == DW_TAG_null) {
          lHierarchy.pop();
        }
        else {
          if (!lHierarchy.empty()) {
            lCurrent.mParent = lHierarchy.top();
            lCurentPtr = &lHierarchy.top()->mChildren.emplace_back(lCurrent);
          }
          else {
            lCurrent.mParent = nullptr;
            lCurentPtr = &mParsedDI.emplace_back(lCurrent);
          }
          mDIOffsetCache[lCurrent.mOffset] = lCurentPtr;
          if (lDecl.mHasChild)
            lHierarchy.push(lCurentPtr);
        }
      }
    }

    for (auto &lDIE : mParsedDI) {
      LLVMDIBuilderRef lDIBuilder = LLVMCreateDIBuilder(mModule);
      apply_die_metadata(lDIBuilder, lDIE);
      LLVMDIBuilderFinalize(lDIBuilder);
      LLVMDisposeDIBuilder(lDIBuilder);
    }

  }

  void module::parse_debug_abbrev_entries() {
    using namespace llvm::dwarf;
    Section lSection = get_custom_section(".debug_abbrev");
    uint8_t *lPointer = lSection.mStart;
    uint8_t *lEnd = lPointer + lSection.mSize;
    while (lPointer < lEnd) {
      ptrdiff_t lOffset = lPointer - lSection.mStart;
      //std::cout << "Abbrev table for offset: 0x" << std::hex << lOffset << std::dec << std::endl;
      AbbrevDecls lDecls;
      while (true) {
        AbbrevDecl lDecl;
        size_t lCode = parse_uleb128<size_t>(lPointer);
        if (lCode == 0)
          break;
        lDecl.mTag = (Tag)parse_uleb128<uint16_t>(lPointer);
        lDecl.mHasChild = *lPointer++ == DW_CHILDREN_yes;
        //std::cout << "[" << lCode << "] " << TagString(lDecl.mTag).data() << " " << ChildrenString((unsigned)lDecl.mHasChild).data() << std::endl;
        while (true) {
          AbbrevAttr lAttr;
          lAttr.mAttribute = (Attribute)parse_uleb128<uint16_t>(lPointer);
          lAttr.mForm = (Form)parse_uleb128<uint16_t>(lPointer);
          if (lAttr.mAttribute == 0 && lAttr.mForm == 0)
            break;
          //std::cout << "\t" << AttributeString(lAttr.mAttribute).data() << "\t" << FormEncodingString(lAttr.mForm).data() << std::endl;
          lDecl.mAttributes.emplace_back(lAttr);
        }
        lDecls[lCode] = lDecl;
      }
      mParsedAbbrevs[lOffset] = lDecls;
    }
  }

  void module::LineContext::append_row(const LineState &pState) {
    /*std::cout << std::setw(10) << reinterpret_cast<void*>(pState.mAddress)
              << "\t" << std::setw(6) << pState.mLine
              << "\t" << std::setw(6) << pState.mColumn
              << "\t" << std::setw(6) << pState.mFile
              << "\t" << std::setw(3) << pState.mISA
              << "\t" << std::setw(13) << pState.mDiscriminator << "\t";
    if (pState.mIsStmt) std::cout << "is ";
    if (pState.mBasicBlock) std::cout << "bb ";
    if (pState.mEndSequence) std::cout << "es ";
    if (pState.mEpilogueBeg) std::cout << "eb ";
    if (pState.mPrologueEnd) std::cout << "pe ";
    std::cout << std::endl;*/

    LineItem lItem;
    lItem.mAddress = pState.mAddress;
    lItem.mFile = pState.mFile - 1;
    lItem.mLine = pState.mLine;
    lItem.mColumn = pState.mColumn;

    mCurrentBuffer.emplace_back(lItem);

    if (pState.mEndSequence) {
      LineSequence lSequence;
      lSequence.mStartAddress = mCurrentBuffer.front().mAddress;
      lSequence.mEndAddress = mCurrentBuffer.back().mAddress;
      lSequence.mLines = std::move(mCurrentBuffer);
      mSequences.push_back(lSequence);
    }
  }

  module::LineSequence &module::LineContext::find_sequence(uint32_t pStart, uint32_t pEnd) {
    for (auto &lSequence : mSequences) {
      if (lSequence.mStartAddress == pStart && lSequence.mEndAddress == pEnd)
        return lSequence;
    }
    throw std::runtime_error("can't find inst sequence");
  }

  LLVMValueRef module::find_func_by_range(uint32_t pStart, uint32_t pEnd) {
    for (auto &lFuncRange : mFuncRanges) {
      if (lFuncRange.mStartAddress == pStart && lFuncRange.mEndAddress == pEnd)
        return lFuncRange.mFunc;
    }
    return nullptr;
  }

  void module::parse_debug_lines() {
    using namespace llvm::dwarf;
    Section lSection = get_custom_section(".debug_line");
    uint8_t *lPointer = lSection.mStart;
    uint8_t *lEnd = lPointer + lSection.mSize;

    while (lPointer < lEnd) {
      LineContext lContext;
      LineState lState;
      uint32_t lSectionStart = lPointer - lSection.mStart;
      //std::cout << "debug_line " << (void*)(lSectionStart) << std::endl;
      uint32_t lSectionSize = parse<uint32_t>(lPointer);
      uint8_t *lSectionEnd = lPointer + lSectionSize;
      /*uint16_t lDwarfVersion =*/ parse<uint16_t>(lPointer);
      /*uint32_t lHeaderSize =*/ parse<uint32_t>(lPointer);
      uint8_t lMinInstrLength = *lPointer++;
      uint8_t lMaxInstrLength = *lPointer++;
      uint8_t lDefaultIsStmt = *lPointer++;
      lState.reset(lDefaultIsStmt);
      int8_t lLineBase = *lPointer++;
      int8_t lLineRange = *lPointer++;
      uint8_t lOpcodeBase = *lPointer++;
      std::string_view lOpcodeLengths((char*)lPointer, lOpcodeBase - 1);
      lPointer += lOpcodeLengths.size();
      while (true) {
        std::string_view lIncludeDir((char*)lPointer);
        lPointer += lIncludeDir.size() + 1;
        if (lIncludeDir.empty())
          break;
        lContext.mDirectories.emplace_back(lIncludeDir);
      }
      while (true) {
        LineFileEntry lFile;
        std::string_view lFileName((char*)lPointer);
        lPointer += lFileName.size() + 1;
        if (lFileName.empty())
          break;
        lFile.mFilename = lFileName;
        lFile.mDirectory = parse_uleb128<uint64_t>(lPointer);
        lFile.mLastModification = parse_uleb128<uint64_t>(lPointer);
        lFile.mByteSize = parse_uleb128<uint64_t>(lPointer);
        lContext.mFiles.emplace_back(lFile);
      }
      while (lPointer < lSectionEnd) {
        LineNumberOps lOpcode = (LineNumberOps)*lPointer++;
        if (lOpcode < lOpcodeBase) {
          //std::cout << LNStandardString(lOpcode).data() << std::endl;
          switch (lOpcode) {
            case DW_LNS_copy: {
              lContext.append_row(lState);
              lState.mDiscriminator = 0;
              lState.mBasicBlock = false;
              lState.mPrologueEnd = false;
              lState.mEpilogueBeg = false;
            } break;
            case DW_LNS_const_add_pc: {
              uint8_t lAdjustedOp = (uint8_t)255 - lOpcodeBase;
              uint32_t lAdvanceOp = lAdjustedOp / lLineRange;
              lState.mAddress += lMinInstrLength * ((lState.mOpIndex + lAdvanceOp) / lMaxInstrLength);
              lState.mOpIndex = (lState.mOpIndex + lAdvanceOp) % lMaxInstrLength;
            } break;
            case DW_LNS_fixed_advance_pc: {
              lState.mAddress += parse_uleb128<uint16_t>(lPointer);
              lState.mOpIndex = 0;
            } break;
            case DW_LNS_advance_pc: lState.mAddress += parse_uleb128<uint32_t>(lPointer) * lMinInstrLength; break;
            case DW_LNS_advance_line: lState.mLine += parse_sleb128<int32_t>(lPointer); break;
            case DW_LNS_set_file: lState.mFile = parse_uleb128<uint32_t>(lPointer); break;
            case DW_LNS_set_column: lState.mColumn = parse_uleb128<uint32_t>(lPointer); break;
            case DW_LNS_negate_stmt: lState.mIsStmt = !lState.mIsStmt; break;
            case DW_LNS_set_basic_block: lState.mBasicBlock = true; break;
            case DW_LNS_set_prologue_end: lState.mPrologueEnd = true; break;
            case DW_LNS_set_epilogue_begin: lState.mEpilogueBeg = true; break;
            case DW_LNS_set_isa: lState.mISA = parse_uleb128<uint32_t>(lPointer); break;

            case DW_LNS_extended_op: {
              uint64_t lExtendedOpcodeSize = parse_uleb128<uint64_t>(lPointer);
              LineNumberExtendedOps lExtendedOpcode = (LineNumberExtendedOps)*lPointer++;
              //std::cout << LNExtendedString(lExtendedOpcode).data() << std::endl;
              switch (lExtendedOpcode) {
                case DW_LNE_end_sequence: {
                  lState.mEndSequence = true;
                  lContext.append_row(lState);
                  lState.reset(lDefaultIsStmt);
                } break;
                case DW_LNE_set_address: {
                  lState.mAddress = parse<uint32_t>(lPointer);
                  lState.mOpIndex = 0;
                } break;
                case DW_LNE_define_file: {
                  LineFileEntry lFile;
                  std::string_view lFileName((char*)lPointer);
                  lPointer += lFileName.size() + 1;
                  lFile.mFilename = lFileName;
                  lFile.mDirectory = parse_uleb128<uint64_t>(lPointer);
                  lFile.mLastModification = parse_uleb128<uint64_t>(lPointer);
                  lFile.mByteSize = parse_uleb128<uint64_t>(lPointer);
                  lContext.mFiles.emplace_back(lFile);
                } break;
                case DW_LNE_set_discriminator: {
                  lState.mDiscriminator = parse_uleb128<uint32_t>(lPointer);
                } break;
                default:
                  lPointer += lExtendedOpcodeSize;
                  break;
              }
            } break;
          }
        }
        else {
          uint8_t lAdjustedOp = lOpcode - lOpcodeBase;
          //std::cout << "Special Opcode: " << (unsigned)lAdjustedOp << std::endl;
          uint32_t lAdvanceOp = lAdjustedOp / lLineRange;
          lState.mAddress += lMinInstrLength * ((lState.mOpIndex + lAdvanceOp) / lMaxInstrLength);
          lState.mOpIndex = (lState.mOpIndex + lAdvanceOp) % lMaxInstrLength;
          lState.mLine += lLineBase + (lAdjustedOp % lLineRange);
          lContext.append_row(lState);
          lState.mBasicBlock = false;
          lState.mPrologueEnd = false;
          lState.mEpilogueBeg = false;
          lState.mDiscriminator = 0;
        }
      }
      mParsedLines[lSectionStart] = lContext;
    }
  }

  void module::LineState::reset(bool pDefaultIsStmt) {
    mAddress = 0;
    mOpIndex = 0;
    mFile = 1;
    mLine = 1;
    mColumn = 0;
    mIsStmt = pDefaultIsStmt;
    mBasicBlock = false;
    mEndSequence = false;
    mPrologueEnd = false;
    mEpilogueBeg = false;
    mISA = 0;
    mDiscriminator = 0;
  }

  std::vector<module::DIE*> module::DIE::children(llvm::dwarf::Tag pTag) {
    std::vector<module::DIE*> lResult;
    for (auto &lChild : mChildren) {
      if (lChild.mTag == pTag)
        lResult.push_back(&lChild);
    }
    return lResult;
  }

  module::DIE* module::DIE::parent(llvm::dwarf::Tag pTag) {
    DIE *lCurrent = this;
    while (lCurrent != nullptr) {
      if (lCurrent->mTag == pTag)
        break;
      lCurrent = lCurrent->mParent;
    }
    return lCurrent;
  }

  LLVMMetadataRef module::get_debug_file(LLVMDIBuilderRef pDIBuilder, DIE *pContext, uint64_t pIndex) {
    using namespace llvm::dwarf;
    if (pIndex == 0)
      return nullptr;
    auto lCU = pContext->parent(DW_TAG_compile_unit);
    uint64_t lLinesOffset = lCU->attr<uint64_t>(DW_AT_stmt_list, -1);
    assert(lLinesOffset != -1);
    auto lContext = mParsedLines.find(lLinesOffset);
    assert(lContext != mParsedLines.end());
    const auto &lFile = lContext->second.mFiles[pIndex - 1];
    return get_debug_rel_file(pDIBuilder, lFile.mFilename, lContext->second.mDirectories[lFile.mDirectory - 1]);
  }

  void module::apply_die_metadata_on_child(LLVMDIBuilderRef pDIBuilder, DIE &pDIE) {
    for (DIE &pChild : pDIE.mChildren) {
      auto lAlreadyEvaluating = mEvaluatingDIE.find(&pChild);
      if (lAlreadyEvaluating == mEvaluatingDIE.end())
        apply_die_metadata(pDIBuilder, pChild);
    }
  }

  void module::apply_die_metadata(LLVMDIBuilderRef pDIBuilder, DIE &pDIE) {
    using namespace llvm::dwarf;
    if (pDIE.mMetadata != nullptr)
      return;

    mEvaluatingDIE.emplace(&pDIE);

    auto lType = pDIE.attr<uint64_t>(DW_AT_type);
    if (lType) {
      auto lTypeDie = mDIOffsetCache.find(lType);
      assert(lTypeDie != mDIOffsetCache.end());
      apply_die_metadata(pDIBuilder, *lTypeDie->second);
    }

    switch (pDIE.mTag) {
      case DW_TAG_base_type: {
        auto lByteSize = pDIE.attr<uint64_t>(DW_AT_byte_size);
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lEncoding = pDIE.attr<uint64_t>(DW_AT_encoding);

        pDIE.mMetadata = LLVMDIBuilderCreateBasicType(pDIBuilder, lName.data(), lName.size(),
            lByteSize * 8, (LLVMDWARFTypeEncoding)lEncoding, LLVMDIFlagZero);
      } break;
      case DW_TAG_pointer_type: {
        LLVMMetadataRef lPointee = nullptr;
        std::string lName;
        if (lType) {
          auto lTypeDie = mDIOffsetCache.find(lType);
          if (lTypeDie == mDIOffsetCache.end()) break;
          lPointee = lTypeDie->second->mMetadata;
        }
        if (lPointee != nullptr) {
          size_t lPointeeNameLength;
          const char *lPointeeName = LLVMDITypeGetName(lPointee, &lPointeeNameLength);
          lName = std::string(lPointeeName, lPointeeNameLength);
          lName += '*';
        }
        else {
          lName = "void*";
        }

        pDIE.mMetadata = LLVMDIBuilderCreatePointerType(pDIBuilder, lPointee, 32, 32, 0, lName.data(), lName.size());
      } break;
      case DW_TAG_typedef: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end()) break;
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        pDIE.mMetadata = LLVMDIBuilderCreateTypedef(pDIBuilder, lTypeDie->second->mMetadata,
            lName.data(), lName.size(), lFile, lLine, pDIE.mParent->mMetadata);
      } break;
      case DW_TAG_subroutine_type: {
        std::vector<LLVMMetadataRef> lParamTypes;
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end())
          lParamTypes.push_back(nullptr);
        else
          lParamTypes.push_back(lTypeDie->second->mMetadata);

        auto lParamTypeOffsets = pDIE.children_attr<uint64_t>(DW_TAG_formal_parameter, DW_AT_type);
        for (uint64_t lParamTypeOffset : lParamTypeOffsets) {
          auto lParamTypeDIE = mDIOffsetCache.find(lParamTypeOffset);
          assert(lParamTypeDIE != mDIOffsetCache.end());
          apply_die_metadata(pDIBuilder, *lParamTypeDIE->second);
          lParamTypes.emplace_back(lParamTypeDIE->second->mMetadata);
        }
        auto lCU = pDIE.parent(DW_TAG_compile_unit);
        auto lName = lCU->attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_abs_file(pDIBuilder, lName);
        pDIE.mMetadata = LLVMDIBuilderCreateSubroutineType(pDIBuilder, lFile, lParamTypes.data(), lParamTypes.size(),
                                                       LLVMDIFlagZero);
      } break;
      case DW_TAG_restrict_type:
      case DW_TAG_const_type:
      case DW_TAG_volatile_type: {
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end()) break;
        auto lUnderlyingType = lTypeDie->second->mMetadata;

        pDIE.mMetadata = LLVMDIBuilderCreateQualifiedType(pDIBuilder, pDIE.mTag, lUnderlyingType);
      } break;

      case DW_TAG_enumerator: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lValue = pDIE.attr<uint64_t >(DW_AT_const_value);
        auto lParentType = pDIE.mParent->attr<uint64_t>(DW_AT_type);
        auto lParentTypeDie = mDIOffsetCache.find(lParentType);
        if (lParentTypeDie == mDIOffsetCache.end()) break;
        auto lParentTypeEncoding = lParentTypeDie->second->attr<uint64_t>(DW_AT_encoding);
        bool lUnsigned;
        switch (lParentTypeEncoding) {
          case DW_ATE_signed:
          case DW_ATE_signed_char:
          case DW_ATE_signed_fixed:
            lUnsigned = false;
            break;
          default:
            lUnsigned = true;
        }

        pDIE.mMetadata = LLVMDIBuilderCreateEnumerator(pDIBuilder, lName.data(), lName.size(), lValue, lUnsigned);
      } break;
      case DW_TAG_enumeration_type: {
        auto lByteSize = pDIE.attr<uint64_t>(DW_AT_byte_size);
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end()) break;
        auto lUnderlyingType = lTypeDie->second->mMetadata;

        apply_die_metadata_on_child(pDIBuilder, pDIE);
        auto lEnumerators = pDIE.children(DW_TAG_enumerator);
        std::vector<LLVMMetadataRef> lEnumeratorRefs;
        for (auto &lEnumerator : lEnumerators)
          lEnumeratorRefs.emplace_back(lEnumerator->mMetadata);

        pDIE.mMetadata = LLVMDIBuilderCreateEnumerationType(pDIBuilder, pDIE.mParent->mMetadata,
            lName.data(), lName.size(), lFile, lLine,
            lByteSize * 8, 32, lEnumeratorRefs.data(),
            lEnumeratorRefs.size(), lUnderlyingType);
      } break;

      case DW_TAG_subrange_type: {
        pDIE.mMetadata = LLVMDIBuilderGetOrCreateSubrange(pDIBuilder,
            pDIE.attr<uint64_t>(DW_AT_lower_bound),
            pDIE.attr<uint64_t>(DW_AT_count));
      } break;
      case DW_TAG_array_type: {
        apply_die_metadata_on_child(pDIBuilder, pDIE);
        auto lSubranges = pDIE.children(DW_TAG_subrange_type);
        std::vector<LLVMMetadataRef> lSubrangeRefs;
        uint64_t lTotalSize = 0;
        for (auto &lSubrange : lSubranges) {
          lTotalSize += lSubrange->attr<uint64_t>(DW_AT_count);
          lSubrangeRefs.emplace_back(lSubrange->mMetadata);
        }
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end()) break;

        pDIE.mMetadata = LLVMDIBuilderCreateArrayType(pDIBuilder, lTotalSize,
            32, lTypeDie->second->mMetadata,
            lSubrangeRefs.data(), lSubrangeRefs.size());
      } break;

      case DW_TAG_member: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lOffsetByte = pDIE.attr<uint64_t>(DW_AT_data_member_location);
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end()) break;
        auto lTypeByteSize = lTypeDie->second->attr<uint64_t>(DW_AT_byte_size, 4);
        auto lBitSize = pDIE.attr<uint64_t>(DW_AT_bit_size, lTypeByteSize * 8);
        auto lBitOffset = pDIE.attr<uint64_t>(DW_AT_bit_offset);

        pDIE.mMetadata = LLVMDIBuilderCreateMemberType(
            pDIBuilder, pDIE.mParent->mMetadata, lName.data(),
            lName.size(), lFile, lLine,
            lBitSize, 32, lOffsetByte * 8 + lBitOffset,
            LLVMDIFlagZero, lTypeDie->second->mMetadata);
      } break;
      case DW_TAG_structure_type: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lByteSize = pDIE.attr<uint64_t>(DW_AT_byte_size);
        apply_die_metadata_on_child(pDIBuilder, pDIE);
        auto lMembers = pDIE.children(DW_TAG_member);
        std::vector<LLVMMetadataRef> lMemberRefs;
        for (auto &lMember : lMembers)
          lMemberRefs.emplace_back(lMember->mMetadata);

        pDIE.mMetadata = LLVMDIBuilderCreateStructType(
            pDIBuilder, pDIE.mParent->mMetadata, lName.data(),
            lName.size(), lFile, lLine,
            lByteSize * 8, 32, LLVMDIFlagZero,
            nullptr, lMemberRefs.data(),
            lMemberRefs.size(), 0, nullptr,
            lName.data(), lName.size());
      } break;
      case DW_TAG_union_type: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lByteSize = pDIE.attr<uint64_t>(DW_AT_byte_size);
        apply_die_metadata_on_child(pDIBuilder, pDIE);
        std::vector<LLVMMetadataRef> lChilds;
        for (auto &lMember : pDIE.mChildren)
          lChilds.emplace_back(lMember.mMetadata);

        pDIE.mMetadata = LLVMDIBuilderCreateUnionType(
            pDIBuilder, pDIE.mParent->mMetadata, lName.data(),
            lName.size(), lFile, lLine,
            lByteSize * 8, 32, LLVMDIFlagZero,
            lChilds.data(), lChilds.size(), 0,
            nullptr, 0);
      } break;

      case DW_TAG_unspecified_parameters: {
        // noop
      } break;
      case DW_TAG_lexical_block: {
        // TODO
      } break;
      case DW_TAG_variable: {
        // TODO
      } break;

      case DW_TAG_formal_parameter: {
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lParentParams = pDIE.mParent->children(DW_TAG_formal_parameter);
        assert(lParentParams.size() > 0);
        size_t lIndex = 0;
        for (; lIndex < lParentParams.size(); lIndex++) {
          if (lParentParams[lIndex] == &pDIE)
            break;
        }
        auto lTypeDie = mDIOffsetCache.find(lType);
        if(lTypeDie != mDIOffsetCache.end())
          break;

        if (!lName.empty()) {
          pDIE.mMetadata = LLVMDIBuilderCreateParameterVariable(
              pDIBuilder, pDIE.mParent->mMetadata, lName.data(), lName.size(), lIndex + 1,
              lFile, lLine, lTypeDie->second->mMetadata, true, LLVMDIFlagZero);
        }
      } break;

      case DW_TAG_subprogram: {
        // FIXME Support inlined function
        if (pDIE.attr<uint64_t>(DW_AT_abstract_origin))
          break;
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_file(pDIBuilder, &pDIE, pDIE.attr<uint64_t>(DW_AT_decl_file));
        auto lLine = pDIE.attr<uint64_t>(DW_AT_decl_line);
        auto lExternal = pDIE.attr<uint64_t>(DW_AT_external);
        std::vector<LLVMMetadataRef> lParamTypes;
        auto lTypeDie = mDIOffsetCache.find(lType);
        if (lTypeDie == mDIOffsetCache.end())
          lParamTypes.push_back(nullptr);
        else
          lParamTypes.push_back(lTypeDie->second->mMetadata);
        auto lParamTypeOffsets = pDIE.children_attr<uint64_t>(DW_TAG_formal_parameter, DW_AT_type);
        for (uint64_t lParamTypeOffset : lParamTypeOffsets) {
          auto lParamTypeDIE = mDIOffsetCache.find(lParamTypeOffset);
          assert(lParamTypeDIE != mDIOffsetCache.end());
          apply_die_metadata(pDIBuilder, *lParamTypeDIE->second);
          lParamTypes.emplace_back(lParamTypeDIE->second->mMetadata);
        }
        auto lSubprogramTypes = LLVMDIBuilderCreateSubroutineType(pDIBuilder, lFile, lParamTypes.data(),
            lParamTypes.size(), LLVMDIFlagZero);

        int lFlags = LLVMDIFlagZero;
        if (pDIE.attr<uint64_t>(DW_AT_prototyped))
          lFlags = LLVMDIFlagPrototyped;

        pDIE.mMetadata = LLVMDIBuilderCreateFunction(pDIBuilder, pDIE.mParent->mMetadata, lName.data(), lName.size(),
            nullptr, 0, lFile, lLine, lSubprogramTypes, !lExternal, true, 0, LLVMDIFlags(lFlags), 0);

        auto lLowPC = pDIE.attr<uint64_t>(DW_AT_low_pc, -1);
        auto lHighPC = lLowPC + pDIE.attr<uint64_t>(DW_AT_high_pc, -1);
        auto lFunc = find_func_by_range(lLowPC, lHighPC);
        if (lFunc) {
          LLVMGlobalSetMetadata(lFunc, mDbgKind, pDIE.mMetadata);

          /*std::cout << "At subprogram " << lName << "(aka " << (lFunc ? LLVMGetValueName(lFunc) : "null") << ") for range "
                    << (void*)lLowPC << ", " << (void*)lHighPC << std::endl;*/

          auto lCUDie = pDIE.parent(DW_TAG_compile_unit);
          auto lStatements = lCUDie->attr<uint64_t>(DW_AT_stmt_list, -1);
          if (lStatements != -1 && lLowPC != -1) {
            auto &lLines = mParsedLines[lStatements].find_sequence(lLowPC, lHighPC);
            for (auto &lLineItem : lLines.mLines) {
              auto lInstr = mInstructions.find(lLineItem.mAddress);
              if (lInstr == mInstructions.end())
                continue;
              lLineItem.mLoc = LLVMDIBuilderCreateDebugLocation(LLVMGetGlobalContext(), lLineItem.mLine, lLineItem.mColumn, pDIE.mMetadata, nullptr);
              auto lLocAsValue = LLVMMetadataAsValue(LLVMGetGlobalContext(), lLineItem.mLoc);
              //std::cout << "Attaching " << (void*)lLineItem.mAddress << " " << lLineItem.mLine << ":" << lLineItem.mColumn << " to: " << lInstr->second << " " << LLVMPrintValueToString(lInstr->second) << std::endl;
              LLVMSetMetadata(lInstr->second, mDbgKind, lLocAsValue);
            }

            // Avoids "inlinable function call in a function with debug info must have a !dbg location"
            // errors by copying a close debug location to call instructions missing one
            LLVMValueRef lLastLoc = nullptr;
            std::vector<LLVMValueRef> lTodo;
            if (lFunc != nullptr) {
              LLVMBasicBlockRef lBlock = LLVMGetFirstBasicBlock(lFunc);
              if (lBlock != nullptr) {
                while (lBlock != nullptr) {
                  LLVMValueRef lInstr = LLVMGetFirstInstruction(lBlock);
                  while (lInstr != nullptr) {
                    LLVMValueRef lLoc = LLVMGetMetadata(lInstr, mDbgKind);
                    if (lLoc != nullptr) {
                      lLastLoc = lLoc;
                      for (auto &lItem : lTodo)
                        LLVMSetMetadata(lItem, mDbgKind, lLoc);
                      lTodo.clear();
                    }
                    if (LLVMGetInstructionOpcode(lInstr) == LLVMCall && lLoc == nullptr) {
                      if (lLastLoc != nullptr)
                        LLVMSetMetadata(lInstr, mDbgKind, lLastLoc);
                      else
                        lTodo.push_back(lInstr);
                    }
                    lInstr = LLVMGetNextInstruction(lInstr);
                  }
                  lBlock = LLVMGetNextBasicBlock(lBlock);
                }
              }
            }
          }
        }
      } break;

      case DW_TAG_compile_unit: {
        auto lProducer = pDIE.attr<std::string_view>(DW_AT_producer);
        auto lLang = pDIE.attr<uint64_t>(DW_AT_language);
        auto lName = pDIE.attr<std::string_view>(DW_AT_name);
        auto lFile = get_debug_abs_file(pDIBuilder, lName);
        pDIE.mMetadata = LLVMDIBuilderCreateCompileUnit(pDIBuilder, LLVMDWARFSourceLanguage(lLang - 1),
            lFile, lProducer.data(), lProducer.size(),
            0, nullptr, 0, 0, nullptr, 0, LLVMDWARFEmissionFull, 0, 0, 0);
      } break;

      default:
#ifdef WEMBED_VERBOSE
        std::cerr << "unsupported DI node " << TagString(pDIE.mTag).data() << std::endl;
#endif
        break;
    }

    mEvaluatingDIE.erase(&pDIE);
    apply_die_metadata_on_child(pDIBuilder, pDIE);
  }

  uint8_t module::bit_count(LLVMTypeRef pType) {
    if (pType == LLVMInt32Type())
      return 32;
    else if (pType == LLVMInt64Type())
      return 64;
    else if (pType == LLVMFloatType())
      return 32;
    else if (pType == LLVMDoubleType())
      return 64;
    else
      throw malformed_exception("unexpected value type");
  }

  LLVMValueRef module::emit_shift_mask(LLVMTypeRef pType, LLVMValueRef pCount) {
    auto lTemp = get_const(bit_count(pType) - 1);
    auto lWidth = LLVMBuildZExt(mBuilder, lTemp, pType, "width");
    return LLVMBuildAnd(mBuilder, pCount, lWidth, "mask");
  }

  LLVMValueRef module::emit_rotl(LLVMTypeRef pType, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    auto lWidth = LLVMBuildZExt(mBuilder, get_const(bit_count(pType)), pType, "width");
    auto lBitWidthMinusRight = LLVMBuildSub(mBuilder, lWidth, pRHS, "");
    return LLVMBuildOr(mBuilder,
                       LLVMBuildShl(mBuilder, pLHS, emit_shift_mask(pType, pRHS), ""),
                       LLVMBuildLShr(mBuilder, pLHS, emit_shift_mask(pType, lBitWidthMinusRight), ""),
                       "rotl");
  }

  LLVMValueRef module::emit_rotr(LLVMTypeRef pType, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    auto lWidth = LLVMBuildZExt(mBuilder, get_const(bit_count(pType)), pType, "width");
    auto lBitWidthMinusRight = LLVMBuildSub(mBuilder, lWidth, pRHS, "");
    return LLVMBuildOr(mBuilder,
                       LLVMBuildShl(mBuilder, pLHS, emit_shift_mask(pType, lBitWidthMinusRight), ""),
                       LLVMBuildLShr(mBuilder, pLHS, emit_shift_mask(pType, pRHS), ""),
                       "rotl");
  }

  LLVMValueRef module::emit_udiv(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    trap_zero_div(lFunc, pLHS, pRHS);
    return LLVMBuildUDiv(mBuilder, pLHS, pRHS, "udiv");
  }

  LLVMValueRef module::emit_sdiv(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    trap_szero_div(lFunc, pLHS, pRHS);
    return LLVMBuildSDiv(mBuilder, pLHS, pRHS, "sdiv");
  }

  LLVMValueRef module::emit_urem(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    trap_zero_div(lFunc, pLHS, pRHS);
    return LLVMBuildURem(mBuilder, pLHS, pRHS, "urem");
  }

  LLVMValueRef module::emit_srem(LLVMTypeRef pType, LLVMValueRef lFunc, LLVMValueRef pLHS, LLVMValueRef pRHS) {
    trap_zero_div(lFunc, pLHS, pRHS);

    LLVMBasicBlockRef lPreTest = LLVMGetInsertBlock(mBuilder);
    LLVMBasicBlockRef lSRemThen = LLVMAppendBasicBlock(lFunc, "sremElse");
    LLVMBasicBlockRef lSRemEnd = LLVMAppendBasicBlock(lFunc, "sremEnd");

    LLVMValueRef lLeftTest = LLVMBuildICmp(mBuilder, LLVMIntNE, pLHS,
                                           pType == LLVMInt32Type() ? get_const(int32_t(INT32_MIN))
                                                                    : get_const(int64_t(INT64_MIN)), "left");
    LLVMValueRef lRightTest = LLVMBuildICmp(mBuilder, LLVMIntNE, pRHS,
                                            pType == LLVMInt32Type() ? get_const(int32_t(-1))
                                                                     : get_const(int64_t(-1)), "right");
    LLVMValueRef lNoOverflow = LLVMBuildOr(mBuilder, lLeftTest, lRightTest, "noOverflow");
    LLVMBuildCondBr(mBuilder, lNoOverflow, lSRemThen, lSRemEnd);

    LLVMPositionBuilderAtEnd(mBuilder, lSRemThen);
    LLVMValueRef lSRem = LLVMBuildSRem(mBuilder, pLHS, pRHS, "rem");
    LLVMBuildBr(mBuilder, lSRemEnd);

    LLVMPositionBuilderAtEnd(mBuilder, lSRemEnd);
    LLVMValueRef lPhi = LLVMBuildPhi(mBuilder, pType, "sremRes");

    LLVMValueRef lValues[] = {
        lSRem,
        get_zero(pType)
    };
    LLVMBasicBlockRef lFroms[] = {
        lSRemThen,
        lPreTest
    };
    LLVMAddIncoming(lPhi, lValues, lFroms, 2);

    return lPhi;
  }

  LLVMValueRef module::emit_quiet_nan(LLVMValueRef pInput) {
    LLVMTypeRef lInputType = LLVMTypeOf(pInput);
    if (lInputType == LLVMFloatType()) {
      LLVMValueRef lBitcast = LLVMBuildBitCast(mBuilder, pInput, LLVMInt32Type(), "bitcast");
      LLVMValueRef lMantissaMask = get_const(~(fp_bits<float>::sMantissaMask));
      LLVMValueRef lMasked = LLVMBuildAnd(mBuilder, lBitcast, lMantissaMask, "masked");
      LLVMValueRef lQuietNan = get_const(fp_bits<float>::sQuietNan);
      LLVMValueRef lCorrected = LLVMBuildOr(mBuilder, lMasked, lQuietNan, "quietNaN");
      return LLVMBuildBitCast(mBuilder, lCorrected, LLVMFloatType(), "castedBack");
    }
    else if (lInputType == LLVMDoubleType()) {
      LLVMValueRef lBitcast = LLVMBuildBitCast(mBuilder, pInput, LLVMInt64Type(), "bitcast");
      LLVMValueRef lMantissaMask = get_const(~(fp_bits<double>::sMantissaMask));
      LLVMValueRef lMasked = LLVMBuildAnd(mBuilder, lBitcast, lMantissaMask, "masked");
      LLVMValueRef lQuietNan = get_const(fp_bits<double>::sQuietNan);
      LLVMValueRef lCorrected = LLVMBuildOr(mBuilder, lMasked, lQuietNan, "quietNaN");
      return LLVMBuildBitCast(mBuilder, lCorrected, LLVMDoubleType(), "castedBack");
    }
    throw std::runtime_error("unexpcted type passed to emit_quiet_nan");
  }

  LLVMValueRef module::emit_quiet_nan_or_intrinsic(LLVMValueRef pInput, LLVMValueRef pF32Intr, LLVMValueRef pF64Intr) {
    LLVMValueRef lNotNan = LLVMBuildFCmp(mBuilder, LLVMRealORD, pInput, pInput, "notNaN");
    LLVMTypeRef lInputType = LLVMTypeOf(pInput);
    assert(lInputType == LLVMFloatType() || lInputType == LLVMDoubleType());
    LLVMValueRef lIntrinsic = lInputType == LLVMFloatType() ? pF32Intr : pF64Intr;
    return LLVMBuildSelect(mBuilder, lNotNan, call_intrinsic(lIntrinsic, {pInput}), emit_quiet_nan(pInput), "quiet_nan_or_ceil");
  }

  LLVMValueRef module::emit_min(LLVMValueRef pLHS, LLVMValueRef pRHS) {
    LLVMTypeRef lBitcastType;
    if (LLVMTypeOf(pLHS) == LLVMFloatType() && LLVMTypeOf(pRHS) == LLVMFloatType())
      lBitcastType = LLVMInt32Type();
    else if (LLVMTypeOf(pLHS) == LLVMDoubleType() && LLVMTypeOf(pRHS) == LLVMDoubleType())
      lBitcastType = LLVMInt64Type();
    else
      throw std::runtime_error("unexpected types for emit_min");

    LLVMValueRef lBitcastLHS = LLVMBuildBitCast(mBuilder, pLHS, lBitcastType, "bitcastL");
    LLVMValueRef lBitcastRHS = LLVMBuildBitCast(mBuilder, pRHS, lBitcastType, "bitcastR");
    LLVMValueRef lBitcastCmp = LLVMBuildICmp(mBuilder, LLVMIntUGT, lBitcastLHS, lBitcastRHS, "bitcastCmp");

    LLVMValueRef lRCmp = LLVMBuildFCmp(mBuilder, LLVMRealOGT, pLHS, pRHS, "RCmp");
    LLVMValueRef lLCmp = LLVMBuildFCmp(mBuilder, LLVMRealOLT, pLHS, pRHS, "LCmp");
    LLVMValueRef lLNan = LLVMBuildFCmp(mBuilder, LLVMRealUNO, pLHS, pLHS, "LNaN");
    LLVMValueRef lRNan = LLVMBuildFCmp(mBuilder, LLVMRealUNO, pRHS, pRHS, "RNaN");

    return LLVMBuildSelect(mBuilder, lRNan, emit_quiet_nan(pRHS),
                           LLVMBuildSelect(mBuilder, lLNan, emit_quiet_nan(pLHS),
                                           LLVMBuildSelect(mBuilder, lLCmp, pLHS,
                                                           LLVMBuildSelect(mBuilder, lRCmp, pRHS,
                                                                           LLVMBuildSelect(mBuilder, lBitcastCmp, pLHS, pRHS,
                                                                                           "stepBitcast"),
                                                                           "stepRCmp"),
                                                           "stepLCmp"),
                                           "cleanLHS"),
                           "cleanRHS");
  }

  LLVMValueRef module::emit_max(LLVMValueRef pLHS, LLVMValueRef pRHS) {
    LLVMTypeRef lBitcastType;
    if (LLVMTypeOf(pLHS) == LLVMFloatType() && LLVMTypeOf(pRHS) == LLVMFloatType())
      lBitcastType = LLVMInt32Type();
    else if (LLVMTypeOf(pLHS) == LLVMDoubleType() && LLVMTypeOf(pRHS) == LLVMDoubleType())
      lBitcastType = LLVMInt64Type();
    else
      throw std::runtime_error("unexpected types for emit_max");


    LLVMValueRef lBitcastLHS = LLVMBuildBitCast(mBuilder, pLHS, lBitcastType, "bitcastL");
    LLVMValueRef lBitcastRHS = LLVMBuildBitCast(mBuilder, pRHS, lBitcastType, "bitcastR");
    LLVMValueRef lBitcastCmp = LLVMBuildICmp(mBuilder, LLVMIntULT, lBitcastLHS, lBitcastRHS, "bitcastCmp");

    LLVMValueRef lRCmp = LLVMBuildFCmp(mBuilder, LLVMRealOLT, pLHS, pRHS, "RCmp");
    LLVMValueRef lLCmp = LLVMBuildFCmp(mBuilder, LLVMRealOGT, pLHS, pRHS, "LCmp");
    LLVMValueRef lLNan = LLVMBuildFCmp(mBuilder, LLVMRealUNO, pLHS, pLHS, "LNaN");
    LLVMValueRef lRNan = LLVMBuildFCmp(mBuilder, LLVMRealUNO, pRHS, pRHS, "RNaN");

    return LLVMBuildSelect(mBuilder, lRNan, emit_quiet_nan(pRHS),
                           LLVMBuildSelect(mBuilder, lLNan, emit_quiet_nan(pLHS),
                                           LLVMBuildSelect(mBuilder, lLCmp, pLHS,
                                                           LLVMBuildSelect(mBuilder, lRCmp, pRHS,
                                                                           LLVMBuildSelect(mBuilder, lBitcastCmp, pLHS, pRHS,
                                                                                           "stepBitcast"),
                                                                           "stepRCmp"),
                                                           "stepLCmp"),
                                           "cleanLHS"),
                           "cleanRHS");
  }

  LLVMTypeRef module::parse_llvm_btype() {
    int8_t lType = parse<int8_t>();
    switch (lType) {
      case bt_i32: return LLVMInt32Type();
      case bt_i64: return LLVMInt64Type();
      case bt_f32: return LLVMFloatType();
      case bt_f64: return LLVMDoubleType();
      case bt_void: return LLVMVoidType();
      default:
        throw malformed_exception("unexpected value type");
    }
  }

  LLVMTypeRef module::parse_llvm_vtype() {
    int8_t lType = parse<int8_t>();
    switch (lType) {
      case vt_i32: return LLVMInt32Type();
      case vt_i64: return LLVMInt64Type();
      case vt_f32: return LLVMFloatType();
      case vt_f64: return LLVMDoubleType();
      default:
        throw malformed_exception("unexpected value type");
    }
  }

  std::function<LLVMValueRef()> module::parse_llvm_init(LLVMTypeRef pType) {
    std::function<LLVMValueRef()> lResult;
    uint8_t lInstr = parse<uint8_t>();
    switch (lInstr) {
      case o_const_i32: {
        if (pType != LLVMInt32Type())
          throw invalid_exception("init i32, expected other type");
        int32_t lValue = parse_sleb128<int32_t>();
        lResult = [lValue] { return LLVMConstInt(LLVMInt32Type(), lValue, true); };
      } break;
      case o_const_i64: {
        if (pType != LLVMInt64Type())
          throw invalid_exception("init i64, expected other type");
        int64_t lValue = parse_sleb128<int64_t>();
        lResult = [lValue] { return LLVMConstInt(LLVMInt64Type(), lValue, true); };
      } break;
      case o_const_f32: {
        if (pType != LLVMFloatType())
          throw invalid_exception("init f32, expected other type");
        float lValue = parse<float>();
        lResult = [lValue] { return LLVMConstReal(LLVMFloatType(), lValue); };
      } break;
      case o_const_f64: {
        if (pType != LLVMDoubleType())
          throw invalid_exception("init f64, expected other type");
        double lValue = parse<double>();
        lResult = [lValue] { return LLVMConstReal(LLVMDoubleType(), lValue); };
      } break;
      case o_get_global: {
        uint32_t lIndex = parse_sleb128<uint32_t>();
        if (lIndex >= mGlobals.size())
          throw invalid_exception("global index out of bounds");
        if (pType != LLVMGetElementType(LLVMTypeOf(mGlobals[lIndex]))) {
          throw invalid_exception("init global, expected other type");
        }
        lResult = [this, lIndex] { return LLVMBuildLoad(mBuilder, mGlobals[lIndex], "globalInit"); };
      } break;
      default: throw invalid_exception("unsupported init expr instr");
    }
    uint8_t lEnd = parse<uint8_t>();
    if (lEnd != o_end)
      throw invalid_exception("expected end after init expr");
    assert(lResult);
    return lResult;
  }

  LLVMValueRef module::get_zero(LLVMTypeRef pType) {
    if (pType == LLVMInt32Type())
      return LLVMConstInt(LLVMInt32Type(), 0, true);
    else if (pType == LLVMInt64Type())
      return LLVMConstInt(LLVMInt64Type(), 0, true);
    else if (pType == LLVMFloatType())
      return LLVMConstReal(LLVMFloatType(), 0);
    else if (pType == LLVMDoubleType())
      return LLVMConstReal(LLVMDoubleType(), 0);
    else
      throw std::runtime_error("unexpected value type");
  }

  LLVMValueRef module::get_string(const char *pString) {
    return LLVMBuildIntToPtr(mBuilder, get_const(i64(pString)), LLVMPointerType(LLVMInt8Type(), 0), "string");
  }

  LLVMValueRef module::clear_nan_internal(LLVMTypeRef pInputType, LLVMTypeRef pIntType, LLVMValueRef pInput,
                                          LLVMValueRef pConstMaskSignificand, LLVMValueRef pConstMaskExponent,
                                          LLVMValueRef pConstNanBit) {
    LLVMValueRef lRaw = LLVMBuildBitCast(mBuilder, pInput, pIntType, "raw");
    LLVMValueRef lSignificand = LLVMBuildAnd(mBuilder, lRaw, pConstMaskSignificand, "significand");
    LLVMValueRef lNotInf = LLVMBuildICmp(mBuilder, LLVMIntNE, lSignificand, get_zero(pIntType), "notInf");
    LLVMValueRef lExponent = LLVMBuildAnd(mBuilder, lRaw, pConstMaskExponent, "exponent");
    LLVMValueRef lMaxExp = LLVMBuildICmp(mBuilder, LLVMIntEQ, lExponent, pConstMaskExponent, "maxExp");
    LLVMValueRef lIsNan = LLVMBuildAnd(mBuilder, lNotInf, lMaxExp, "isNan");
    LLVMValueRef lCleanedNan = LLVMBuildOr(mBuilder, lRaw, pConstNanBit, "cleanedNan");
    LLVMValueRef lRawResult = LLVMBuildSelect(mBuilder, lIsNan, lCleanedNan, lRaw, "select_nan");
    return LLVMBuildBitCast(mBuilder, lRawResult, pInputType, "result");
  }

  LLVMValueRef module::clear_nan(LLVMValueRef pInput) {
    LLVMTypeRef lType = LLVMTypeOf(pInput);
    if (lType == LLVMFloatType()) {
      LLVMValueRef lSignificandMask = LLVMConstInt(LLVMInt32Type(), 8388607ULL, false);
      LLVMValueRef lExponentMask = LLVMConstInt(LLVMInt32Type(), 2139095040ULL, false);
      LLVMValueRef lConstNanBit = LLVMConstInt(LLVMInt32Type(), 4194304ULL, false);
      return clear_nan_internal(lType, LLVMInt32Type(), pInput, lSignificandMask, lExponentMask, lConstNanBit);
    }
    else if (lType == LLVMDoubleType()) {
      LLVMValueRef lSignificandMask = LLVMConstInt(LLVMInt64Type(), 4503599627370495ULL, false);
      LLVMValueRef lExponentMask = LLVMConstInt(LLVMInt64Type(), 9218868437227405312ULL, false);
      LLVMValueRef lConstNanBit = LLVMConstInt(LLVMInt64Type(), 2251799813685248ULL, false);
      return clear_nan_internal(lType, LLVMInt64Type(), pInput, lSignificandMask, lExponentMask, lConstNanBit);
    }
    else {
      throw std::runtime_error("wrong type while clearing nan");
    }
  }

  void module::parse_types() {
    uint32_t lCount = parse_uleb128<uint32_t>();
    mTypes.resize(lCount);
    for (size_t lType = 0; lType < lCount; lType++) {
      parse<uint8_t>(); // "form", should always be 0x60

      const size_t lParamTypeCount = parse_uleb128<uint32_t>();
      LLVMTypeRef lParamTypes[lParamTypeCount];
      for (size_t lParamType = 0; lParamType < lParamTypeCount; lParamType++)
        lParamTypes[lParamType] = parse_llvm_vtype();

      const size_t lReturnTypesCount = parse_uleb128<uint32_t>();
      if (lReturnTypesCount > 1)
        throw invalid_exception("only one return type supported");
      LLVMTypeRef lReturnType = lReturnTypesCount ? parse_llvm_vtype() : LLVMVoidType();
      mTypes[lType] = LLVMFunctionType(lReturnType, lParamTypes, uint(lParamTypeCount), false);
    }
    profile_step("  module/types");
  }

  void module::parse_imports(std::initializer_list<std::string_view> pContextImportNs) {
    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lImport = 0; lImport < lCount; lImport++) {
      uint32_t lSize = parse_uleb128<uint32_t>();
      std::string_view lModule = parse_str(lSize);
      const bool lContextImportMod = std::find(pContextImportNs.begin(), pContextImportNs.end(), lModule);
      lSize = parse_uleb128<uint32_t>();
      std::string_view lField = parse_str(lSize);
      external_kind lKind = (external_kind)parse<uint8_t>();
      std::stringstream lName;
      lName << "__wimport_" << lModule << '_' << lField;
      switch (lKind) {
        case ek_function: {
          uint32_t lTypeIndex = parse_uleb128<uint32_t>();
          if (lTypeIndex >= mTypes.size())
            throw invalid_exception("invalid type index");
          LLVMTypeRef lType = mTypes[lTypeIndex];

          if (lContextImportMod) {
            // Add context as parameter
            size_t lParamCount = LLVMCountParamTypes(lType);
            std::vector<LLVMTypeRef> lParams(lParamCount + 1);
            lParams[0] = map_ctype<void*>();
            LLVMGetParamTypes(lType, lParams.data() + 1);
            lType = LLVMFunctionType(LLVMGetReturnType(lType), lParams.data(), lParams.size(), false);
          }

          LLVMValueRef lFunction = LLVMAddFunction(mModule, "wembed.func", lType);
          LLVMSetLinkage(lFunction, LLVMLinkage::LLVMExternalLinkage);
          const uint64_t lTypeHash = hash_fn_type(lType);
          mFunctions.emplace_back(lFunction, lTypeHash, lContextImportMod);
          mImports[std::string(lModule)].emplace(std::string(lField), symbol_t{ek_function, hash_fn_type(lType), lFunction});
  #ifdef WEMBED_VERBOSE
          std::cout << "Import func " << mFunctions.size() << ": " << LLVMPrintTypeToString(lType) << ", hash:" << hash_fn_type(lType) << std::endl;
  #endif
        } break;
        case ek_global: {
          LLVMTypeRef lType = parse_llvm_vtype();
          uint8_t lMutable = parse<uint8_t>();
          LLVMValueRef lGlobal = LLVMAddGlobal(mModule, lType, "wembed.glob");
          LLVMSetLinkage(lGlobal, LLVMLinkage::LLVMExternalLinkage);
          LLVMSetGlobalConstant(lGlobal, !lMutable);
#ifdef WEMBED_VERBOSE
          std::cout << "Import global " << mGlobals.size() << ": " << LLVMPrintTypeToString(lType) << ", hash:" << hash_type(lType) << std::endl;
#endif
          mGlobals.emplace_back(lGlobal);
          mImports[std::string(lModule)].emplace(std::string(lField), symbol_t{ek_global, hash_type(lType, !lMutable), lGlobal});
        } break;
        case ek_table: {
          mTableImport.mModule = lModule;
          mTableImport.mField = lField;
          if (!mTables.empty())
            throw invalid_exception("multiple tables not supported");
          table_type lType = parse_table_type();
          LLVMTypeRef lContainerType = LLVMPointerType(LLVMInt8Type(), 0);
          LLVMValueRef lPointers = LLVMAddGlobal(mModule, lContainerType, "wembed.tablePtrs");
          LLVMValueRef lTypes = LLVMAddGlobal(mModule, LLVMInt64Type(), "wembed.tableTypes");
          mTables.emplace_back(lType, lPointers, lTypes);
          LLVMSetLinkage(lPointers, LLVMLinkage::LLVMExternalLinkage);
          LLVMSetLinkage(lTypes, LLVMLinkage::LLVMExternalLinkage);
          mImports[std::string(lModule)].emplace(std::string(lField), symbol_t{ek_table, WEMBED_HASH_TABLE, {lPointers, lTypes}});
        } break;
        case ek_memory: {
          if (!mMemoryTypes.empty())
            throw invalid_exception("multiple memories");
          mMemoryImport.mModule = lModule;
          mMemoryImport.mField = lField;
          memory_type lMemType;
          lMemType.mLimits = parse_resizable_limits();
          if (lMemType.mLimits.mInitial > 65536 || (lMemType.mLimits.mFlags && lMemType.mLimits.mMaximum > 65536))
            throw invalid_exception("memory size too big, max 65536 pages");
          mMemoryTypes.emplace_back(lMemType);
          mImports[std::string(lModule)].emplace(std::string(lField), symbol_t{ek_memory, WEMBED_HASH_MEMORY, mBaseMemory});
        } break;
        default: throw malformed_exception("unexpected import kind");
      }
    }
    mImportFuncOffset = mFunctions.size();
    profile_step("  module/imports");
  }

  void module::parse_functions() {
    mImportFuncOffset = mFunctions.size();
    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lFunc = 0; lFunc < lCount; lFunc++) {
      uint32_t lTIndex = parse_uleb128<uint32_t>();
      if (lTIndex >= mTypes.size())
        throw invalid_exception("type index out of range");
      LLVMValueRef lFuncRef = LLVMAddFunction(mModule, "wembed.func", mTypes[lTIndex]);
      mFunctions.emplace_back(lFuncRef, hash_fn_type(mTypes[lTIndex]));
    }
    profile_step("  module/functions");
  }

  void module::parse_section_table(uint32_t pSectionSize) {
    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lI = 0; lI < lCount; lI++) {
      if (!mTables.empty())
        throw invalid_exception("multiple tables not supported");
      table_type lType = parse_table_type();
      LLVMTypeRef lContainerType = LLVMPointerType(LLVMInt8Type(), 0);
      LLVMValueRef lPointers = LLVMAddGlobal(mModule, lContainerType, "wembed.tablePtrs");
      LLVMValueRef lTypes = LLVMAddGlobal(mModule, LLVMInt64Type(), "wembed.tableTypes");
      mTables.emplace_back(lType, lPointers, lTypes);
    }
    profile_step("  module/tables");
  }

  void module::parse_section_memory(uint32_t pSectionSize) {
    uint32_t lCount = parse_uleb128<uint32_t>();
    if (lCount > 1)
      throw invalid_exception("only one memory region supported");
    if (!mMemoryTypes.empty() && lCount > 0)
      throw invalid_exception("multiple memories");
    for (size_t lI = 0; lI < lCount; lI++) {
      memory_type lResult;
      lResult.mLimits = parse_resizable_limits();
      if (lResult.mLimits.mInitial > 65536 || (lResult.mLimits.mFlags && lResult.mLimits.mMaximum > 65536))
        throw invalid_exception("memory size too big, max 65536 pages");
      mMemoryTypes.emplace_back(lResult);
    }
    profile_step("  module/memories");
  }

  void module::parse_globals() {
    LLVMValueRef lInit = LLVMAddFunction(mModule, "wembed.start.globals", LLVMFunctionType(LLVMVoidType(), nullptr, 0, false));
    LLVMBasicBlockRef lBlock = LLVMAppendBasicBlock(lInit, "entry");
    LLVMPositionBuilderAtEnd(mBuilder, lBlock);

    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lI = 0; lI < lCount; lI++) {
      LLVMTypeRef lType = parse_llvm_vtype();
      uint8_t lMutable = parse<uint8_t>();
      auto lInitValue = parse_llvm_init(lType);
      auto lValue = lInitValue();
  #ifdef WEMBED_VERBOSE
      std::cout << "Global " << mGlobals.size() << " " << LLVMPrintTypeToString(lType)
                << ": " << LLVMPrintValueToString(lValue) << std::endl;
  #endif
      LLVMValueRef lGlobal = LLVMAddGlobal(mModule, lType, "wembed.global");
      LLVMSetGlobalConstant(lGlobal, !lMutable);
      if (LLVMIsConstant(lValue)) {
        LLVMSetInitializer(lGlobal, lValue);
      }
      else {
        LLVMSetInitializer(lGlobal, get_zero(lType));
        LLVMBuildStore(mBuilder, lValue, lGlobal);
        LLVMSetGlobalConstant(lGlobal, false);
      }
      mGlobals.emplace_back(lGlobal);
    }

    LLVMBuildRetVoid(mBuilder);
    LLVMPositionBuilderAtEnd(mBuilder, mStartInit);
    LLVMBuildCall(mBuilder, lInit, nullptr, 0, "");
    profile_step("  module/globals");
  }

  void module::parse_exports() {
    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lI = 0; lI < lCount; lI++) {
      uint32_t lSize = parse_uleb128<uint32_t>();
      std::string_view lName = parse_str(lSize);
      if (mExports.find(std::string(lName)) != mExports.end())
        throw invalid_exception("duplicate export name");
      external_kind lKind = parse_external_kind();
      uint32_t lIndex = parse_uleb128<uint32_t>();
      switch (lKind) {
        case ek_function: {
          if (lIndex >= mFunctions.size())
            throw invalid_exception("function index out of bounds");
          auto lFuncDef = mFunctions[lIndex];
          mExports.emplace(std::string(lName), symbol_t{lKind, lFuncDef.mType, {mFunctions[lIndex].mValue}});
          //LLVMSetLinkage(mFunctions[lIndex], LLVMInternalLinkage);
#ifdef WEMBED_VERBOSE
          std::cout << "Export func " << lIndex << ": " << LLVMPrintTypeToString(LLVMTypeOf(lFuncDef.mValue)) << ", hash:" << lFuncDef.mType << std::endl;
#endif
        } break;
        case ek_global: {
          if (lIndex >= mGlobals.size())
            throw invalid_exception("global index out of bounds");
          LLVMTypeRef lType = LLVMGetElementType(LLVMTypeOf(mGlobals[lIndex]));
          mExports.emplace(lName, symbol_t{lKind, hash_type(lType, LLVMIsGlobalConstant(mGlobals[lIndex]) != 0), {mGlobals[lIndex]}});
          //LLVMSetLinkage(mGlobals[lIndex], LLVMInternalLinkage);
#ifdef WEMBED_VERBOSE
          std::cout << "Export global " << lIndex << ": " << LLVMPrintTypeToString(lType) << ", hash:" << hash_type(lType) << std::endl;
#endif
        } break;
        case ek_table: {
          if (lIndex >= mTables.size())
            throw invalid_exception("table index out of bounds");
          mExports.emplace(lName, symbol_t{lKind, WEMBED_HASH_TABLE, {mTables[0].mPointers, mTables[0].mTypes}});
          //LLVMSetLinkage(mTables[0].mPointers, LLVMInternalLinkage);
          //LLVMSetLinkage(mTables[0].mTypes, LLVMInternalLinkage);
        } break;
        case ek_memory: {
          if (lIndex >= mMemoryTypes.size())
            throw invalid_exception("memory index out of bounds");
          mExports.emplace(lName, symbol_t{lKind, WEMBED_HASH_MEMORY, {mBaseMemory}});
          //(mBaseMemory, LLVMInternalLinkage);
        } break;
        default:
          throw std::runtime_error("unsupported export type");
          break;
      }
    }
    profile_step("  module/exports");
  }

  void module::parse_section_start(uint32_t pSectionSize) {
    LLVMPositionBuilderAtEnd(mBuilder, mStartUser);
    uint32_t lIndex = parse_uleb128<uint32_t>();
    if (lIndex >= mFunctions.size())
      throw invalid_exception("function index out of bounds");
    LLVMTypeRef lStartSignature = LLVMGetElementType(LLVMTypeOf(mFunctions[lIndex].mValue));
    if (LLVMGetReturnType(lStartSignature) != LLVMVoidType())
      throw invalid_exception("start function required to return void");
    if (LLVMCountParamTypes(lStartSignature) > 0)
      throw invalid_exception("start function require no input param");

    LLVMBuildCall(mBuilder, mFunctions[lIndex].mValue, nullptr, 0, "");
    profile_step("  module/start");
  }

  void module::parse_section_element(uint32_t pSectionSize) {
    LLVMValueRef lInit = LLVMAddFunction(mModule, "wembed.start.element", LLVMFunctionType(LLVMVoidType(), nullptr, 0, false));
    LLVMBasicBlockRef lChecks = LLVMAppendBasicBlock(lInit, "checks");
    LLVMBasicBlockRef lCopies = LLVMAppendBasicBlock(lInit, "copies");

    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lI = 0; lI < lCount; lI++) {
      uint32_t lIndex = parse_uleb128<uint32_t>();
      if (lIndex >= mTables.size())
        throw invalid_exception("table index out of bounds");
      const Table &lTable = mTables[lIndex];

      LLVMPositionBuilderAtEnd(mBuilder, lChecks);
      auto lOffsetInit = parse_llvm_init(LLVMInt32Type());
      {
        LLVMValueRef lOffset = lOffsetInit();
        LLVMValueRef lTabSize = LLVMBuildCall(mBuilder, mTableSize, &mContextRef, 1, "curTabSize");
        LLVMValueRef lOffsetOverflow = LLVMBuildICmp(mBuilder, LLVMIntUGT, lOffset, lTabSize, "offsetOverflow");
        static const char *lErrorString = "elements segment does not fit";
        lChecks = trap_if(lInit, lOffsetOverflow, mThrowUnlinkable, {get_string(lErrorString)});
      }

      if (lTable.mType.mType == e_anyfunc) {
        uint32_t lElemCount = parse_uleb128<uint32_t>();
        for (uint32_t lElemIndex = 0; lElemIndex < lElemCount; lElemIndex++) {
          uint32_t lFuncIndex = parse_uleb128<uint32_t>();
          if (lFuncIndex >= mFunctions.size())
            throw invalid_exception("function index out of bounds");

          LLVMPositionBuilderAtEnd(mBuilder, lChecks);
          {
            LLVMValueRef lOffset = lOffsetInit();
            LLVMValueRef lTotalOffset = LLVMBuildAdd(mBuilder, lOffset, get_const(lElemIndex), "totalOffset");
            lChecks = trap_elem_copy(lInit, lTotalOffset);
          }

          LLVMPositionBuilderAtEnd(mBuilder, lCopies);
          {
            LLVMValueRef lOffset = lOffsetInit();
            LLVMValueRef lTotalOffset = LLVMBuildAdd(mBuilder, lOffset, get_const(lElemIndex), "totalOffset");
            LLVMValueRef lTablePtr = LLVMBuildPointerCast(mBuilder, lTable.mPointers,
                                                          LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0),
                                                          "table");
            LLVMValueRef lPointer = LLVMBuildInBoundsGEP(mBuilder, lTablePtr, &lTotalOffset, 1, "gep");
            // Stores the indices, it's the context that will replace them to func ptr
            LLVMBuildStore(mBuilder, LLVMBuildIntToPtr(mBuilder, get_const(lFuncIndex + 1),
                                                       LLVMPointerType(LLVMInt8Type(), 0), "cast"), lPointer);
          }
        }
      }
    }

    LLVMPositionBuilderAtEnd(mBuilder, lChecks);
    LLVMBuildBr(mBuilder, lCopies);

    LLVMPositionBuilderAtEnd(mBuilder, lCopies);
    LLVMBuildRetVoid(mBuilder);
    LLVMPositionBuilderAtEnd(mBuilder, mStartInit);
    LLVMBuildCall(mBuilder, lInit, nullptr, 0, "");
    profile_step("  module/elements");
  }

  void module::parse_section_data(uint32_t pSectionSize) {
    if (mMemoryTypes.empty())
      throw invalid_exception("data provided without memory section");

    LLVMValueRef lInit = LLVMAddFunction(mModule, "wembed.start.data", LLVMFunctionType(LLVMVoidType(), nullptr, 0, false));
    LLVMBasicBlockRef lChecks = LLVMAppendBasicBlock(lInit, "checks");
    LLVMBasicBlockRef lCopies = LLVMAppendBasicBlock(lInit, "copies");
    LLVMPositionBuilderAtEnd(mBuilder, lChecks);

    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lI = 0; lI < lCount; lI++) {
      uint32_t lIndex = parse_uleb128<uint32_t>();
      if (lIndex != 0)
        throw invalid_exception("multiple memory block not supported");
      auto lOffsetInit = parse_llvm_init(LLVMInt32Type());
      LLVMValueRef lOffset = lOffsetInit();
      uint32_t lSize = parse_uleb128<uint32_t>();

      LLVMPositionBuilderAtEnd(mBuilder, lChecks);
      lChecks = trap_data_copy(lInit, lOffset, lSize);
      if (lSize > 0) {
        LLVMPositionBuilderAtEnd(mBuilder, lCopies);
        call_intrinsic(mMemCpy, {
            LLVMBuildInBoundsGEP(mBuilder, mBaseMemory, &lOffset, 1, "offseted"),
            LLVMConstIntToPtr(LLVMConstInt(LLVMInt64Type(), ull_t(mCurrent), false),
                              LLVMPointerType(LLVMInt8Type(), 0)),
            LLVMConstInt(LLVMInt32Type(), lSize, false),
            LLVMConstInt(LLVMInt1Type(), 1, false),
        });
        mCurrent += lSize;
#ifdef WEMBED_VERBOSE
        std::cout << "data copy from " << ull_t(mCurrent) << " to " << LLVMPrintValueToString(lOffset) << std::endl;
#endif
      }
    }

    LLVMPositionBuilderAtEnd(mBuilder, lChecks);
    LLVMBuildBr(mBuilder, lCopies);

    LLVMPositionBuilderAtEnd(mBuilder, lCopies);
    LLVMBuildRetVoid(mBuilder);
    LLVMPositionBuilderAtEnd(mBuilder, mStartInit);
    LLVMBuildCall(mBuilder, lInit, nullptr, 0, "");
    profile_step("  module/data");
  }

  void module::skip_unreachable(uint8_t *pPastEnd) {
    auto &up = mCFEntries.back();
    if (up.mOuterStackSize > mEvalStack.size())
      throw invalid_exception("invalid stack state");
    mEvalStack.resize(up.mOuterStackSize);
    up.mReachable = false;
    mUnreachableDepth = 0;
  }

  void module::parse_section_code(uint32_t pSectionSize) {
    uint8_t *lCodeSectionStart = mCurrent;
    uint32_t lCount = parse_uleb128<uint32_t>();
    for (size_t lCode = 0; lCode < lCount; lCode++) {
      size_t lFIndex = mImportFuncOffset + lCode;
      if (lFIndex >= mFunctions.size())
        throw invalid_exception("function index out of bounds");
      LLVMValueRef lFunc = mFunctions[lFIndex].mValue;
      LLVMTypeRef lFuncType = LLVMGetElementType(LLVMTypeOf(lFunc));
      LLVMTypeRef lReturnType = LLVMGetReturnType(lFuncType);

      std::vector<LLVMValueRef> lParams(LLVMCountParams(lFunc));
      LLVMGetParams(lFunc, lParams.data());
      std::vector<LLVMTypeRef> lParamTypes(LLVMCountParamTypes(lFuncType));
      LLVMGetParamTypes(lFuncType, lParamTypes.data());

      LLVMBasicBlockRef lFuncEnd = LLVMAppendBasicBlock(lFunc, "fEnd");
      LLVMValueRef lReturnPhi = create_phi(lReturnType, lFuncEnd);
      pushCFEntry(cf_function, lReturnType, lFuncEnd, lReturnPhi);
      pushBlockEntry(lReturnType, lFuncEnd, lReturnPhi);

      LLVMBasicBlockRef lFuncEntry = LLVMAppendBasicBlock(lFunc, "fEntry");
      LLVMPositionBuilderAtEnd(mBuilder, lFuncEntry);

      std::vector<LLVMValueRef> lLocals;
      std::vector<LLVMTypeRef> lLocalTypes;
      for(size_t lIndexArg = 0; lIndexArg < lParams.size(); lIndexArg++) {
        LLVMValueRef lAlloc = LLVMBuildAlloca(mBuilder, lParamTypes[lIndexArg], "param");
        LLVMBuildStore(mBuilder, lParams[lIndexArg], lAlloc);
        lLocals.emplace_back(lAlloc);
        lLocalTypes.emplace_back(LLVMTypeOf(lAlloc));
      }

      uint32_t lBodySize = parse_uleb128<uint32_t>();
      uint8_t *lBefore = mCurrent;

      uint32_t lFuncOffsetStart = mCurrent - lCodeSectionStart;
      /*if (mDebugSupport) {
        mInstructions[lFuncOffsetStart] = LLVMBasicBlockAsValue(lFuncEntry);
      }*/

#ifdef WEMBED_VERBOSE
      std::cout << "At PC 0x" << std::hex << lFuncOffsetStart << std::dec
                << " parsing code for func " << lFIndex << ": " << LLVMGetValueName(lFunc)
                << ", type " << LLVMPrintTypeToString(lFuncType) << std::endl;
#endif

      uint32_t lGroupCount = parse_uleb128<uint32_t>();
      for (size_t lLocalBlock = 0; lLocalBlock < lGroupCount; lLocalBlock++) {
        uint32_t lLocalsCount = parse_uleb128<uint32_t>();
        LLVMTypeRef lType = parse_llvm_vtype();
        LLVMValueRef lZero = get_zero(lType);
        for (size_t lLocal = 0; lLocal < lLocalsCount; lLocal++) {
          LLVMValueRef lAlloc = LLVMBuildAlloca(mBuilder, lType, "local");
          lLocals.emplace_back(lAlloc);
          lLocalTypes.emplace_back(LLVMTypeOf(lAlloc));
          LLVMBuildStore(mBuilder, lZero, lAlloc);
        }
      }

      size_t lCodeSize = lBodySize - (mCurrent - lBefore);
      uint8_t *lEndPos = mCurrent + lCodeSize;
      while (mCurrent < lEndPos && !mCFEntries.empty()) {
  #if defined(WEMBED_VERBOSE) && 0
        std::cout << "At PC 0x" << std::hex << (mCurrent - lCodeSectionStart) << ", instruction 0x" << (uint32_t)*mCurrent << std::dec
                  << ", reachable: " << mCFEntries.back().mReachable
                  << ", depth: " << mUnreachableDepth << std::endl;
  #endif
        size_t lPC = mCurrent - lCodeSectionStart;
        LLVMValueRef lLastValue = nullptr;
        uint8_t lInstr = *mCurrent++;
        if (!mCFEntries.back().mReachable) {
          switch (lInstr) {
            case o_block:
            case o_loop:
            case o_if:
              mUnreachableDepth++;
              parse_llvm_btype();
              continue;
            case o_else:
              if (!mUnreachableDepth)
                break;
              continue;
            case o_end:
              if (!mUnreachableDepth)
                break;
              else
                mUnreachableDepth--;
              continue;

            case o_memory_grow:
            case o_memory_size:
              mCurrent++;
              continue;

            case o_br:
            case o_const_i32:
            case o_get_local:
            case o_set_local:
            case o_tee_local:
            case o_get_global:
            case o_set_global:
            case o_call:
            case o_br_if:
              parse_uleb128<uint32_t>();
              continue;

            case o_const_i64:
              parse_uleb128<uint64_t>();
              continue;

            case o_const_f32:
              parse<float>();
              continue;

            case o_const_f64:
              parse<double>();
              continue;

            case o_call_indirect:
              parse_uleb128<uint32_t>();
              mCurrent++;
              continue;

            case o_store_f32:
            case o_store_i32:
            case o_load_f32:
            case o_load_i32:
            case o_load8_si32:
            case o_load16_si32:
            case o_load8_ui32:
            case o_load16_ui32:
            case o_store8_i32:
            case o_store16_i32:
              parse_uleb128<uint32_t>();
              parse_uleb128<uint32_t>();
              continue;

            case o_store_f64:
            case o_store_i64:
            case o_load_f64:
            case o_load_i64:
            case o_load8_si64:
            case o_load16_si64:
            case o_load32_si64:
            case o_load8_ui64:
            case o_load16_ui64:
            case o_load32_ui64:
            case o_store8_i64:
            case o_store16_i64:
            case o_store32_i64:
              parse_uleb128<uint32_t>();
              parse_uleb128<uint64_t>();
              continue;

            case o_br_table: {
              uint32_t lTargetCount = parse_uleb128<uint32_t>();
              for (size_t i = 0; i < lTargetCount; i++)
                parse_uleb128<uint32_t>();
              parse_uleb128<uint32_t>();
              continue;
            }

            case o_prefix_numeric:
              mCurrent++;
              continue;

            default:
              continue;
          }
        }
        switch (lInstr) {
          case o_nop: break;
          case o_drop: pop(); break;

          case o_block: {
            LLVMTypeRef lType = parse_llvm_btype();
            auto lBlockEnd = LLVMAppendBasicBlock(lFunc, "bEnd");
            LLVMValueRef lPhi = create_phi(lType, lBlockEnd);
            pushCFEntry(cf_block, lType, lBlockEnd, lPhi);
            pushBlockEntry(lType, lBlockEnd, lPhi);
          } break;

          case o_loop: {
            LLVMTypeRef lType = parse_llvm_btype();
            LLVMBasicBlockRef lLoop = LLVMAppendBasicBlock(lFunc, "lEntry");
            LLVMBasicBlockRef lEnd = LLVMAppendBasicBlock(lFunc, "lEnd");
            LLVMValueRef lPhi = create_phi(lType, lEnd);

            LLVMBuildBr(mBuilder, lLoop);
            LLVMPositionBuilderAtEnd(mBuilder, lLoop);

            pushCFEntry(cf_loop, lType, lEnd, lPhi);
            pushBlockEntry(LLVMVoidType(), lLoop, nullptr);
          } break;

          case o_if: {
            LLVMTypeRef lType = parse_llvm_btype();
            LLVMBasicBlockRef lThen = LLVMAppendBasicBlock(lFunc, "iThen");
            LLVMBasicBlockRef lElse = LLVMAppendBasicBlock(lFunc, "iElse");
            LLVMBasicBlockRef lEnd = LLVMAppendBasicBlock(lFunc, "iEnd");
            LLVMValueRef lPhi = create_phi(lType, lEnd);

            LLVMBuildCondBr(mBuilder, i32_to_bool(pop(LLVMInt32Type())), lThen, lElse);
            LLVMPositionBuilderAtEnd(mBuilder, lThen);

            pushCFEntry(cf_if, lType, lEnd, lPhi, lElse);
            pushBlockEntry(lType, lEnd, lPhi);
          } break;

          case o_else: {
            assert(mCFEntries.size() && "empty control flow");
            CFEntry &lEntry = mCFEntries.back();
            if (lEntry.mReachable) {
              if (lEntry.mSignature != LLVMVoidType()) {
                auto lValue = pop(lEntry.mSignature);
                auto lWhere = LLVMGetInsertBlock(mBuilder);
                LLVMAddIncoming(lEntry.mPhi, &lValue, &lWhere, 1);
              }
              LLVMBuildBr(mBuilder, lEntry.mEnd);
            }
            if (mEvalStack.size() != lEntry.mOuterStackSize)
              throw invalid_exception("wrong stack size after block end");
            assert(lEntry.mElse);
            assert(lEntry.mInstr == cf_if);

            auto lCurrentBlock = LLVMGetInsertBlock(mBuilder);
            LLVMMoveBasicBlockAfter(lEntry.mElse, lCurrentBlock);
            LLVMPositionBuilderAtEnd(mBuilder, lEntry.mElse);

            lEntry.mInstr = cf_else;
            lEntry.mReachable = lEntry.mElseReachable;
            lEntry.mElse = nullptr;
          } break;

          case o_end: {
            assert(mCFEntries.size() && "empty control flow");
            const CFEntry &lEntry = mCFEntries.back();
            if (lEntry.mReachable) {
              if (lEntry.mSignature != LLVMVoidType()) {
                auto lValue = pop(LLVMTypeOf(lEntry.mPhi));
                auto lWhere = LLVMGetInsertBlock(mBuilder);
                LLVMAddIncoming(lEntry.mPhi, &lValue, &lWhere, 1);
              }
              LLVMBuildBr(mBuilder, lEntry.mEnd);
            }
            if (mEvalStack.size() != lEntry.mOuterStackSize)
              throw invalid_exception("wrong stack size after block end");

            if (lEntry.mElse) {
              auto lCurrentBlock = LLVMGetInsertBlock(mBuilder);
              LLVMMoveBasicBlockAfter(lEntry.mElse, lCurrentBlock);
              LLVMPositionBuilderAtEnd(mBuilder, lEntry.mElse);
              LLVMBuildBr(mBuilder, lEntry.mEnd);
            }

            auto lCurrentBlock = LLVMGetInsertBlock(mBuilder);
            LLVMMoveBasicBlockAfter(lEntry.mEnd, lCurrentBlock);
            LLVMPositionBuilderAtEnd(mBuilder, lEntry.mEnd);

            if (lEntry.mPhi) {
              if(LLVMCountIncoming(lEntry.mPhi)) {
                push(lEntry.mPhi);
              }
              else {
                LLVMInstructionEraseFromParent(lEntry.mPhi);
                assert(lEntry.mSignature != LLVMVoidType());
                push(get_zero(lEntry.mSignature));
              }
            }
            assert(lEntry.mOuterBlockDepth <= mBlockEntries.size());
            mBlockEntries.resize(lEntry.mOuterBlockDepth);
            mCFEntries.pop_back();
          } break;

          case o_unreachable: {
            const char *lErrorString = "unreachable reached";
            trap_if(lFunc, get_const(true), mThrowVMException, {get_string(lErrorString)});
            lLastValue = LLVMBuildUnreachable(mBuilder);
            skip_unreachable(lEndPos);
          } break;

          case o_br: {
            uint32_t lDepth = parse_uleb128<uint32_t>();
            const BlockEntry &lTarget = branch_depth(lDepth);
            if (lTarget.mSignature != LLVMVoidType()) {
              LLVMValueRef lResult = pop(lTarget.mSignature);
              auto lWhere = LLVMGetInsertBlock(mBuilder);
              LLVMAddIncoming(lTarget.mPhi, &lResult, &lWhere, 1);
            }
            lLastValue = LLVMBuildBr(mBuilder, lTarget.mBlock);
            skip_unreachable(lEndPos);
          } break;

          case o_br_if: {
            auto lCond = pop(LLVMInt32Type());
            uint32_t lDepth = parse_uleb128<uint32_t>();
            const BlockEntry &lTarget = branch_depth(lDepth);
            if (lTarget.mSignature != LLVMVoidType()) {
              LLVMValueRef lResult = top(lTarget.mSignature);
              auto lWhere = LLVMGetInsertBlock(mBuilder);
              LLVMAddIncoming(lTarget.mPhi, &lResult, &lWhere, 1);
            }
            LLVMBasicBlockRef lElse = LLVMAppendBasicBlock(lFunc, "afterBrIf");
            lLastValue = LLVMBuildCondBr(mBuilder, i32_to_bool(lCond), lTarget.mBlock, lElse);

            LLVMPositionBuilderAtEnd(mBuilder, lElse);
          } break;

          case o_br_table: {
            auto lIndex = pop(LLVMInt32Type());
            uint32_t lTargetCount = parse_uleb128<uint32_t>();
            std::vector<uint32_t> lTargets(lTargetCount);
            for (size_t i = 0; i < lTargetCount; i++) {
              lTargets[i] = parse_uleb128<uint32_t>();
            }
            uint32_t lDefault = parse_uleb128<uint32_t>();

            const BlockEntry &lDefaultTarget = branch_depth(lDefault);
            LLVMValueRef lResult = nullptr;
            if (lDefaultTarget.mSignature != LLVMVoidType()) {
              lResult = pop(lDefaultTarget.mSignature);
              auto lWhere = LLVMGetInsertBlock(mBuilder);
              LLVMAddIncoming(lDefaultTarget.mPhi, &lResult, &lWhere, 1);
            }

            LLVMValueRef lLastValue = LLVMBuildSwitch(mBuilder, lIndex, lDefaultTarget.mBlock, lTargets.size());
            for (size_t i = 0; i < lTargets.size(); i++) {
              const BlockEntry &lTarget = branch_depth(lTargets[i]);
              if (lTarget.mSignature != lDefaultTarget.mSignature)
                throw invalid_exception("br_table type mismatch");
              LLVMAddCase(lLastValue, get_const((uint32_t)i), lTarget.mBlock);
              if (lResult != nullptr) {
                auto lWhere = LLVMGetInsertBlock(mBuilder);
                LLVMAddIncoming(lTarget.mPhi, &lResult, &lWhere, 1);
              }
            }

            skip_unreachable(lEndPos);
          } break;

          case o_return: {
            if (lReturnType != LLVMVoidType()) {
              auto lValue = pop(lReturnType);
              auto lWhere = LLVMGetInsertBlock(mBuilder);
              LLVMAddIncoming(mCFEntries[0].mPhi, &lValue, &lWhere, 1);
            }
            lLastValue = LLVMBuildBr(mBuilder, mCFEntries[0].mEnd);
            skip_unreachable(lEndPos);
          } break;

          case o_call: {
            uint32_t lCalleeIndex = parse_uleb128<uint32_t>();
            if (lCalleeIndex >= mFunctions.size())
              throw invalid_exception("function index out of bounds");
            auto lCalleeDef = mFunctions[lCalleeIndex];
            LLVMValueRef lCallee = lCalleeDef.mValue;
            LLVMTypeRef lCalleeType = LLVMGetElementType(LLVMTypeOf(lCallee));
            size_t lCalleeParamCount = LLVMCountParams(lCallee);
            size_t lArgsOffset = 0;
            std::vector<LLVMValueRef> lCalleeParams(lCalleeParamCount);

            if (lCalleeDef.mWithContext) {
              lCalleeParamCount--;
              lArgsOffset = 1;
              lCalleeParams[0] = LLVMBuildPointerCast(mBuilder, mContextRef, map_ctype<void*>(), "cast ctx to void*");
            }
            if (lCalleeParamCount > mEvalStack.size())
              throw invalid_exception("not enough args in stack");
            std::copy(mEvalStack.end() - lCalleeParamCount, mEvalStack.end(), lCalleeParams.begin() + lArgsOffset);
            mEvalStack.resize(mEvalStack.size() - lCalleeParamCount);

            std::vector<LLVMTypeRef> lArgTypes(LLVMCountParamTypes(lCalleeType));
            LLVMGetParamTypes(lCalleeType, lArgTypes.data());
            for (size_t i = 0; i < lCalleeParamCount; i++) {
              if (LLVMTypeOf(lCalleeParams[i]) != lArgTypes[i])
                throw invalid_exception("arg type mismatch");
            }

            bool lCalleeReturnValue = LLVMGetReturnType(lCalleeType) != LLVMVoidType();
            lLastValue = LLVMBuildCall(mBuilder, lCallee, lCalleeParams.data(), lCalleeParams.size(),
                                         lCalleeReturnValue ? "call" : "");
            if(lCalleeReturnValue) {
              push(lLastValue);
            }
          } break;
          case o_call_indirect: {
            if (mTables.empty())
              throw invalid_exception("can't use call indirect without table");
            uint32_t lSignature = parse_uleb128<uint32_t>();
            /*uint8_t lReserved =*/ parse_uleb128<uint8_t>();
            LLVMValueRef lCalleeIndice = pop(LLVMInt32Type());
            if (lSignature >= mTypes.size())
              throw invalid_exception("invalid type index");
            LLVMTypeRef lCalleeType = mTypes[lSignature];
            LLVMTypeRef lCalleePtr = LLVMPointerType(lCalleeType, 0);
            size_t lCalleeParamCount = LLVMCountParamTypes(lCalleeType);
            if (lCalleeParamCount > mEvalStack.size())
              throw invalid_exception("not enough args in stack");
            std::vector<LLVMValueRef> lCalleeParams(lCalleeParamCount);
            std::copy(mEvalStack.end() - lCalleeParamCount, mEvalStack.end(), lCalleeParams.begin());
            mEvalStack.resize(mEvalStack.size() - lCalleeParamCount);
            std::vector<LLVMTypeRef> lArgTypes(lCalleeParamCount);
            LLVMGetParamTypes(lCalleeType, lArgTypes.data());
            for (size_t i = 0; i < lCalleeParamCount; i++) {
              if (LLVMTypeOf(lCalleeParams[i]) != lArgTypes[i])
                throw invalid_exception("arg type mismatch");
            }
            bool lCalleeReturnValue = LLVMGetReturnType(lCalleeType) != LLVMVoidType();
            const Table &lTable = mTables[0];
            uint64_t lTypeHash = hash_fn_type(lCalleeType);
            LLVMValueRef lTableTypes = LLVMBuildPointerCast(mBuilder, lTable.mTypes,
                                                            LLVMPointerType(LLVMInt64Type(), 0),
                                                            "tableType");
            LLVMValueRef lTableTypePtr = LLVMBuildInBoundsGEP(mBuilder, lTableTypes, &lCalleeIndice, 1, "gep");
            LLVMValueRef lType = LLVMBuildLoad(mBuilder, lTableTypePtr, "loadPtr");
            // FIXME JBL: It would be uninitialized if ptr was, this is a type mismatch error
            LLVMValueRef lTestType = LLVMBuildICmp(mBuilder, LLVMIntNE, get_const(lTypeHash), lType, "testType");
            static const char *lErrorString = "call to uninitialized table element";
            trap_if(lFunc, lTestType, mThrowVMException, {get_string(lErrorString)});

            LLVMValueRef lTablePtr = LLVMBuildPointerCast(mBuilder, lTable.mPointers,
                                                          LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0),
                                                          "tablePtr");
            LLVMValueRef lPointer = LLVMBuildInBoundsGEP(mBuilder, lTablePtr, &lCalleeIndice, 1, "gep");
            LLVMValueRef lRawFuncPtr = LLVMBuildLoad(mBuilder, lPointer, "loadPtr");
            LLVMValueRef lFuncPtr = LLVMBuildPointerCast(mBuilder, lRawFuncPtr, lCalleePtr, "cast");
            lLastValue = LLVMBuildCall(mBuilder, lFuncPtr, lCalleeParams.data(), lCalleeParams.size(),
                                         lCalleeReturnValue ? "call" : "");
            if(lCalleeReturnValue) {
              push(lLastValue);
            }
          } break;

          case o_select: {
            LLVMValueRef lCondition = pop(LLVMInt32Type());
            LLVMValueRef lFalse = pop();
            LLVMValueRef lTrue = pop(LLVMTypeOf(lFalse));
            lLastValue = push(LLVMBuildSelect(mBuilder, i32_to_bool(lCondition), lTrue, lFalse, "select"));
          } break;
          case o_eqz_i32: {
            LLVMValueRef lZero32 = LLVMConstInt(LLVMInt32Type(), 0, true);
            LLVMValueRef lCond = LLVMBuildICmp(mBuilder, LLVMIntEQ, pop(LLVMInt32Type()), lZero32, "eq");
            lLastValue = push(bool_to_i32(lCond));
          } break;
          case o_eqz_i64: {
            LLVMValueRef lZero64 = LLVMConstInt(LLVMInt64Type(), 0, true);
            LLVMValueRef lCond = LLVMBuildICmp(mBuilder, LLVMIntEQ, pop(LLVMInt64Type()), lZero64, "eq");
            lLastValue = push(bool_to_i32(lCond));
          } break;

          case o_get_local: {
            uint32_t lLocalIndex = parse_uleb128<uint32_t>();
            if (lLocalIndex >= lLocals.size())
              throw invalid_exception("local index out of bounds");
            lLastValue = push(LLVMBuildLoad(mBuilder, lLocals[lLocalIndex], "getLocal"));
          } break;
          case o_set_local: {
            uint32_t lLocalIndex = parse_uleb128<uint32_t>();
            if (lLocalIndex >= lLocals.size())
              throw invalid_exception("local index out of bounds");
            auto lValueType = LLVMGetElementType(lLocalTypes[lLocalIndex]);
            auto lValue = LLVMBuildIntToPtr(mBuilder, pop(lValueType), lValueType, "setLocal");
            lLastValue = LLVMBuildStore(mBuilder, lValue, lLocals[lLocalIndex]);
          } break;
          case o_tee_local: {
            uint32_t lLocalIndex = parse_uleb128<uint32_t>();
            if (lLocalIndex >= lLocals.size())
              throw invalid_exception("local index out of bounds");
            auto lValueType = LLVMGetElementType(lLocalTypes[lLocalIndex]);
            auto lValue = LLVMBuildIntToPtr(mBuilder, top(lValueType), lValueType, "teeLocal");
            lLastValue = LLVMBuildStore(mBuilder, lValue, lLocals[lLocalIndex]);
          } break;

          case o_get_global: {
            uint32_t lGlobalIndex = parse_uleb128<uint32_t>();
            if (lGlobalIndex >= mGlobals.size())
              throw invalid_exception("global index out of bounds");
            lLastValue = push(LLVMBuildLoad(mBuilder, mGlobals[lGlobalIndex], "getGlobal"));
          } break;
          case o_set_global: {
            uint32_t lGlobalIndex = parse_uleb128<uint32_t>();
            if (lGlobalIndex >= mGlobals.size())
              throw invalid_exception("global index out of bounds");
            auto lValueType = LLVMGetElementType(LLVMTypeOf(mGlobals[lGlobalIndex]));
            auto lValue = LLVMBuildIntToPtr(mBuilder, pop(lValueType), lValueType, "setGlobal");
            auto lGlobal = mGlobals[lGlobalIndex];
            if (LLVMIsGlobalConstant(lGlobal))
              throw invalid_exception("global is immutable");
            lLastValue = LLVMBuildStore(mBuilder, lValue, lGlobal);
          } break;

          case o_memory_grow: {
            if (mMemoryTypes.empty()) throw invalid_exception("grow_memory without memory block");
            /*uint8_t lReserved =*/ parse_uleb128<uint8_t>();
            LLVMValueRef lArgs[] = { mContextRef, pop_int() };
            lLastValue = push(LLVMBuildCall(mBuilder, mMemoryGrow, lArgs, 2, "growMem"));
          } break;
          case o_memory_size: {
            if (mMemoryTypes.empty()) throw invalid_exception("current_memory without memory block");
            /*uint8_t lReserved =*/ parse_uleb128<uint8_t>();
            lLastValue = push(LLVMBuildCall(mBuilder, mMemorySize, &mContextRef, 1, "curMem"));
          } break;

          // NOTE JB: Don't affect lLastValue for const, LLVM doens't support attaching source location to them

  #define WEMBED_CONST(OPCODE, PARSEOP) case OPCODE: { \
            push(PARSEOP); \
          } break;

          WEMBED_CONST(o_const_i32, LLVMConstInt(LLVMInt32Type(), static_cast<ull_t>(parse_sleb128<int32_t>()), true))
          WEMBED_CONST(o_const_i64, LLVMConstInt(LLVMInt64Type(), static_cast<ull_t>(parse_sleb128<int64_t>()), true))
          case o_const_f32: {
            fp_bits<float> components(parse<float>());
            push(LLVMConstBitCast(LLVMConstInt(LLVMInt32Type(), static_cast<ull_t>(components.mRaw), false),
                                    LLVMFloatType()));
          } break;
          WEMBED_CONST(o_const_f64, LLVMConstReal(LLVMDoubleType(), parse<double>()))

  #define WEMBED_TRUNC(OPCODE, OPCONV, ITYPE, OTYPE, ICTYPE, FBITSTYPE, OMIN, OMAX) case OPCODE: { \
            LLVMValueRef lValue = pop(ITYPE); \
            LLVMValueRef lBitcast = LLVMBuildBitCast(mBuilder, lValue, FBITSTYPE, "bitcast"); \
            LLVMValueRef lExponent = LLVMBuildAnd(mBuilder, lBitcast, get_const(fp_bits<ICTYPE>::sExponentMask), "exponent"); \
            LLVMValueRef lExponentTest = LLVMBuildICmp(mBuilder, LLVMIntEQ, lExponent, get_const(fp_bits<ICTYPE>::sExponentMask), "exponentTest"); \
            LLVMValueRef lMinBoundsTest = LLVMBuildFCmp(mBuilder, LLVMRealOLE, lValue, get_const(OMIN), "minBounds"); \
            LLVMValueRef lMaxBoundsTest = LLVMBuildFCmp(mBuilder, LLVMRealOGE, lValue, get_const(OMAX), "maxBounds"); \
            LLVMValueRef lBoundsTest = LLVMBuildOr(mBuilder, lMinBoundsTest, lMaxBoundsTest, "boundsTest"); \
            LLVMValueRef lInvalid = LLVMBuildOr(mBuilder, lBoundsTest, lExponentTest, "validTruncate"); \
            const char *lErrorString = "invalid truncate"; \
            trap_if(lFunc, lInvalid, mThrowVMException, {get_string(lErrorString)}); \
            lLastValue = push(OPCONV(mBuilder, lValue, OTYPE, #OPCODE)); \
          } break;

          WEMBED_TRUNC(o_trunc_f32_si32, LLVMBuildFPToSI, LLVMFloatType(), LLVMInt32Type(), float, LLVMInt32Type(), -2147483904.0f, 2147483648.0f);
          WEMBED_TRUNC(o_trunc_f64_si32, LLVMBuildFPToSI, LLVMDoubleType(), LLVMInt32Type(), double, LLVMInt64Type(), -2147483649.0, 2147483648.0);
          WEMBED_TRUNC(o_trunc_f32_ui32, LLVMBuildFPToUI, LLVMFloatType(), LLVMInt32Type(), float, LLVMInt32Type(), -1.0f, 4294967296.0f);
          WEMBED_TRUNC(o_trunc_f64_ui32, LLVMBuildFPToUI, LLVMDoubleType(), LLVMInt32Type(), double, LLVMInt64Type(), -1.0, 4294967296.0);
          WEMBED_TRUNC(o_trunc_f32_si64, LLVMBuildFPToSI, LLVMFloatType(), LLVMInt64Type(), float, LLVMInt32Type(), -9223373136366403584.0f, 9223372036854775808.0f);
          WEMBED_TRUNC(o_trunc_f64_si64, LLVMBuildFPToSI, LLVMDoubleType(), LLVMInt64Type(), double, LLVMInt64Type(), -9223372036854777856.0, 9223372036854775808.0);
          WEMBED_TRUNC(o_trunc_f32_ui64, LLVMBuildFPToUI, LLVMFloatType(), LLVMInt64Type(), float, LLVMInt32Type(), -1.0f, 18446744073709551616.0f);
          WEMBED_TRUNC(o_trunc_f64_ui64, LLVMBuildFPToUI, LLVMDoubleType(), LLVMInt64Type(), double, LLVMInt64Type(), -1.0, 18446744073709551616.0);

  #define WEMBED_LOAD(OPCODE, TYPE, BYTES, CONVOP) case OPCODE: { \
            if (mMemoryTypes.empty()) throw invalid_exception("load without memory block"); \
            LLVMTypeRef lPtrType = LLVMPointerType(TYPE, 0); \
            uint32_t lFlags = parse_uleb128<uint32_t>(); \
            LLVMValueRef lOffset = LLVMConstInt(LLVMInt32Type(), static_cast<ull_t>(parse_uleb128<uint32_t>()), false); \
            auto lTotalOffsetResult = call_mv_intrinsic<2>(mUAddWithOverflow_i32, {pop_int(), lOffset}); \
            static const char *lErrorString = "load addr overflow"; \
            trap_if(lFunc, lTotalOffsetResult[1], mThrowVMException, {get_string(lErrorString)}); \
            LLVMValueRef lTotalOffset = lTotalOffsetResult[0]; \
            LLVMValueRef lOffseted = LLVMBuildInBoundsGEP(mBuilder, mBaseMemory, &lTotalOffset, 1, "offseted"); \
            LLVMValueRef lCasted = LLVMBuildPointerCast(mBuilder, lOffseted, lPtrType, "casted"); \
            LLVMValueRef lLoad = LLVMBuildLoad(mBuilder, lCasted, "load"); \
            uint lAlign = 1<<lFlags; \
            if (lAlign > BYTES) throw invalid_exception("unnatural alignment"); \
            LLVMSetAlignment(lLoad, lAlign); \
            LLVMSetVolatile(lLoad, true); \
            lLastValue = push(CONVOP); \
          } break;

          WEMBED_LOAD(o_load_i32, LLVMInt32Type(), 4, lLoad)
          WEMBED_LOAD(o_load_i64, LLVMInt64Type(), 8, lLoad)
          WEMBED_LOAD(o_load_f32, LLVMFloatType(), 4, lLoad)
          WEMBED_LOAD(o_load_f64, LLVMDoubleType(), 8, lLoad)

          WEMBED_LOAD(o_load8_si32, LLVMInt8Type(), 1, LLVMBuildSExt(mBuilder, lLoad, LLVMInt32Type(), "sext"))
          WEMBED_LOAD(o_load16_si32, LLVMInt16Type(), 2, LLVMBuildSExt(mBuilder, lLoad, LLVMInt32Type(), "sext"))
          WEMBED_LOAD(o_load8_si64, LLVMInt8Type(), 1, LLVMBuildSExt(mBuilder, lLoad, LLVMInt64Type(), "sext"))
          WEMBED_LOAD(o_load16_si64, LLVMInt16Type(), 2, LLVMBuildSExt(mBuilder, lLoad, LLVMInt64Type(), "sext"))
          WEMBED_LOAD(o_load32_si64, LLVMInt32Type(), 4, LLVMBuildSExt(mBuilder, lLoad, LLVMInt64Type(), "sext"))

          WEMBED_LOAD(o_load8_ui32, LLVMInt8Type(), 1, LLVMBuildZExt(mBuilder, lLoad, LLVMInt32Type(), "zext"))
          WEMBED_LOAD(o_load16_ui32, LLVMInt16Type(), 2, LLVMBuildZExt(mBuilder, lLoad, LLVMInt32Type(), "zext"))
          WEMBED_LOAD(o_load8_ui64, LLVMInt8Type(), 1, LLVMBuildZExt(mBuilder, lLoad, LLVMInt64Type(), "zext"))
          WEMBED_LOAD(o_load16_ui64, LLVMInt16Type(), 2, LLVMBuildZExt(mBuilder, lLoad, LLVMInt64Type(), "zext"))
          WEMBED_LOAD(o_load32_ui64, LLVMInt32Type(), 4, LLVMBuildZExt(mBuilder, lLoad, LLVMInt64Type(), "zext"))

  #define WEMBED_STORE(OPCODE, ITYPE, BYTES, OTYPE, CONVOP) case OPCODE: { \
            if (mMemoryTypes.empty()) throw invalid_exception("store without memory block"); \
            LLVMTypeRef lPtrType = LLVMPointerType(OTYPE, 0); \
            uint32_t lFlags = parse_uleb128<uint32_t>(); \
            LLVMValueRef lOffset = LLVMConstInt(LLVMInt32Type(), static_cast<ull_t>(parse_uleb128<uint32_t>()), false); \
            LLVMValueRef lValue = pop(ITYPE); \
            auto lTotalOffsetResult = call_mv_intrinsic<2>(mUAddWithOverflow_i32, {pop_int(), lOffset}); \
            static const char *lErrorString = "store addr overflow"; \
            trap_if(lFunc, lTotalOffsetResult[1], mThrowVMException, {get_string(lErrorString)}); \
            LLVMValueRef lTotalOffset = lTotalOffsetResult[0]; \
            LLVMValueRef lOffseted = LLVMBuildInBoundsGEP(mBuilder, mBaseMemory, &lTotalOffset, 1, "offseted"); \
            LLVMValueRef lCasted = LLVMBuildPointerCast(mBuilder, lOffseted, lPtrType, "casted"); \
            LLVMValueRef lStore = LLVMBuildStore(mBuilder, CONVOP, lCasted); \
            uint lAlign = 1<<lFlags; \
            if (lAlign > BYTES) throw invalid_exception("unnatural alignment"); \
            LLVMSetAlignment(lStore, lAlign); \
            LLVMSetVolatile(lStore, true); \
            lLastValue = lStore; \
        } break;

          WEMBED_STORE(o_store_i32, LLVMInt32Type(), 4, LLVMInt32Type(), lValue)
          WEMBED_STORE(o_store_i64, LLVMInt64Type(), 8, LLVMInt64Type(), lValue)
          WEMBED_STORE(o_store_f32, LLVMFloatType(), 4, LLVMFloatType(), lValue)
          WEMBED_STORE(o_store_f64, LLVMDoubleType(), 8, LLVMDoubleType(), lValue)

          WEMBED_STORE(o_store8_i32, LLVMInt32Type(), 1, LLVMInt8Type(),
                       LLVMBuildTrunc(mBuilder, lValue, LLVMInt8Type(), "trunc"))
          WEMBED_STORE(o_store16_i32, LLVMInt32Type(), 2, LLVMInt16Type(),
                       LLVMBuildTrunc(mBuilder, lValue, LLVMInt16Type(), "trunc"))
          WEMBED_STORE(o_store8_i64, LLVMInt64Type(), 1, LLVMInt8Type(),
                       LLVMBuildTrunc(mBuilder, lValue, LLVMInt8Type(), "trunc"))
          WEMBED_STORE(o_store16_i64, LLVMInt64Type(), 2, LLVMInt16Type(),
                       LLVMBuildTrunc(mBuilder, lValue, LLVMInt16Type(), "trunc"))
          WEMBED_STORE(o_store32_i64, LLVMInt64Type(), 4, LLVMInt32Type(),
                       LLVMBuildTrunc(mBuilder, lValue, LLVMInt32Type(), "trunc"))

  #define WEMBED_ICMP(OPCODE, OPCOMP) case OPCODE##i32: { \
            LLVMValueRef rhs = pop(); \
            LLVMValueRef lhs = pop(LLVMTypeOf(rhs)); \
            lLastValue = push(bool_to_i32(LLVMBuildICmp(mBuilder, OPCOMP, lhs, rhs, #OPCOMP))); \
          } break; \
          case OPCODE##i64: { \
            LLVMValueRef rhs = pop(); \
            LLVMValueRef lhs = pop(LLVMTypeOf(rhs)); \
            lLastValue = push(bool_to_i32(LLVMBuildICmp(mBuilder, OPCOMP, lhs, rhs, #OPCOMP))); \
          } break;

          WEMBED_ICMP(o_eq_, LLVMIntEQ)
          WEMBED_ICMP(o_ne_, LLVMIntNE)
          WEMBED_ICMP(o_lt_s, LLVMIntSLT)
          WEMBED_ICMP(o_lt_u, LLVMIntULT)
          WEMBED_ICMP(o_le_s, LLVMIntSLE)
          WEMBED_ICMP(o_le_u, LLVMIntULE)
          WEMBED_ICMP(o_gt_s, LLVMIntSGT)
          WEMBED_ICMP(o_gt_u, LLVMIntUGT)
          WEMBED_ICMP(o_ge_s, LLVMIntSGE)
          WEMBED_ICMP(o_ge_u, LLVMIntUGE)

  #define WEMBED_FCMP(OPCODE, OPCOMP) case OPCODE##f32: { \
            LLVMValueRef rhs = pop(); \
            LLVMValueRef lhs = pop(LLVMTypeOf(rhs)); \
            lLastValue = push(bool_to_i32(LLVMBuildFCmp(mBuilder, OPCOMP, lhs, rhs, #OPCOMP))); \
          } break; \
          case OPCODE##f64: { \
            LLVMValueRef rhs = pop(); \
            LLVMValueRef lhs = pop(LLVMTypeOf(rhs)); \
            lLastValue = push(bool_to_i32(LLVMBuildFCmp(mBuilder, OPCOMP, lhs, rhs, #OPCOMP))); \
          } break;

          WEMBED_FCMP(o_eq_, LLVMRealOEQ)
          WEMBED_FCMP(o_ne_, LLVMRealUNE)
          WEMBED_FCMP(o_lt_, LLVMRealOLT)
          WEMBED_FCMP(o_le_, LLVMRealOLE)
          WEMBED_FCMP(o_gt_, LLVMRealOGT)
          WEMBED_FCMP(o_ge_, LLVMRealOGE)

  #define WEMBED_BINARY(OPCODE, INSTR) case OPCODE: { \
            LLVMValueRef rhs = pop(); \
            LLVMValueRef lhs = pop(LLVMTypeOf(rhs)); \
            lLastValue = push(INSTR); \
          } break;
  #define WEMBED_BINARY_MULTI(OPCODE, INSTR) WEMBED_BINARY(OPCODE##32, INSTR) WEMBED_BINARY(OPCODE##64, INSTR)

  #define WEMBED_BINARY_LLVM(OPCODE, INSTR) WEMBED_BINARY_MULTI(OPCODE, INSTR(mBuilder, lhs, rhs, #INSTR))

          WEMBED_BINARY_LLVM(o_add_i, LLVMBuildAdd)
          WEMBED_BINARY_LLVM(o_sub_i, LLVMBuildSub)
          WEMBED_BINARY_LLVM(o_mul_i, LLVMBuildMul)

          WEMBED_BINARY(o_div_ui32, emit_udiv(LLVMInt32Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_div_ui64, emit_udiv(LLVMInt64Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_div_si32, emit_sdiv(LLVMInt32Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_div_si64, emit_sdiv(LLVMInt64Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_rem_ui32, emit_urem(LLVMInt32Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_rem_ui64, emit_urem(LLVMInt64Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_rem_si32, emit_srem(LLVMInt32Type(), lFunc, lhs, rhs))
          WEMBED_BINARY(o_rem_si64, emit_srem(LLVMInt64Type(), lFunc, lhs, rhs))

          WEMBED_BINARY_MULTI(o_add_f, clear_nan(LLVMBuildFAdd(mBuilder, lhs, rhs, "fadd")))
          WEMBED_BINARY_MULTI(o_sub_f, clear_nan(LLVMBuildFSub(mBuilder, lhs, rhs, "fsub")))
          WEMBED_BINARY_MULTI(o_mul_f, clear_nan(LLVMBuildFMul(mBuilder, lhs, rhs, "fadd")))
          WEMBED_BINARY_MULTI(o_div_f, clear_nan(LLVMBuildFDiv(mBuilder, lhs, rhs, "fsub")))

          WEMBED_BINARY_LLVM(o_and_i, LLVMBuildAnd)
          WEMBED_BINARY_LLVM(o_or_i, LLVMBuildOr)
          WEMBED_BINARY_LLVM(o_xor_i, LLVMBuildXor)

          WEMBED_BINARY(o_rotl_i32, emit_rotl(LLVMInt32Type(), lhs, rhs))
          WEMBED_BINARY(o_rotl_i64, emit_rotl(LLVMInt64Type(), lhs, rhs))
          WEMBED_BINARY(o_rotr_i32, emit_rotr(LLVMInt32Type(), lhs, rhs))
          WEMBED_BINARY(o_rotr_i64, emit_rotr(LLVMInt64Type(), lhs, rhs))

          WEMBED_BINARY(o_shl_i32, LLVMBuildShl(mBuilder, lhs, emit_shift_mask(LLVMInt32Type(), rhs), "shl"))
          WEMBED_BINARY(o_shl_i64, LLVMBuildShl(mBuilder, lhs, emit_shift_mask(LLVMInt64Type(), rhs), "shl"))
          WEMBED_BINARY(o_shr_si32, LLVMBuildAShr(mBuilder, lhs, emit_shift_mask(LLVMInt32Type(), rhs), "shrs"))
          WEMBED_BINARY(o_shr_si64, LLVMBuildAShr(mBuilder, lhs, emit_shift_mask(LLVMInt64Type(), rhs), "shrs"))
          WEMBED_BINARY(o_shr_ui32, LLVMBuildLShr(mBuilder, lhs, emit_shift_mask(LLVMInt32Type(), rhs), "shru"))
          WEMBED_BINARY(o_shr_ui64, LLVMBuildLShr(mBuilder, lhs, emit_shift_mask(LLVMInt64Type(), rhs), "shru"))

  #define WEMBED_INTRINSIC(OPCODE, INTRINSIC, ...) case OPCODE: { \
            lLastValue = push(call_intrinsic(INTRINSIC, {__VA_ARGS__})); \
        } break;

          WEMBED_INTRINSIC(o_ctz_i32, mCttz_i32, pop(LLVMInt32Type()), get_const(false))
          WEMBED_INTRINSIC(o_ctz_i64, mCttz_i64, pop(LLVMInt64Type()), get_const(false))
          WEMBED_INTRINSIC(o_clz_i32, mCtlz_i32, pop(LLVMInt32Type()), get_const(false))
          WEMBED_INTRINSIC(o_clz_i64, mCtlz_i64, pop(LLVMInt64Type()), get_const(false))
          WEMBED_INTRINSIC(o_popcnt_i32, mCtpop_i32, pop(LLVMInt32Type()))
          WEMBED_INTRINSIC(o_popcnt_i64, mCtpop_i64, pop(LLVMInt64Type()))
          WEMBED_INTRINSIC(o_sqrt_f32, mSqrt_f32, pop(LLVMFloatType()))
          WEMBED_INTRINSIC(o_sqrt_f64, mSqrt_f64, pop(LLVMDoubleType()))
          WEMBED_INTRINSIC(o_abs_f32, mAbs_f32, pop(LLVMFloatType()))
          WEMBED_INTRINSIC(o_abs_f64, mAbs_f64, pop(LLVMDoubleType()))

          case o_ceil_f32: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMFloatType()), mCeil_f32, mCeil_f64));
          } break;
          case o_ceil_f64: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMDoubleType()), mCeil_f32, mCeil_f64));
          } break;
          case o_floor_f32: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMFloatType()), mFloor_f32, mFloor_f64));
          } break;
          case o_floor_f64: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMDoubleType()), mFloor_f32, mFloor_f64));
          } break;
          case o_trunc_f32: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMFloatType()), mTrunc_f32, mTrunc_f64));
          } break;
          case o_trunc_f64: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMDoubleType()), mTrunc_f32, mTrunc_f64));
          } break;
          case o_nearest_f32: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMFloatType()), mNearest_f32, mNearest_f64));
          } break;
          case o_nearest_f64: {
            lLastValue = push(emit_quiet_nan_or_intrinsic(pop(LLVMDoubleType()), mNearest_f32, mNearest_f64));
          } break;

  #define WEMBED_INTRINSIC_BINARY(OPCODE, INTRINSIC, ...) WEMBED_BINARY(OPCODE, call_intrinsic(INTRINSIC, {lhs, rhs, __VA_ARGS__}));
  #define WEMBED_INTRINSIC_BINARY_MULTI(OPCODE, INTRINSIC, ...) WEMBED_INTRINSIC_BINARY(OPCODE##32, INTRINSIC##32, __VA_ARGS__) \
            WEMBED_INTRINSIC_BINARY(OPCODE##64, INTRINSIC##64, __VA_ARGS__)

          case o_min_f32: {
            LLVMValueRef rhs = pop(LLVMFloatType());
            LLVMValueRef lhs = pop(LLVMFloatType());
            lLastValue = push(emit_min(lhs, rhs));
          } break;
          case o_min_f64: {
            LLVMValueRef rhs = pop(LLVMDoubleType());
            LLVMValueRef lhs = pop(LLVMDoubleType());
            lLastValue = push(emit_min(lhs, rhs));
          } break;
          case o_max_f32: {
            LLVMValueRef rhs = pop(LLVMFloatType());
            LLVMValueRef lhs = pop(LLVMFloatType());
            lLastValue = push(emit_max(lhs, rhs));
          } break;
          case o_max_f64: {
            LLVMValueRef rhs = pop(LLVMDoubleType());
            LLVMValueRef lhs = pop(LLVMDoubleType());
            lLastValue = push(emit_max(lhs, rhs));
          } break;

          WEMBED_INTRINSIC_BINARY_MULTI(o_copysign_f, mCopysign_f)

          case o_neg_f32: {
            lLastValue = push(LLVMBuildFNeg(mBuilder, pop(LLVMFloatType()), "neg"));
          } break;
          case o_neg_f64: {
            lLastValue = push(LLVMBuildFNeg(mBuilder, pop(LLVMDoubleType()), "neg"));
          } break;

  #define WEMBED_CAST(OPCODE, INSTR, ITYPE, OTYPE) case OPCODE: { \
            lLastValue = push(INSTR(mBuilder, pop(ITYPE), OTYPE, #INSTR)); \
          } break;

          WEMBED_CAST(o_wrap_i64, LLVMBuildTrunc, LLVMInt64Type(), LLVMInt32Type())
          WEMBED_CAST(o_extend_si32, LLVMBuildSExt, LLVMInt32Type(), LLVMInt64Type())
          WEMBED_CAST(o_extend_ui32, LLVMBuildZExt, LLVMInt32Type(), LLVMInt64Type())


#define WEMBED_SIGN_EXT(OPCODE, INSTR, ITYPE, OTYPE) case OPCODE: { \
            lLastValue = push(INSTR(mBuilder, LLVMBuildTrunc(mBuilder, pop(), ITYPE, #INSTR"-signext"), OTYPE, #INSTR)); \
          } break;

          WEMBED_SIGN_EXT(o_extend_i32_s8, LLVMBuildSExt, LLVMInt8Type(), LLVMInt32Type())
          WEMBED_SIGN_EXT(o_extend_i32_s16, LLVMBuildSExt, LLVMInt16Type(), LLVMInt32Type())
          WEMBED_SIGN_EXT(o_extend_i64_s8, LLVMBuildSExt, LLVMInt8Type(), LLVMInt64Type())
          WEMBED_SIGN_EXT(o_extend_i64_s16, LLVMBuildSExt, LLVMInt16Type(), LLVMInt64Type())
          WEMBED_SIGN_EXT(o_extend_i64_s32, LLVMBuildSExt, LLVMInt32Type(), LLVMInt64Type())

          case o_demote_f64: {
            lLastValue = push(clear_nan(LLVMBuildFPTrunc(mBuilder, pop(LLVMDoubleType()), LLVMFloatType(), "demote")));
          } break;
          case o_promote_f32: {
            lLastValue = push(clear_nan(LLVMBuildFPExt(mBuilder, pop(LLVMFloatType()), LLVMDoubleType(), "promote")));
          } break;

          WEMBED_CAST(o_convert_f32_si32, LLVMBuildSIToFP, LLVMInt32Type(), LLVMFloatType())
          WEMBED_CAST(o_convert_f32_si64, LLVMBuildSIToFP, LLVMInt64Type(), LLVMFloatType())
          WEMBED_CAST(o_convert_f64_si32, LLVMBuildSIToFP, LLVMInt32Type(), LLVMDoubleType())
          WEMBED_CAST(o_convert_f64_si64, LLVMBuildSIToFP, LLVMInt64Type(), LLVMDoubleType())
          WEMBED_CAST(o_convert_f32_ui32, LLVMBuildUIToFP, LLVMInt32Type(), LLVMFloatType())
          WEMBED_CAST(o_convert_f32_ui64, LLVMBuildUIToFP, LLVMInt64Type(), LLVMFloatType())
          WEMBED_CAST(o_convert_f64_ui32, LLVMBuildUIToFP, LLVMInt32Type(), LLVMDoubleType())
          WEMBED_CAST(o_convert_f64_ui64, LLVMBuildUIToFP, LLVMInt64Type(), LLVMDoubleType())

          WEMBED_CAST(o_reinterpret_i32_f32, LLVMBuildBitCast, LLVMFloatType(), LLVMInt32Type())
          WEMBED_CAST(o_reinterpret_i64_f64, LLVMBuildBitCast, LLVMDoubleType(), LLVMInt64Type())
          WEMBED_CAST(o_reinterpret_f32_i32, LLVMBuildBitCast, LLVMInt32Type(), LLVMFloatType())
          WEMBED_CAST(o_reinterpret_f64_i64, LLVMBuildBitCast, LLVMInt64Type(), LLVMDoubleType())

          case o_prefix_numeric: {
            switch(*mCurrent++) {

            #define WEMBED_SAT_TRUNC(OPCODE, OPCONV, ITYPE, OTYPE, ICTYPE, OCTYPE, FBITSTYPE, OMIN, OMAX) case OPCODE: { \
                LLVMValueRef lValue = pop(ITYPE); \
                LLVMValueRef lBitcast = LLVMBuildBitCast(mBuilder, lValue, FBITSTYPE, "bitcast"); \
                LLVMValueRef lExponent = LLVMBuildAnd(mBuilder, lBitcast, get_const(fp_bits<ICTYPE>::sExponentMask), "exponent"); \
                LLVMValueRef lExponentTest = LLVMBuildICmp(mBuilder, LLVMIntEQ, lExponent, get_const(fp_bits<ICTYPE>::sExponentMask), "exponentTest"); \
                LLVMValueRef lMantissa = LLVMBuildAnd(mBuilder, lBitcast, get_const(fp_bits<ICTYPE>::sMantissaMask), "mantissa"); \
                LLVMValueRef lMantissaTest = LLVMBuildICmp(mBuilder, LLVMIntNE, lMantissa, get_zero(FBITSTYPE), "mantissaTest"); \
                LLVMValueRef lNan = LLVMBuildAnd(mBuilder, lExponentTest, lMantissaTest, "isNan"); \
                LLVMValueRef lMinBoundsTest = LLVMBuildFCmp(mBuilder, LLVMRealOLE, lValue, get_const(OMIN), "minBounds"); \
                LLVMValueRef lMaxBoundsTest = LLVMBuildFCmp(mBuilder, LLVMRealOGE, lValue, get_const(OMAX), "maxBounds"); \
                LLVMValueRef lConverted = OPCONV(mBuilder, lValue, OTYPE, #OPCODE); \
                LLVMValueRef lValueOrMin = LLVMBuildSelect(mBuilder, lMinBoundsTest, get_const(std::numeric_limits<OCTYPE>::min()), lConverted, "valueOrMin"); \
                LLVMValueRef lValueOrMax = LLVMBuildSelect(mBuilder, lMaxBoundsTest, get_const(std::numeric_limits<OCTYPE>::max()), lValueOrMin, "valueOrMax"); \
                LLVMValueRef lValueOrZero = LLVMBuildSelect(mBuilder, lNan, get_zero(OTYPE), lValueOrMax, "valueOrZero"); \
                lLastValue = push(lValueOrZero); \
              } break;

              WEMBED_SAT_TRUNC(o_trunc_sat_f32_si32, LLVMBuildFPToSI, LLVMFloatType(), LLVMInt32Type(), float, int32_t, LLVMInt32Type(), -2147483904.0f, 2147483648.0f);
              WEMBED_SAT_TRUNC(o_trunc_sat_f64_si32, LLVMBuildFPToSI, LLVMDoubleType(), LLVMInt32Type(), double, int32_t, LLVMInt64Type(), -2147483649.0, 2147483648.0);
              WEMBED_SAT_TRUNC(o_trunc_sat_f32_ui32, LLVMBuildFPToUI, LLVMFloatType(), LLVMInt32Type(), float, uint32_t, LLVMInt32Type(), -1.0f, 4294967296.0f);
              WEMBED_SAT_TRUNC(o_trunc_sat_f64_ui32, LLVMBuildFPToUI, LLVMDoubleType(), LLVMInt32Type(), double, uint32_t, LLVMInt64Type(), -1.0, 4294967296.0);
              WEMBED_SAT_TRUNC(o_trunc_sat_f32_si64, LLVMBuildFPToSI, LLVMFloatType(), LLVMInt64Type(), float, int64_t, LLVMInt32Type(), -9223373136366403584.0f, 9223372036854775808.0f);
              WEMBED_SAT_TRUNC(o_trunc_sat_f64_si64, LLVMBuildFPToSI, LLVMDoubleType(), LLVMInt64Type(), double, int64_t, LLVMInt64Type(), -9223372036854777856.0, 9223372036854775808.0);
              WEMBED_SAT_TRUNC(o_trunc_sat_f32_ui64, LLVMBuildFPToUI, LLVMFloatType(), LLVMInt64Type(), float, uint64_t, LLVMInt32Type(), -1.0f, 18446744073709551616.0f);
              WEMBED_SAT_TRUNC(o_trunc_sat_f64_ui64, LLVMBuildFPToUI, LLVMDoubleType(), LLVMInt64Type(), double, uint64_t, LLVMInt64Type(), -1.0, 18446744073709551616.0);

              default:
                throw malformed_exception("unknown numeric instruction");
            }
          } break;

          default:
            throw malformed_exception("unknown instruction");
        }
  #if defined(WEMBED_VERBOSE) && 0
        std::cout << "Step: " << LLVMPrintValueToString(lFunc) << std::endl;
        std::cout << "Eval stack:" << mEvalStack.size() << std::endl;
        for (size_t i = 0; i < mEvalStack.size(); i++) {
          std::cout << '\t' << i << ": ";
          if (mEvalStack[i] == nullptr)
            std::cout << "null";
          else {
            std::cout << LLVMPrintValueToString(mEvalStack[i]) << ", "
                      << LLVMPrintTypeToString(LLVMTypeOf(mEvalStack[i]));
          }
          std::cout << std::endl;
        }
        std::cout << "Control flow stack:" << mCFEntries.size() << std::endl;
        for (size_t i = 0; i < mCFEntries.size(); i++) {
          std::cout << '\t' << i;
          std::cout << ": " << LLVMGetBasicBlockName(mCFEntries[i].mEnd);
          std::cout << ", " << mCFEntries[i].mReachable << ", ";
          std::cout << LLVMPrintTypeToString(mCFEntries[i].mSignature);
          std::cout << std::endl;
        }
        std::cout << "Block stack:" << mBlockEntries.size() << std::endl;
        for (size_t i = 0; i < mBlockEntries.size(); i++) {
          std::cout << '\t' << i << ": ";
          std::cout << LLVMGetBasicBlockName(mBlockEntries[i].mBlock) << ", ";
          std::cout << LLVMPrintTypeToString(mBlockEntries[i].mSignature);
          std::cout << std::endl;
        }
  #endif
        if (lLastValue != nullptr && mDebugSupport) {
          mInstructions[lPC] = lLastValue;
        }
      }
      assert(mCurrent == lEndPos);
      assert(LLVMGetInsertBlock(mBuilder) == lFuncEnd);

#if defined(WEMBED_VERBOSE) && 0
      std::cout << "At PC 0x" << std::hex << (mCurrent - lCodeSectionStart) << std::dec
                << " done parsing code for func " << lFIndex << ": " << LLVMGetValueName(lFunc) << std::endl;
#endif

      LLVMValueRef lReturn;
      if (lReturnType != LLVMVoidType())
        lReturn = LLVMBuildRet(mBuilder, pop(lReturnType));
      else
        lReturn = LLVMBuildRetVoid(mBuilder);

      if (mDebugSupport) {
        uint32_t lFuncOffsetEnd = mCurrent - lCodeSectionStart;
        mInstructions[lFuncOffsetEnd] = lReturn;
        FuncRange lFuncRange;
        lFuncRange.mStartAddress = lFuncOffsetStart;
        lFuncRange.mEndAddress = lFuncOffsetEnd;
        lFuncRange.mFunc = lFunc;
        mFuncRanges.emplace_back(lFuncRange);
      }

  #if defined(WEMBED_VERBOSE) && 0
      std::cout << "Done: " << LLVMPrintValueToString(lFunc) << std::endl;
  #endif
    }
    profile_step("  module/code");
  }

  void addInitialAliasAnalysisPasses(LLVMPassManagerRef pPass) {
    LLVMAddTypeBasedAliasAnalysisPass(pPass);
    LLVMAddScopedNoAliasAAPass(pPass);
  }

  void addInstructionCombiningPass(LLVMPassManagerRef pPass) {
    LLVMAddInstructionCombiningPass(pPass);
  }

  void addFunctionSimplificationPasses(uint8_t pOptLevel, LLVMPassManagerRef pPass) {
    LLVMAddScalarReplAggregatesPassSSA(pPass);
    //createSpeculativeExecutionIfHasBranchDivergencePass
    LLVMAddJumpThreadingPass(pPass);
    LLVMAddCorrelatedValuePropagationPass(pPass);
    LLVMAddCFGSimplificationPass(pPass);
    if (pOptLevel > 2) LLVMAddAggressiveInstCombinerPass(pPass);
    addInstructionCombiningPass(pPass);
    //createPGOMemOPSizeOptLegacyPass
    LLVMAddTailCallEliminationPass(pPass);
    LLVMAddCFGSimplificationPass(pPass);
    LLVMAddReassociatePass(pPass);
    LLVMAddLoopRotatePass(pPass);
    LLVMAddLICMPass(pPass);
    LLVMAddLoopUnswitchPass(pPass);
    LLVMAddCFGSimplificationPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddIndVarSimplifyPass(pPass);
    LLVMAddLoopIdiomPass(pPass);
    LLVMAddLoopDeletionPass(pPass);
    //createSimpleLoopUnrollPass
    if (pOptLevel > 1) {
      LLVMAddMergedLoadStoreMotionPass(pPass);
      LLVMAddNewGVNPass(pPass);
    }
    LLVMAddMemCpyOptPass(pPass);
    LLVMAddSCCPPass(pPass);
    LLVMAddBitTrackingDCEPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddJumpThreadingPass(pPass);
    LLVMAddCorrelatedValuePropagationPass(pPass);
    LLVMAddDeadStoreEliminationPass(pPass);
    LLVMAddLICMPass(pPass);
    LLVMAddLoopRerollPass(pPass);
    LLVMAddAggressiveDCEPass(pPass);
    LLVMAddCFGSimplificationPass(pPass);
    addInstructionCombiningPass(pPass);
  }

  void addLTOOptimizationPasses(uint8_t pOptLevel, LLVMPassManagerRef pPass) {
    LLVMAddGlobalDCEPass(pPass);
    addInitialAliasAnalysisPasses(pPass);
    //createForceFunctionAttrsLegacyPass
    //createInferFunctionAttrsLegacyPass
    if (pOptLevel > 1) {
      //createCallSiteSplittingPass
      //createPGOIndirectCallPromotionLegacyPass
      LLVMAddIPSCCPPass(pPass);
      LLVMAddCalledValuePropagationPass(pPass);
    }
    //LLVMAddFunctionAttrsPass(pPass); Makes call.wast fail
    //createReversePostOrderFunctionAttrsPass
    //createGlobalSplitPass
    //createWholeProgramDevirtPass
    if (pOptLevel == 1)
      return;
    LLVMAddGlobalOptimizerPass(pPass);
    LLVMAddPromoteMemoryToRegisterPass(pPass);
    LLVMAddConstantMergePass(pPass);
    LLVMAddDeadArgEliminationPass(pPass);
    if (pOptLevel > 2) LLVMAddAggressiveInstCombinerPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddFunctionInliningPass(pPass);
    LLVMAddPruneEHPass(pPass);
    LLVMAddGlobalOptimizerPass(pPass);
    LLVMAddGlobalDCEPass(pPass);
    LLVMAddArgumentPromotionPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddJumpThreadingPass(pPass);
    LLVMAddScalarReplAggregatesPass(pPass);
    //LLVMAddFunctionAttrsPass(pPass); Makes call.wast fail
    //createGlobalsAAWrapperPass
    LLVMAddLICMPass(pPass);
    LLVMAddMergedLoadStoreMotionPass(pPass);
    LLVMAddNewGVNPass(pPass);
    LLVMAddMemCpyOptPass(pPass);
    LLVMAddDeadStoreEliminationPass(pPass);
    LLVMAddIndVarSimplifyPass(pPass);
    LLVMAddLoopDeletionPass(pPass);
    //createLoopInterchangePass
    //createSimpleLoopUnrollPass
    LLVMAddLoopVectorizePass(pPass);
    LLVMAddLoopUnrollPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddCFGSimplificationPass(pPass);
    LLVMAddSCCPPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddBitTrackingDCEPass(pPass);
    LLVMAddAlignmentFromAssumptionsPass(pPass);
    addInstructionCombiningPass(pPass);
    LLVMAddJumpThreadingPass(pPass);
  }

  void addLateLTOOptimizationPasses(uint8_t pOptLevel, LLVMPassManagerRef pPass) {
    LLVMAddCFGSimplificationPass(pPass);
    //createEliminateAvailableExternallyPass
    LLVMAddGlobalDCEPass(pPass);
  }

  void module::optimize(uint8_t pOptLevel) {
    if (pOptLevel == 0)
      return;

    mOptLevel = pOptLevel;
    LLVMPassManagerRef lPass = LLVMCreatePassManager();

    addLTOOptimizationPasses(pOptLevel, lPass);
    addLateLTOOptimizationPasses(pOptLevel, lPass);

    LLVMRunPassManager(lPass, mModule);
    LLVMDisposePassManager(lPass);

#if defined(WEMBED_VERBOSE) && 0
    dump_ll(std::cout);
#endif
  }
}  // namespace wembed
