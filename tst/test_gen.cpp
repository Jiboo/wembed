#include <cassert>

#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <unordered_set>

#include <experimental/filesystem>

#include "wembed.hpp"

using namespace std;
using namespace std::experimental::filesystem;

const unordered_set<string> sBlacklist = {
    // WONT FIX: Text parser related
    "comments",
    "inline-module",

    //FIXME Multimodule support
    "elem",
    "imports",
    "exports",
    "linking",
};

random_device rdevice;
default_random_engine rengine(rdevice());
uniform_int_distribution<int> rdist;

string wast2wasm(string_view pCode) {
  string uid = to_string(rdist(rengine));
  path tmp = temp_directory_path();
  path wast = tmp / (string("tmp.wat") + uid);
  path wasm = tmp / (string("tmp.wasm") + uid);

  ofstream owast(wast.string(), ios::trunc);
  owast.write(pCode.data(), pCode.size());
  owast.close();

  stringstream command;
  command << "wat2wasm --no-check -o " << wasm.string() << ' ' << wast.string();
  if (system(command.str().c_str()) != 0)
    throw std::runtime_error(string("couldn't compile module: ") + command.str());

  ifstream ibin(wasm.string(), ios::ate);
  size_t lBinSize = size_t(ibin.tellg());
  ibin.seekg(0, ios::beg);
  string lResult(lBinSize, '\0');
  ibin.read(lResult.data(), lBinSize);
  ibin.close();

  remove(wast);
  remove(wasm);
  return lResult;
}

string bin2hex(string pInput) {
  string uid = to_string(rdist(rengine));
  path tmp = temp_directory_path();
  path wasm = tmp / (string("tmp.wasm") + uid);
  path hex = tmp / (string("tmp.hex") + uid);

  ofstream obin(wasm.string(), ios::trunc);
  obin.write(pInput.data(), pInput.size());
  obin.close();

  stringstream command;
  command << "xxd -i " << wasm.string() << " > " << hex.string();
  system(command.str().c_str());

  ifstream ihex(hex.string(), ios::ate);
  size_t lHexSize = size_t(ihex.tellg());
  ihex.seekg(0, ios::beg);

  string lHex(lHexSize, '\0');
  ihex.read(lHex.data(), lHexSize);
  ihex.close();

  size_t lStart = lHex.find_first_of('{');
  size_t lEnd = lHex.find_last_of('}');
  string lResult = lHex.substr(lStart + 2, lEnd - lStart - 3);

  remove(wasm);
  remove(hex);
  return lResult;
}

struct token_t {
  string_view mView;
  enum type_t {
    OPAR, CPAR, NAME, STR
  } mType;
};

struct tokenizer_t {
  char *mInput, *mEnd;
  vector<token_t> mBuffer;
  tokenizer_t(char *pInput, size_t pSize) : mInput(pInput), mEnd(pInput + pSize) {
  }
  void buff(token_t pToken) {
    mBuffer.push_back(pToken);
  }
  void skip_spaces() {
    bool handled;
    do {
      handled = false;
      if (mInput >= mEnd)
        return;

      if (isspace(*mInput)) {
        while (mInput < mEnd && isspace(*mInput))
          mInput++;
        handled = true;
      }
      if (mInput + 2 < mEnd) {
        if (mInput[0] == ';' && mInput[1] == ';') {
          handled = true;
          while (mInput < mEnd && *mInput != '\n')
            mInput++;
        } else if (mInput[0] == '(' && mInput[1] == ';') {
          handled = true;
          mInput += 2;
          while (mInput < mEnd && (mInput[0] != ';' || mInput[1] != ')'))
            mInput++;
          mInput += 2;
        }
      }
    } while(handled);
  }
  bool ate() {
    skip_spaces();
    // TODO Skip comments
    skip_spaces();
    return mInput == mEnd;
  }
  bool isname(char c) {
    switch (c) {
      case 'A' ... 'Z': case 'a' ... 'z': case '0' ... '9':
      case '_': case '.': case '+': case '-': case '*': case '/': case '^': case '~':
      case '=': case '<': case '>': case '!': case '?': case '@': case '#': case '$':
      case '%': case '&': case '|': case ':': case '`': case '\\': case '\'':
        return true;
      default:
        return false;
    }
  }
  token_t parse_str() {
    char *start = mInput++;
    while (mInput[0] != '"') {
      mInput++;
      if (*mInput == '\\')
        mInput+=2;
    }
    mInput++;
    return {{start, size_t(mInput - start)}, token_t::STR};
  }
  token_t parse_name() {
    char *start = mInput;
    while(isname(*mInput))
      mInput++;
    return {{start, size_t(mInput - start)}, token_t::NAME};
  }
  token_t next() {
    if (!mBuffer.empty()) {
      token_t lResult = mBuffer.back();
      mBuffer.pop_back();
      return lResult;
    }
    skip_spaces();
    if (*mInput == '(') return {{mInput++, 1}, token_t::OPAR};
    else if (*mInput == ')') return {{mInput++, 1}, token_t::CPAR};
    else if (*mInput == '"') return parse_str();
    else if (isname(*mInput)) return parse_name();
    else
      throw runtime_error("unexpected character");
  }
};

