/**
 * This program is used to run wasm32 generated through wasi sysroot
 */

#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <vector>

#include <wembed.hpp>

using namespace std;
using namespace std::filesystem;
using namespace wembed;

void usage(const string &pProgName) {
  cout << "usage: " << pProgName << " <flags> [-- <args>]\n"
          "  \n"
          "  runs '_start' from a wasm32 module, compiled with clang&wasi-libc\n"
          "  \n"
          "  available flags:\n"
          "    -i <wasm>  module input path (required)\n"
          "    -O<level>  optimisation level ([0-4] default: 0)\n"
          "    -g         enable debug support\n"
          "    -d         emit optimized llvm IR before executing\n"
          "  \n"
          "  result 1 in case of error, or the return value of the wasm's main function."
       << endl;
}

int main(int argc, char **argv) {
  profile_step("Start");

  llvm_init();

  profile_step("LLVM Init");

  if (argc < 2) {
    cerr << "Invalid argument count, expecting at least 2, got " << argc << endl;
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  path lWasmPath;
  uint8_t lOptLevel = 4;
  bool lDump = false;
  bool lDebug = false;
  vector<string_view> lArgs;

  for (int i = 1; i < argc; i++) {
    if (argv[i] == std::string("--")) {
      if (argc == i + 1) {
        cerr << "-- should be followed by at least one argument" << endl;
        usage(argv[0]);
        return EXIT_FAILURE;
      }
      for (int j = i + 1; j < argc; j++)
        lArgs.emplace_back(argv[j]);
      break;
    }
    if (argv[i][0] == '-') {
      switch(argv[i][1]) {
        case 'O': {
          char lRaw = argv[i][2];
          if (lRaw < '0' || lRaw > '4') {
            cerr << "-O accepts values in the [0-4] range" << endl;
            usage(argv[0]);
            return EXIT_FAILURE;
          }
          lOptLevel = (uint8_t) (argv[i][2] - '0');
        } break;
        case 'i':
          if (argc == i + 1) {
            cerr << "-i should be followed by a path" << endl;
            usage(argv[0]);
            return EXIT_FAILURE;
          }
          lWasmPath = argv[i+1];
          i++;
          break;
        case 'd':
          lDump = true;
          break;
        case 'g':
          lDebug = true;
          break;
        default:
          cerr << "Can't parse argument: " << argv[i] << endl;
          usage(argv[0]);
          return EXIT_FAILURE;
      }
    }
    else {
      cerr << "Can't parse argument: " << argv[i] << endl;
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (lWasmPath.empty()) {
    cerr << "wasm path is required" << endl;
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  profile_step("Args parsing");

  ifstream lModuleHandle(lWasmPath, ios::binary);
  if (!lModuleHandle.is_open()) {
    std::cerr << "can't open module: " << lWasmPath.string() << endl;
    return EXIT_FAILURE;
  }
  std::string lModuleBin((istreambuf_iterator<char>(lModuleHandle)),
                  istreambuf_iterator<char>());
  lModuleHandle.close();

  profile_step("Module read");

  resolvers_t lResolvers = {
      {"wasi_unstable", wasi::make_unstable_resolver()},
  };

  try {
    module lModule((uint8_t*)lModuleBin.data(), lModuleBin.size(), {"wasi_unstable"}, lDebug);
    profile_step("Module parse");

    lModule.optimize(lOptLevel);
    profile_step("Module optimize");

    if (lDump) {
      lModule.dump_ll(cout);
      profile_step("Module dump");
    }

    wasi::wasi_context lWasiContext;
    lWasiContext.add_preopen_host();
    lWasiContext.add_env_host();
    lWasiContext.add_arg(absolute(lWasmPath).string());
    for (const auto &lArg : lArgs) {
      lWasiContext.add_arg(lArg);
    }
    profile_step("Wasi context initialization");

    context lContext(lModule, lResolvers, &lWasiContext);
    profile_step("Context initialization");

    auto lEntry = lContext.get_export("_start");
    if (lEntry.mPointer == nullptr) {
      std::cerr << "can't find '_start' export" << endl;
      return EXIT_FAILURE;
    }
    if (lEntry.mKind != ek_function) {
      std::cerr << "'_start' export isn't a function" << endl;
      return EXIT_FAILURE;
    }

    auto lEntryFn = lContext.get_fn<void()>("_start");
    profile_step("Find _start symbol");

    lEntryFn();
    profile_step("_start done");

    return EXIT_SUCCESS;
  }
  catch(const std::exception &e) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}
