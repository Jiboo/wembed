#include <cassert>

#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_set>

#include <experimental/filesystem>

using namespace std;
using namespace std::experimental::filesystem;

string wast2wasm(string_view pCode) {
  path tmp = temp_directory_path();
  path wast = tmp / string("tmp.wast");
  path wasm = tmp / string("tmp.wasm");
  path hex = tmp / string("tmp.hex");

  ofstream owast(wast.string(), ios::trunc);
  owast.write(pCode.data(), pCode.size());
  owast.close();

  stringstream command;
  command << "wast2wasm -o " << wasm.string() << ' ' << wast.string();
  system(command.str().c_str());

  command.str(std::string());
  command << "hexdump -v -e '1/1 \"0x%02X, \"' " << wasm.string() << " > " << hex.string();
  system(command.str().c_str());

  ifstream ihex(hex.string(), ios::ate);
  size_t lHexSize = size_t(ihex.tellg()) - 2;
  ihex.seekg(0, ios::beg);
  string lResult(lHexSize, '\0');
  ihex.read(lResult.data(), lHexSize);
  ihex.close();

  remove(wast);
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
    while(isspace(*mInput) && mInput < mEnd) mInput++;
  }
  bool ate() {
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
    if (mInput[0] == ';' && mInput[1] == ';') {
      while (*mInput != '\n')
        mInput++;
      return next();
    }
    else if (mInput[0] == '(' && mInput[1] == ';') {
      mInput += 2;
      while (mInput[0] != ';' || mInput[1] != ')')
        mInput++;
      mInput += 2;
      return next();
    }
    else if (*mInput == '(') return {{mInput++, 1}, token_t::OPAR};
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

  bool parse_assertion(ostream &pOutput) {
    if (mTokenizer.ate())
      return false;

    auto lStart = expect(token_t::OPAR);
    auto lId = mTokenizer.next();
    if (lId.mView == "invoke") {
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
      string lArgsFormatted = lArgs.str().substr(0, lArgs.str().size() - 2);
      pOutput << "  lCtx.get_fn<" << "void" << lArgTypes.str() << ">("
              << lFuncName.mView << ")(" << lArgsFormatted << ");\n";
    }
    else if (lId.mView == "assert_return") {
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
              << lFuncName.mView << ")(" << lArgsFormatted << ')';
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
    else if (lId.mView.find_first_of("assert_") == 0) {
      // unsuported assert type
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
    mOutput << "TEST(" << mTestName << ", section" << mSectionIndex << ") {\n";

    string_view lModuleSource = parse_module();
    if (lModuleSource.size()) {
      string lModuleBytecode = wast2wasm(lModuleSource);
      mOutput << "  uint8_t lCode[] = {" << lModuleBytecode << "};\n";
      mOutput << "/*" << lModuleSource << "*/\n\n";
      mOutput << "  module lModule(lCode, sizeof(lCode));\n";
      mOutput << "  context lCtx(lModule, {\n"
          "    {\"spectest_global\", (void*)&spectest_global},\n"
          "    {\"spectest_print\", (void*)&spectest_print},\n"
          "  });\n";
    }

    while(parse_assertion(mOutput));

    mOutput << "}\n\n";

    mSectionIndex++;
  }
};

const unordered_set<string> sBlacklist = {
  // WONT FIX
  "comments",
  "inline-module",

  //FIXME UTF8
  "names",
  "utf8-custom-section-id",
  "utf8-import-field",
  "utf8-import-module",

  //FIXME Multimodule support
  "elem",
  "imports",
  "exports",
  "linking",

  // FIXME https://bugs.llvm.org/show_bug.cgi?id=34941
  "f64",
};

void handle(path pInputFile, ostream &pOutput) {
  string lTestName = pInputFile.stem().string();
  if (sBlacklist.find(lTestName) != sBlacklist.end())
    return;
  ifstream is(pInputFile, ios::ate);
  if (!is.is_open())
    throw runtime_error(string("couldn't open file ") + pInputFile.string());
  size_t size = is.tellg();
#if 1
  //if (!(lTestName == "f32" || lTestName == "f64")) return;
  vector<char> contents(size);
  is.seekg(0, ios::beg);
  is.read(contents.data(), size);
#else
  if (lTestName != "func") return;
  string contents = "(module"
      "  (memory (data \"\\00\\00\\a0\\7f\"))\n"
      "\n"
      "  (func (export \"f32.load\") (result f32) (f32.load (i32.const 0)))\n"
      "  (func (export \"i32.load\") (result i32) (i32.load (i32.const 0)))\n"
      "  (func (export \"f32.store\") (f32.store (i32.const 0) (f32.const nan:0x200000)))\n"
      "  (func (export \"i32.store\") (i32.store (i32.const 0) (i32.const 0x7fa00000)))\n"
      "  (func (export \"reset\") (i32.store (i32.const 0) (i32.const 0)))\n"
      ")\n"
      "\n"
      "(assert_return (invoke \"i32.load\") (i32.const 0x7fa00000))\n"
      "(assert_return (invoke \"f32.load\") (f32.const nan:0x200000))\n"
      "(invoke \"reset\")\n"
      "(assert_return (invoke \"i32.load\") (i32.const 0x0))\n"
      "(assert_return (invoke \"f32.load\") (f32.const 0.0))\n"
      "(invoke \"f32.store\")\n"
      "(assert_return (invoke \"i32.load\") (i32.const 0x7fa00000))\n"
      "(assert_return (invoke \"f32.load\") (f32.const nan:0x200000))\n"
      "(invoke \"reset\")\n"
      "(assert_return (invoke \"i32.load\") (i32.const 0x0))\n"
      "(assert_return (invoke \"f32.load\") (f32.const 0.0))\n"
      "(invoke \"i32.store\")\n"
      "(assert_return (invoke \"i32.load\") (i32.const 0x7fa00000))\n"
      "(assert_return (invoke \"f32.load\") (f32.const nan:0x200000))"
  ;
#endif

  cout << "handling " << lTestName << endl;
  Transpiler transpiler(contents.data(), contents.size(), lTestName, pOutput);
  transpiler.parse_tests();
}

int main(int argc,char** argv) {
  if (argc != 3) {
    cerr << "usage: " << argv[0] << " <.wast tests folder> <output path>" << endl;
    return EXIT_FAILURE;
  }
  path lInputPath(argv[1]);
  if (!is_directory(lInputPath)) {
    cerr << argv[1] << " is not a valid directory" << endl;
    return EXIT_FAILURE;
  }
  ofstream lOutput(argv[2]);
  lOutput << "#include <gtest/gtest.h>\n";
  lOutput << "#include \"wembed.hpp\"\n";
  lOutput << "#include \"test_utils.hpp\"\n\n";

  lOutput << "using namespace wembed;\n\n";

  for (auto &lFile : directory_iterator(lInputPath)) {
    if (lFile.path().extension().compare(".wast") == 0)
      handle(lFile.path(), lOutput);
  }

  lOutput << "int main(int argc, char **argv) {\n";
  lOutput << "  ::testing::InitGoogleTest(&argc, argv);\n";
  lOutput << "  LLVMLinkInMCJIT();\n";
  lOutput << "  LLVMInitializeNativeTarget();\n";
  lOutput << "  LLVMInitializeNativeAsmPrinter();\n";

  lOutput << "#ifdef WEMBED_NATIVE_CODE_DUMP\n";
  lOutput << "  LLVMInitializeAllTargetInfos();\n";
  lOutput << "  LLVMInitializeAllTargetMCs();\n";
  lOutput << "  LLVMInitializeAllDisassemblers();\n";
  lOutput << "#endif\n";

  lOutput << "  return RUN_ALL_TESTS();\n";
  lOutput << "}\n";
}