class Transpiler {
public:
  Transpiler(char *pInput, size_t pSize, const string &pName, ostream &pOutput)
      : mTokenizer(pInput, pSize), mOutput(pOutput) {
    mTestName = pName;
    replace(mTestName.begin(), mTestName.end(), '-', '_');
  }

  void parse_tests() {
    while (!mTokenizer.ate()) {
      parse_test();
    }
  }

protected:
  string mTestName;
  tokenizer_t mTokenizer;
  ostream &mOutput;
  size_t mSectionIndex = 0;
  unique_ptr<wembed::module> mModule;

  token_t expect(token_t::type_t pType) {
    token_t result = mTokenizer.next();
    if (result.mType != pType)
      throw runtime_error(string("unexpected token: ") + string(result.mView));
    return result;
  }

  token_t expect(const std::string &pName) {
    token_t result = mTokenizer.next();
    if (result.mView != pName)
      throw runtime_error(string("unexpected token") + string(result.mView));
    return result;
  }

  token_t match_par() {
    size_t lPar = 1;
    token_t lEnd;
    while (lPar > 0 && !mTokenizer.ate()) {
      lEnd = mTokenizer.next();
      switch (lEnd.mType) {
        case token_t::OPAR: lPar++; break;
        case token_t::CPAR: lPar--; break;
        default:
          break;
      }
    }
    return lEnd;
  }

  string_view parse_module() {
    auto lStart = expect(token_t::OPAR);
    auto lId = mTokenizer.next();
    if (lId.mView != "module") {
      mTokenizer.buff(lId);
      mTokenizer.buff(lStart);
      return "";
    }
    //lId = mTokenizer.next();
    if (lId.mType != token_t::OPAR) {

    }

    token_t lEnd = match_par();
    size_t lModuleSize = lEnd.mView.end() - lStart.mView.begin();
    string_view lModuleCode(lStart.mView.data(), lModuleSize);
    return lModuleCode;
  }

  string dump_value(const std::string_view &pType, const std::string_view &pValue) {
    stringstream lOutput;
    if (pType[0] == 'i') {
      lOutput << pType << "(strtoul(\"" << pValue << "\", nullptr, 0))";
    }
    else {
      lOutput << "fp<" << pType << ">(\"" << pValue << "\")";
    }

    return lOutput.str();
  }

  string func_return_type(const string &pFuncName) {
    LLVMValueRef lFunc = mModule->symbol(pFuncName);
    assert(lFunc);
    LLVMTypeRef lFuncSig = LLVMGetElementType(LLVMTypeOf(lFunc));
    LLVMTypeRef lType = LLVMGetReturnType(lFuncSig);
    if (lType == LLVMFloatType())
      return "f32";
    else if (lType == LLVMDoubleType())
      return "f64";
    else if (lType == LLVMInt32Type())
      return "i32";
    else if (lType == LLVMInt64Type())
      return "i64";
    else if (lType == LLVMVoidType())
      return "void";
    else
      throw std::runtime_error("can't determine return type");
  }

  std::string fixescape(const std::string_view &pView) {
    static std::regex findEscape("\\\\([0-9a-zA-Z][0-9a-zA-Z])");
    return std::regex_replace(std::string(pView), findEscape, "\\x$1");
  }

