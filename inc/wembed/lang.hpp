#pragma once

#include <cstdint>

namespace wembed {
  using ull_t = unsigned long long int;
  using ul_t = unsigned long int;
  using i32 = int32_t;
  using i64 = int64_t;
  using f32 = float;
  using f64 = double;

  constexpr uint32_t sPageSize = 64 * 1024;
  constexpr uint32_t sMaxPages = 64 * 1024;
  constexpr uint32_t sMaxTableSize = 256;

  enum value_type : uint8_t {
    vt_i32 = 127,
    vt_i64 = 126,
    vt_f32 = 125,
    vt_f64 = 124,
  };

  enum block_type : uint8_t {
    bt_i32 = 127,
    bt_i64 = 126,
    bt_f32 = 125,
    bt_f64 = 124,
    bt_void = 64,
  };

  enum elem_type : uint8_t {
    e_anyfunc = 112,
  };

  struct global_type {
    value_type mType;
    bool mMutable;
  };

  struct resizable_limits {
    uint8_t mFlags;
    uint32_t mInitial;
    uint32_t mMaximum;
  };

  struct table_type {
    elem_type mType;
    resizable_limits mLimits;
    inline size_t initial() const { return mLimits.mInitial; }
    inline size_t maximum() const { return mLimits.mFlags & 0x1 ? mLimits.mMaximum : sMaxTableSize; }
  };

  struct memory_type {
    resizable_limits mLimits;
    inline size_t initial() const { return mLimits.mInitial; }
    inline size_t maximum() const { return mLimits.mFlags & 0x1 ? mLimits.mMaximum : sMaxPages; }
  };

  enum external_kind {
    ek_function = 0,
    ek_table = 1,
    ek_memory = 2,
    ek_global = 3
  };

  enum opcode {
    o_unreachable = 0x0,
    o_nop = 0x1,
    o_block = 0x2,
    o_loop = 0x3,
    o_if = 0x4,
    o_else = 0x5,
    o_end = 0xb,
    o_br = 0xc,
    o_br_if = 0xd,
    o_br_table = 0xe,
    o_return = 0xf,
    o_call = 0x10,
    o_call_indirect = 0x11,
    o_drop = 0x1a,
    o_select = 0x1b,

    o_get_local = 0x20,
    o_set_local = 0x21,
    o_tee_local = 0x22,
    o_get_global = 0x23,
    o_set_global = 0x24,
    o_load_i32 = 0x28,
    o_load_i64 = 0x29,
    o_load_f32 = 0x2a,
    o_load_f64 = 0x2b,
    o_store_i32 = 0x36,
    o_store_i64 = 0x37,
    o_store_f32 = 0x38,
    o_store_f64 = 0x39,
    o_const_i32 = 0x41,
    o_const_i64 = 0x42,
    o_const_f32 = 0x43,
    o_const_f64 = 0x44,

    o_load8_si32 = 0x2c,
    o_load16_si32 = 0x2e,
    o_load8_si64 = 0x30,
    o_load16_si64 = 0x32,
    o_load32_si64 = 0x34,
    o_load8_ui32 = 0x2d,
    o_load16_ui32 = 0x2f,
    o_load8_ui64 = 0x31,
    o_load16_ui64 = 0x33,
    o_load32_ui64 = 0x35,
    o_store8_i32 = 0x3a,
    o_store16_i32 = 0x3b,
    o_store8_i64 = 0x3c,
    o_store16_i64 = 0x3d,
    o_store32_i64 = 0x3e,

    o_eqz_i32 = 0x45,
    o_eqz_i64 = 0x50,
    o_eq_i32 = 0x46,
    o_eq_i64 = 0x51,
    o_ne_i32 = 0x47,
    o_ne_i64 = 0x52,
    o_lt_si32 = 0x48,
    o_lt_si64 = 0x53,
    o_lt_ui32 = 0x49,
    o_lt_ui64 = 0x54,
    o_le_si32 = 0x4c,
    o_le_si64 = 0x57,
    o_le_ui32 = 0x4d,
    o_le_ui64 = 0x58,
    o_gt_si32 = 0x4a,
    o_gt_si64 = 0x55,
    o_gt_ui32 = 0x4b,
    o_gt_ui64 = 0x56,
    o_ge_si32 = 0x4e,
    o_ge_si64 = 0x59,
    o_ge_ui32 = 0x4f,
    o_ge_ui64 = 0x5a,

