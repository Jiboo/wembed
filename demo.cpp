#include <iostream>

#include <wembed.hpp>

void print_i32(int32_t pData) {
  std::cout << "print_i32: " << pData << std::endl;
}

int main(int argc, char **argv) {
  wembed::llvm_init();

  /*
  (module
    ;; Host imported global
    (global $host_glob (import "host" "glob_i32") (mut i32))
    (export "host_glob" (global $host_glob))

    ;; Host imported function
    (func $host_print_i32 (import "host" "print_i32") (param i32))
    (export "main.print_i32" (func $host_print_i32))
    (func (export "print_arg") (param i32) (call $host_print_i32 (get_local 0)))

    ;; Module global
    (global $glob (export "glob") (mut i32) (i32.const 142))
    (func (export "get") (result i32) (get_global $glob))
    (func (export "set") (param i32) (set_global $glob (get_local 0)))
  )
  */

  // Generated using wat2wasm -o demo.wasm demo.wast && xxd -i demo.wasm
  unsigned char demo_wasm[] = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60,
      0x01, 0x7f, 0x00, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x23, 0x02, 0x04, 0x68,
      0x6f, 0x73, 0x74, 0x08, 0x67, 0x6c, 0x6f, 0x62, 0x5f, 0x69, 0x33, 0x32,
      0x03, 0x7f, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x09, 0x70, 0x72, 0x69,
      0x6e, 0x74, 0x5f, 0x69, 0x33, 0x32, 0x00, 0x00, 0x03, 0x04, 0x03, 0x00,
      0x01, 0x00, 0x06, 0x07, 0x01, 0x7f, 0x01, 0x41, 0x8e, 0x01, 0x0b, 0x07,
      0x3d, 0x06, 0x09, 0x68, 0x6f, 0x73, 0x74, 0x5f, 0x67, 0x6c, 0x6f, 0x62,
      0x03, 0x00, 0x0e, 0x6d, 0x61, 0x69, 0x6e, 0x2e, 0x70, 0x72, 0x69, 0x6e,
      0x74, 0x5f, 0x69, 0x33, 0x32, 0x00, 0x00, 0x09, 0x70, 0x72, 0x69, 0x6e,
      0x74, 0x5f, 0x61, 0x72, 0x67, 0x00, 0x01, 0x04, 0x67, 0x6c, 0x6f, 0x62,
      0x03, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x02, 0x03, 0x73, 0x65, 0x74,
      0x00, 0x03, 0x0a, 0x14, 0x03, 0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
      0x04, 0x00, 0x23, 0x01, 0x0b, 0x06, 0x00, 0x20, 0x00, 0x24, 0x01, 0x0b
  };
  unsigned int demo_wasm_len = 156;

  // Host variable that will be imported by the module
  int32_t glob = 0;

  // Define a resolver that will be used to resolve imports from the "host" namespace
  auto lHostResolver = [&glob](std::string_view pFieldName) -> wembed::resolve_result_t {

    // Static hardcoded table
    const static std::unordered_map<std::string_view, wembed::resolve_result_t> sSpectestMappings = {
      {"glob_i32", {(void*) &glob, wembed::ek_global, wembed::hash_ctype<int32_t>()}},
      {"print_i32", {(void*) &print_i32, wembed::ek_function, wembed::hash_fn_ctype_ptr(print_i32)}},
    };

    auto lFound = sSpectestMappings.find(pFieldName);
    if (lFound == sSpectestMappings.end())
      return {nullptr, wembed::ek_global, 0x0};
    return lFound->second;
  };

  // Make the map of namespace resolver, if we were using multiple modules, we'd need to add them here
  wembed::resolvers_t lResolvers = {
    {"host", lHostResolver},
  };

  // Parse the module bytecode and translate it to LLVM IR
  wembed::module mod(demo_wasm, demo_wasm_len);

  // Create a JIT context, and generate native code
  wembed::context ctx(mod, lResolvers);

  // Access to an exported function, allowing to call it
  auto lGetGlob = ctx.get_fn<int32_t>("get");
  std::cout << "glob = " << lGetGlob() << std::endl;

  auto lSetGlob = ctx.get_fn<void, int32_t>("set");
  lSetGlob(42);

  // Access to an exported global in memory, allowing to read/write to it
  int32_t *lGlobPtr = ctx.get_global<int32_t>("glob");
  std::cout << "glob = " << *lGlobPtr << std::endl;

  auto lPrintArg = ctx.get_fn<void, int32_t>("print_arg");
  lPrintArg(1);

  // This export ends up just being an "alias"
  auto lMainPrint = ctx.get_fn<void, int32_t>("main.print_i32");
  lMainPrint(2);

  // Compare the address of the global "alias" vs our global on the stack
  int32_t *lHostGlob = ctx.get_global<int32_t>("host_glob");
  std::cout << "host_glob addr " << lHostGlob << " vs " << &glob << std::endl;

  return EXIT_SUCCESS;
}

/*
Sample output:
  resolved print_i32 to 0x45eb90
  resolved glob_i32 to 0x7ffd7f8be4f8
  glob = 142
  glob = 42
  print_i32: 1
  print_i32: 2
  host_glob addr 0x7ffd7f8be4f8 vs 0x7ffd7f8be4f8
*/
