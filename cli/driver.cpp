/**
 * This program is used tp run wasm32 generated through wasmception
 */

#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

// used to forward syscall to host
#include <asm/unistd_32.h> // syscall names
#include <sys/uio.h> // writev
#include <sys/ioctl.h>
#include <sys/mman.h> // madvise

#include <wembed.hpp>

using namespace std;
using namespace std::filesystem;
using namespace wembed;

memory *vm;

struct iovec32 {
  i32 mOffset;
  uint32_t mSize;
};

uint32_t syscall_brk(uint32_t pNewSize) {
  uint32_t lHeapSize = (uint32_t)vm->size() * sPageSize;
  if (pNewSize > lHeapSize) {
    uint32_t lIncByte = pNewSize - lHeapSize;
    uint32_t lIncPages = (uint32_t)ceil(lIncByte / sPageSize);
    vm->resize(vm->size() + lIncPages);
    lHeapSize = (uint32_t)vm->size() * sPageSize;
  }
  return lHeapSize;
}

i32 syscall_writev(i32 pFd, iovec32 *pVecOffset, uint32_t pVecSize) {
  //dump_hex(pVecOffset, sizeof(iovec32) * pVecSize);
  vector<iovec> lTranslatedVec(pVecSize);
  for (int i = 0; i < pVecSize; i++) {
    lTranslatedVec[i].iov_base = vm->data() + pVecOffset[i].mOffset;
    lTranslatedVec[i].iov_len = pVecOffset[i].mSize;
  }
  return (i32)writev(pFd, lTranslatedVec.data(), pVecSize);
}

i32 syscall_ioctl(i32 pFd, uint32_t pRequest, void* pArgp) {
  return ioctl(pFd, pRequest, pArgp);
}

i32 syscall_madvise(void* pArgp, uint32_t pSize, uint32_t pAdvice) {
  return madvise(pArgp, pSize, pAdvice);
}

/*i32 syscall_mmap2(void *addr, i32 length, i32 prot,  i32 flags, i32 fd, i32 pgoffset) {
  // FIXME Check params syscall_mmap2 0, 65536, 3, 34, -1, 0
  return syscall_brk(vm->size() * sPageSize + length);
}*/

i32 env_syscall0(i32 a) {
  switch(a) {
    default:
      cerr << "unimplemented syscall0 " << a << endl;
      return -1;
  }
}
i32 env_syscall1(i32 a, i32 b) {
  switch(a) {
    case __NR_brk: return syscall_brk(b);
    default:
      cerr << "unimplemented syscall1 " << a << endl;
      return -1;
  }
}
i32 env_syscall2(i32 a, i32 b, i32 c) {
  switch(a) {
    default:
      cerr << "unimplemented syscall2 " << a << endl;
      return -1;
  }
}
i32 env_syscall3(i32 a, i32 b, i32 c, i32 d) {
  switch(a) {
    case __NR_ioctl: return syscall_ioctl(b, (uint32_t)c, (void*)(vm->data() + c));
    case __NR_writev: return syscall_writev(b, (iovec32*)(vm->data() + c), (uint32_t)d);
    case __NR_madvise: return syscall_madvise((iovec32*)(vm->data() + b), (uint32_t)c, (uint32_t)d);
    default:
      cerr << "unimplemented syscall3 " << a << endl;
      return -1;
  }
}
i32 env_syscall4(i32 a, i32 b, i32 c, i32 d, i32 e) {
  switch(a) {
    default:
      cerr << "unimplemented syscall4 " << a << endl;
      return -1;
  }
}
i32 env_syscall5(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f) {
  switch(a) {
    default:
      cerr << "unimplemented syscall5 " << a << endl;
      return -1;
  }
}
i32 env_syscall6(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f, i32 g) {
  switch(a) {
    //case __NR_mmap2: return syscall_mmap2((void*)(/*vm->data() + */b), c, d, e, f, g);
    default:
      cerr << "unimplemented syscall6 " << a << endl;
      return -1;
  }
}

