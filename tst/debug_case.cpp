/**
 * This file is just to iterate on a testsuite case that would not pass
 */

#include <gtest/gtest.h>
#include "wembed.hpp"
#include "test.hpp"

using namespace wembed;
using namespace std::literals::string_literals;

#include "wembed/wasi_decls.hpp"

TEST(debug, case){
  std::unordered_map<std::string_view, context*> declared, registered;
  auto lSpectestResolver = [](context&, std::string_view pFieldName) -> resolve_result_t {
    const static std::unordered_map<std::string_view, resolve_result_t> sSpectestMappings = {
        {"global_i32",    expose_cglob(&spectest_global_i32)},
        {"global_f32",    expose_cglob(&spectest_global_f32)},
        {"global_f64",    expose_cglob(&spectest_global_f64)},
        {"print",         expose_func(&spectest_print)},
        {"print_i32",     expose_func(&spectest_print_i32)},
        {"print_i32_f32", expose_func(&spectest_print_i32_f32)},
        {"print_f64_f64", expose_func(&spectest_print_f64_f64)},
        {"print_f32",     expose_func(&spectest_print_f32)},
        {"print_f64",     expose_func(&spectest_print_f64)},
        {"table",         expose_table(&spectest_tab)},
        {"memory",        expose_memory(&spectest_mem)},
    };
    auto lFound = sSpectestMappings.find(pFieldName);
    if (lFound == sSpectestMappings.end())
      return {nullptr, ek_global, 0x0};
    return lFound->second;
  };

  wembed::resolvers_t resolvers = {
      {"spectest", lSpectestResolver},
  };
  struct module_resolver {
    context *mContext;
    module_resolver(context *pContext) : mContext(pContext) {}
    resolve_result_t operator()(context&, std::string_view pFieldName) {
      return mContext->get_export(std::string(pFieldName));
    }
  };

  uint8_t lCode29[] = {  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60,
                         0x00, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x00, 0x00, 0x04, 0x04, 0x01, 0x70,
                         0x00, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x31, 0x04, 0x06, 0x6d,
                         0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x05, 0x74, 0x61, 0x62, 0x6c,
                         0x65, 0x01, 0x00, 0x0d, 0x67, 0x65, 0x74, 0x20, 0x6d, 0x65, 0x6d, 0x6f,
                         0x72, 0x79, 0x5b, 0x30, 0x5d, 0x00, 0x00, 0x0c, 0x67, 0x65, 0x74, 0x20,
                         0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x30, 0x5d, 0x00, 0x01, 0x0a, 0x11,
                         0x02, 0x07, 0x00, 0x41, 0x00, 0x2d, 0x00, 0x00, 0x0b, 0x07, 0x00, 0x41,
                         0x00, 0x11, 0x00, 0x00, 0x0b};
/*(module $Ms
  (type $t (func (result i32)))
  (memory (export "memory") 1)
  (table (export "table") 1 funcref)
  (func (export "get memory[0]") (type $t)
    (i32.load8_u (i32.const 0))
  )
  (func (export "get table[0]") (type $t)
    (call_indirect (type $t) (i32.const 0))
  )
)*/

  module lModule29(lCode29, sizeof(lCode29));
  lModule29.optimize();
  context lCtx29(lModule29, resolvers);
  declared["$Ms"] = &lCtx29;

  /* (register "Ms" $*/
  registered["Ms"] = declared["$Ms"];
  resolvers["Ms"] = module_resolver(registered["Ms"]);

  uint8_t lTrappingBin30[] = {  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60,
                                0x00, 0x01, 0x7f, 0x60, 0x00, 0x00, 0x02, 0x1b, 0x02, 0x02, 0x4d, 0x73,
                                0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x01, 0x02, 0x4d,
                                0x73, 0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x01, 0x70, 0x00, 0x01, 0x03,
                                0x03, 0x02, 0x00, 0x01, 0x08, 0x01, 0x01, 0x09, 0x07, 0x01, 0x00, 0x41,
                                0x00, 0x0b, 0x01, 0x00, 0x0a, 0x0c, 0x02, 0x06, 0x00, 0x41, 0xad, 0xbd,
                                0x03, 0x0b, 0x03, 0x00, 0x00, 0x0b, 0x0b, 0x0b, 0x01, 0x00, 0x41, 0x00,
                                0x0b, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f};
  /*(module
    (import "Ms" "memory" (memory 1))
    (import "Ms" "table" (table 1 funcref))
    (data (i32.const 0) "hello")
    (elem (i32.const 0) $f)
    (func $f (result i32)
      (i32.const 0xdead)
    )
    (func $main
      (unreachable)
    )
    (start $main)
  )*/
  module lMod30 = wembed::module(lTrappingBin30, sizeof(lTrappingBin30));
  EXPECT_THROW(wembed::context lContext30(lMod30, resolvers), wembed::vm_runtime_exception);

/* (assert_return (invoke $Ms "get memory[0]") (i32.const 104))*/
  { i32 res = declared["$Ms"]->get_fn<i32()>("get memory[0]"s)(); EXPECT_EQ(i32(strtoul("104", nullptr, 0)), res); };

/* (assert_return (invoke $Ms "get table[0]") (i32.const 0xdead))*/
  { i32 res = declared["$Ms"]->get_fn<i32()>("get table[0]"s)(); EXPECT_EQ(i32(strtoul("0xdead", nullptr, 0)), res); };
}