  bool parse_assertion(ostream &pOutput) {
    if (mTokenizer.ate())
      return false;

    auto lStart = expect(token_t::OPAR);
    auto lId = mTokenizer.next();
    if (lId.mView == "invoke") {
      auto lFuncName = fixescape(expect(token_t::STR).mView);
      stringstream lArgTypes, lArgs;
      auto lNext = mTokenizer.next();
      // Parse params
      while (lNext.mType != token_t::CPAR) {
        auto lType = mTokenizer.next().mView.substr(0, 3);
        auto lValue = mTokenizer.next();
        expect(token_t::CPAR);
        lArgTypes << ", " << lType;
        lArgs << dump_value(lType, lValue.mView) << ", ";
        lNext = mTokenizer.next();
      }
      string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);
      pOutput << "  lCtx.get_fn<" << "void" << lArgTypes.str() << ">("
              << lFuncName << "s)(" << lArgsFormatted << ");\n";
    }
    else if (lId.mView == "assert_return") {
      expect(token_t::OPAR);
      expect("invoke");
      auto lFuncName = fixescape(expect(token_t::STR).mView);
      stringstream lArgTypes, lArgs;
      auto lNext = mTokenizer.next();
      // Parse params
      while (lNext.mType != token_t::CPAR) {
        auto lType = mTokenizer.next().mView.substr(0, 3);
        auto lValue = mTokenizer.next();
        expect(token_t::CPAR);
        lArgTypes << ", " << lType;
        lArgs << dump_value(lType, lValue.mView) << ", ";
        lNext = mTokenizer.next();
      }
      assert(lNext.mType == token_t::CPAR);
      string_view lReturnType = "void";
      stringstream lExpectedValue;
      lNext = mTokenizer.next();
      if (lNext.mType == token_t::OPAR) {
        // Parse expected result
        lReturnType = mTokenizer.next().mView.substr(0, 3);
        lExpectedValue << dump_value(lReturnType, mTokenizer.next().mView);
        expect(token_t::CPAR);
        lNext = mTokenizer.next();
      }
      assert(lNext.mType == token_t::CPAR);
      if (lReturnType != "void") {
        pOutput << "  { " << lReturnType << " res = ";
      }
      else
        pOutput << "  ";
      string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);
      pOutput << "lCtx.get_fn<" << lReturnType << lArgTypes.str() << ">("
              << lFuncName << "s)(" << lArgsFormatted << ')';
      if (lReturnType != "void") {
        pOutput << "; EXPECT_EQ(" << lExpectedValue.str() << ", ";
        if (lReturnType[0] == 'i')
          pOutput << "res";
        else
          pOutput << "wembed::fp_bits<" << lReturnType << ">(res)";
        pOutput << "); }";
      }
      pOutput << ";\n";
    }
    else if (lId.mView == "assert_return_canonical_nan") {
      expect(token_t::OPAR);
      expect("invoke");
      auto lFuncName = expect(token_t::STR);
      stringstream lArgTypes, lArgs;
      auto lNext = mTokenizer.next();
      // Parse params
      while (lNext.mType != token_t::CPAR) {
        auto lType = mTokenizer.next().mView.substr(0, 3);
        auto lValue = mTokenizer.next();
        expect(token_t::CPAR);
        lArgTypes << ", " << lType;
        lArgs << dump_value(lType, lValue.mView) << ", ";
        lNext = mTokenizer.next();
      }
      assert(lNext.mType == token_t::CPAR);
      expect(token_t::CPAR);
      string lCleanedName = string(lFuncName.mView.data() + 1, lFuncName.mView.size() - 2);
      string lReturnType = func_return_type(lCleanedName);
      string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);

      pOutput << "  EXPECT_TRUE(canonical_nan<" << lReturnType << ">("
              << "lCtx.get_fn<" << lReturnType << lArgTypes.str() << ">("
              << lFuncName.mView << "s)(" << lArgsFormatted << ")));\n";
    }
    else if (lId.mView == "assert_return_arithmetic_nan") {
      expect(token_t::OPAR);
      expect("invoke");
      auto lFuncName = expect(token_t::STR);
      stringstream lArgTypes, lArgs;
      auto lNext = mTokenizer.next();
      // Parse params
      while (lNext.mType != token_t::CPAR) {
        auto lType = mTokenizer.next().mView.substr(0, 3);
        auto lValue = mTokenizer.next();
        expect(token_t::CPAR);
        lArgTypes << ", " << lType;
        lArgs << dump_value(lType, lValue.mView) << ", ";
        lNext = mTokenizer.next();
      }
      assert(lNext.mType == token_t::CPAR);
      expect(token_t::CPAR);
      string lCleanedName = string(lFuncName.mView.data() + 1, lFuncName.mView.size() - 2);
      string lReturnType = func_return_type(lCleanedName);
      string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);

      pOutput << "  EXPECT_TRUE(arithmetic_nan<" << lReturnType << ">("
              << "lCtx.get_fn<" << lReturnType << lArgTypes.str() << ">("
              << lFuncName.mView << "s)(" << lArgsFormatted << ")));\n";
    }
    else if (lId.mView == "assert_malformed") {
      expect(token_t::OPAR);
      expect("module");
      auto lType = expect(token_t::NAME);
      vector<token_t> lCodeFragments;
      lCodeFragments.push_back(expect(token_t::STR));
      auto lNext = mTokenizer.next();
      while (lNext.mType != token_t::CPAR) {
        assert(lNext.mType == token_t::STR);
        lCodeFragments.push_back(lNext);
        lNext = mTokenizer.next();
      }
      assert(lNext.mType == token_t::CPAR);
      auto lError = expect(token_t::STR);
      expect(token_t::CPAR);

      if (lType.mView == "binary") {
        pOutput << "}\n\n"
                << "TEST(" << mTestName << ", malformed" << ++mSectionIndex << ") {\n"
                << "  std::string lMalformedBin = \n";

        regex lHexReplacer("\\\\([0-9a-fA-F][0-9a-fA-F])");
        for (auto &lFragment : lCodeFragments)
          pOutput << "    " << std::regex_replace(string(lFragment.mView), lHexReplacer, "\\u00$1") << "\n";
        pOutput << ";\n  // Expected error: " << lError.mView << "\n"
                << "  EXPECT_THROW(wembed::module((uint8_t*)lMalformedBin.data(), lMalformedBin.size()),"
                << "malformed_exception);\n";
      }
    }
    else if (lId.mView == "assert_invalid") {
      auto lCodeStart = expect(token_t::OPAR);
      expect("module");
      auto lEnd = match_par();
      auto lError = expect(token_t::STR);
      expect(token_t::CPAR);

      string_view lCode(lCodeStart.mView.data(), lEnd.mView.data() - lCodeStart.mView.data() + 1);
      string lBin = wast2wasm(lCode);
      if (lBin.size() > 0) {
        string lHex = bin2hex(lBin);
        pOutput << "}\n\n"
                << "TEST(" << mTestName << ", invalid" << ++mSectionIndex << ") {\n"
                << "  uint8_t lInvalidBin[] = {" << lHex << "};\n"
                << "  /*" << lCode << "*/\n"
                << "  // Expected error: " << lError.mView << "\n"
                << "  EXPECT_THROW(wembed::module(lInvalidBin, sizeof(lInvalidBin)), wembed::invalid_exception);\n";
      }
    }
    else if (lId.mView == "assert_unlinkable") {
      auto lCodeStart = expect(token_t::OPAR);
      expect("module");
      auto lEnd = match_par();
      auto lError = expect(token_t::STR);
      expect(token_t::CPAR);

      string_view lCode(lCodeStart.mView.data(), lEnd.mView.data() - lCodeStart.mView.data() + 1);
      string lBin = wast2wasm(lCode);
      if (lBin.size() > 0) {
        string lHex = bin2hex(lBin);
        pOutput << "}\n\n"
                << "TEST(" << mTestName << ", unlinkable" << ++mSectionIndex << ") {\n"
                << "  uint8_t lUnlinkableBin[] = {" << lHex << "};\n"
                << "  /*" << lCode << "*/\n"
                << "  // Expected error: " << lError.mView << "\n"
                << "  module lMod = wembed::module(lUnlinkableBin, sizeof(lUnlinkableBin));\n"
                << "  EXPECT_THROW(wembed::context lContext(lMod, \"spectest\", {\n"
                   "    {\"global_i32\", (void*)&spectest_global_i32},\n"
                   "    {\"global_f32\", (void*)&spectest_global_f32},\n"
                   "    {\"global_f64\", (void*)&spectest_global_f64},\n"
                   "    {\"print\", (void*)&spectest_print},\n"
                   "    {\"print_i32\", (void*)&spectest_print_i32},\n"
                   "    {\"print_i32_f32\", (void*)&spectest_print_i32_f32},\n"
                   "    {\"print_f64_f64\", (void*)&spectest_print_f64_f64},\n"
                   "    {\"print_f32\", (void*)&spectest_print_f32},\n"
                   "    {\"print_f64\", (void*)&spectest_print_f64},\n"
                   "    {\"memory\", (void*)&spectest_mem},\n"
                   "  }), wembed::unlinkable_exception);\n";
      }
    }
    else if (lId.mView == "assert_trap" || lId.mView == "assert_exhaustion") {
      expect(token_t::OPAR);
      auto lType = expect(token_t::NAME);
      if (lType.mView == "invoke") {
        auto lFuncName = expect(token_t::STR);
        stringstream lArgTypes, lArgs;
        auto lNext = mTokenizer.next();
        // Parse params
        while (lNext.mType != token_t::CPAR) {
          auto lType = mTokenizer.next().mView.substr(0, 3);
          auto lValue = mTokenizer.next();
          expect(token_t::CPAR);
          lArgTypes << ", " << lType;
          lArgs << dump_value(lType, lValue.mView) << ", ";
          lNext = mTokenizer.next();
        }
        assert(lNext.mType == token_t::CPAR);
        string lCleanedName = string(lFuncName.mView.data() + 1, lFuncName.mView.size() - 2);
        string lReturnType = func_return_type(lCleanedName);
        string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);
        pOutput << "  {\n"
                << "    auto lThrow = [&lCtx]() {\n"
                << "      lCtx.get_fn<" << lReturnType << lArgTypes.str() << ">("
                << lFuncName.mView << "s)(" << lArgsFormatted << ");\n"
                << "    };\n"
                << "    EXPECT_THROW(lThrow(), vm_runtime_exception);\n"
                << "  }\n";
      }
      else {
        std::cerr << "unsuported assert trap: " << lType.mView << std::endl;
        match_par();
      }
      expect(token_t::STR); // trap error string
      expect(token_t::CPAR);
    }
    else if (lId.mView.find_first_of("assert_") == 0) {
      // unsuported assert type
      std::cerr << "unsuported assert type: " << lId.mView << std::endl;
      match_par();
    }
    else {
      // not an assertion, probably a module, put back
      mTokenizer.buff(lId);
      mTokenizer.buff(lStart);
      return false;
    }

    return true;
  }

  void parse_test() {
    mOutput << "TEST(" << mTestName << ", module" << mSectionIndex << ") {\n";

    string_view lModuleSource = parse_module();
    if (lModuleSource.size()) {
      string lModuleBytecode = wast2wasm(lModuleSource);
      string lModuleHex = bin2hex(lModuleBytecode);
      mOutput << "  uint8_t lCode[] = {" << lModuleHex << "};\n";
      mOutput << "/*" << lModuleSource << "*/\n\n";
      mOutput << "  module lModule(lCode, sizeof(lCode));\n";
      mOutput << "  context lCtx(lModule, \"spectest\", {\n"
          "    {\"global_i32\", (void*)&spectest_global_i32},\n"
          "    {\"global_f32\", (void*)&spectest_global_f32},\n"
          "    {\"global_f64\", (void*)&spectest_global_f64},\n"
          "    {\"print\", (void*)&spectest_print},\n"
          "    {\"print_i32\", (void*)&spectest_print_i32},\n"
          "    {\"print_i32_f32\", (void*)&spectest_print_i32_f32},\n"
          "    {\"print_f64_f64\", (void*)&spectest_print_f64_f64},\n"
          "    {\"print_f32\", (void*)&spectest_print_f32},\n"
          "    {\"print_f64\", (void*)&spectest_print_f64},\n"
          "    {\"memory\", (void*)&spectest_mem},\n"
          "  });\n";
      mOutput.flush();
      try {
        mModule = std::make_unique<wembed::module>((uint8_t *) lModuleBytecode.data(),
                                                   lModuleBytecode.size());
      }
      catch (const std::exception &e) {
        std::cerr << "error while compiling module for " << mTestName << " at section " << mSectionIndex << std::endl;
        std::cerr << e.what() << std::endl;
      }
    }

    while(parse_assertion(mOutput))
      mOutput.flush();

    mOutput << "}\n\n";

    mSectionIndex++;
  }
};