void usage(const string &pProgName) {
  cout << "usage: " << pProgName << " <flags> [-- <args>]\n"
          "  \n"
          "  runs 'main' from a wasm32 module, compiled with wasmception\n"
          "  \n"
          "  available flags:\n"
          "    -i <wasm>  module input path (required)\n"
          "    -O<level>  optimisation level ([0-4] default: 0)\n"
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

  auto lEnvResolver = [](string_view pFieldName) -> resolve_result_t {
    const static unordered_map<string_view, resolve_result_t> sEnvMappings = {
        {"__syscall0", expose_func(&env_syscall0)},
        {"__syscall1", expose_func(&env_syscall1)},
        {"__syscall",  expose_func(&env_syscall1)},
        {"__syscall2", expose_func(&env_syscall2)},
        {"__syscall3", expose_func(&env_syscall3)},
        {"__syscall4", expose_func(&env_syscall4)},
        {"__syscall5", expose_func(&env_syscall5)},
        {"__syscall6", expose_func(&env_syscall6)},
    };

    auto lFound = sEnvMappings.find(pFieldName);
    if (lFound == sEnvMappings.end())
      throw std::runtime_error(string("unknown env import: ") + string(pFieldName));
    return lFound->second;
  };

  resolvers_t lResolvers = {
      {"env", lEnvResolver},
  };

  try {
    module lModule((uint8_t*)lModuleBin.data(), lModuleBin.size());
    profile_step("Module parse");

    lModule.optimize(lOptLevel);
    profile_step("Module optimize");

    if (lDump) {
      lModule.dump_ll(cout);
      profile_step("Module dump");
    }

    context lContext(lModule, lResolvers);
    profile_step("Context initialization");

    vm = lContext.mem();

    auto lEntry = lContext.get_export("main");
    if (lEntry.mPointer == nullptr) {
      std::cerr << "can't find 'main' export" << endl;
      return EXIT_FAILURE;
    }
    if (lEntry.mKind != ek_function) {
      std::cerr << "'main' export isn't a function" << endl;
      return EXIT_FAILURE;
    }

    auto lDataEnd = lContext.get_global<i32>("__data_end");
    //auto lHeapBase = lContext.get_global<i32>("__heap_base");
    vector<i32> lArgvOffsets;

    i32 lDataVMOffset = *lDataEnd;
    uint8_t *lDataHostOffset = vm->data() + lDataVMOffset;
    //uint8_t *lStart = lDataHostOffset;

    // Add module absolute path as argv[0]
    string lModuleAbsPath = absolute(lWasmPath).string();
    lArgvOffsets.emplace_back(lDataVMOffset);
    memcpy(lDataHostOffset, lModuleAbsPath.data(), lModuleAbsPath.size());
    lDataVMOffset += lModuleAbsPath.size() + 1;
    lDataHostOffset += lModuleAbsPath.size();
    *lDataHostOffset = '\0';
    lDataHostOffset++;

    // Add input args as argv[1+i]
    for (const auto &lArg : lArgs) {
      lArgvOffsets.emplace_back(lDataVMOffset);
      memcpy(lDataHostOffset, lArg.data(), lArg.size());
      lDataVMOffset += lArg.size() + 1;
      lDataHostOffset += lArg.size();
      *lDataHostOffset = '\0';
      lDataHostOffset++;
    }

    i32 *lArgvPtrs = (i32*)lDataHostOffset;
    for (i32 lOffset : lArgvOffsets) {
      *lArgvPtrs++ = lOffset;
    }

    //dump_hex(vm->data() + *lDataEnd, lDataHostOffset - lStart + 4 * lArgvOffsets.size());

    profile_step("Argv construction");

    auto lEntryFn = lContext.get_fn<int(int, char**)>("main");
    profile_step("Find main symbol");

    int lResult = lEntryFn((int)lArgvOffsets.size(), (char**)lDataVMOffset);
    profile_step("Main execution");

    return lResult;
  }
  catch(const std::exception &e) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}