    o_eq_f32 = 0x5b,
    o_eq_f64 = 0x61,
    o_ne_f32 = 0x5c,
    o_ne_f64 = 0x62,
    o_lt_f32 = 0x5d,
    o_lt_f64 = 0x63,
    o_le_f32 = 0x5f,
    o_le_f64 = 0x65,
    o_gt_f32 = 0x5e,
    o_gt_f64 = 0x64,
    o_ge_f32 = 0x60,
    o_ge_f64 = 0x66,

    o_add_i32 = 0x6a,
    o_add_i64 = 0x7c,
    o_sub_i32 = 0x6b,
    o_sub_i64 = 0x7d,
    o_mul_i32 = 0x6c,
    o_mul_i64 = 0x7e,
    o_div_si32 = 0x6d,
    o_div_si64 = 0x7f,
    o_div_ui32 = 0x6e,
    o_div_ui64 = 0x80,
    o_rem_si32 = 0x6f,
    o_rem_si64 = 0x81,
    o_rem_ui32 = 0x70,
    o_rem_ui64 = 0x82,
    o_add_f32 = 0x92,
    o_add_f64 = 0xa0,
    o_sub_f32 = 0x93,
    o_sub_f64 = 0xa1,
    o_mul_f32 = 0x94,
    o_mul_f64 = 0xa2,
    o_div_f32 = 0x95,
    o_div_f64 = 0xa3,
    o_and_i32 = 0x71,
    o_and_i64 = 0x83,
    o_or_i32 = 0x72,
    o_or_i64 = 0x84,
    o_xor_i32 = 0x73,
    o_xor_i64 = 0x85,
    o_shl_i32 = 0x74,
    o_shl_i64 = 0x86,
    o_shr_si32 = 0x75,
    o_shr_si64 = 0x87,
    o_shr_ui32 = 0x76,
    o_shr_ui64 = 0x88,
    o_rotl_i32 = 0x77,
    o_rotl_i64 = 0x89,
    o_rotr_i32 = 0x78,
    o_rotr_i64 = 0x8a,
    o_clz_i32 = 0x67,
    o_clz_i64 = 0x79,
    o_ctz_i32 = 0x68,
    o_ctz_i64 = 0x7a,
    o_popcnt_i32 = 0x69,
    o_popcnt_i64 = 0x7b,

    o_sqrt_f32 = 0x91,
    o_sqrt_f64 = 0x9f,
    o_ceil_f32 = 0x8d,
    o_ceil_f64 = 0x9b,
    o_floor_f32 = 0x8e,
    o_floor_f64 = 0x9c,
    o_trunc_f32 = 0x8f,
    o_trunc_f64 = 0x9d,
    o_nearest_f32 = 0x90,
    o_nearest_f64 = 0x9e,
    o_abs_f32 = 0x8b,
    o_abs_f64 = 0x99,
    o_neg_f32 = 0x8c,
    o_neg_f64 = 0x9a,
    o_min_f32 = 0x96,
    o_min_f64 = 0xa4,
    o_max_f32 = 0x97,
    o_max_f64 = 0xa5,
    o_copysign_f32 = 0x98,
    o_copysign_f64 = 0xa6,

    o_trunc_f32_si32 = 0xa8,
    o_trunc_f64_si32 = 0xaa,
    o_trunc_f32_si64 = 0xae,
    o_trunc_f64_si64 = 0xb0,
    o_trunc_f32_ui32 = 0xa9,
    o_trunc_f64_ui32 = 0xab,
    o_trunc_f32_ui64 = 0xaf,
    o_trunc_f64_ui64 = 0xb1,
    o_wrap_i64 = 0xa7,
    o_extend_si32 = 0xac,
    o_extend_ui32 = 0xad,
    o_demote_f64 = 0xb6,
    o_promote_f32 = 0xbb,
    o_convert_f32_si32 = 0xb2,
    o_convert_f32_si64 = 0xb4,
    o_convert_f64_si32 = 0xb7,
    o_convert_f64_si64 = 0xb9,
    o_convert_f32_ui32 = 0xb3,
    o_convert_f32_ui64 = 0xb5,
    o_convert_f64_ui32 = 0xb8,
    o_convert_f64_ui64 = 0xba,
    o_reinterpret_i32_f32 = 0xbc,
    o_reinterpret_i64_f64 = 0xbd,
    o_reinterpret_f32_i32 = 0xbe,
    o_reinterpret_f64_i64 = 0xbf,

    o_memory_grow = 0x40,
    o_memory_size = 0x3f,
  };

}  // namespace wembed