void handle(path pInputFile, ostream &pOutput) {
  string lTestName = pInputFile.stem().string();
  if (sBlacklist.find(lTestName) != sBlacklist.end())
    return;
  ifstream is(pInputFile, ios::ate);
  if (!is.is_open())
    throw runtime_error(string("couldn't open file ") + pInputFile.string());
  size_t size = is.tellg();
  vector<char> contents(size);
  is.seekg(0, ios::beg);
  is.read(contents.data(), size);
  Transpiler transpiler(contents.data(), contents.size(), lTestName, pOutput);
  transpiler.parse_tests();
}

int main(int argc,char** argv) {
  if (argc != 3) {
    cerr << "usage: " << argv[0] << " <.wast> <.cpp>" << endl;
    return EXIT_FAILURE;
  }
  path lInputPath(argv[1]);
  if (!is_regular_file(lInputPath)) {
    cerr << argv[1] << " is not a valid file" << endl;
    return EXIT_FAILURE;
  }

  //bool lOutputMain = false;
  ofstream lOutput(argv[2]);
  if (!lOutput.is_open()) {
    cerr << "i/o error: can't open " << argv[2] << std::endl;
    return EXIT_FAILURE;
  }
  lOutput << "#include <gtest/gtest.h>\n";
  lOutput << "#include \"wembed.hpp\"\n";
  lOutput << "#include \"test.hpp\"\n\n";
  lOutput << "using namespace wembed;\n";
  lOutput << "using namespace std::literals::string_literals;\n\n";
  handle(lInputPath, lOutput);
}
