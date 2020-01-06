#include <eosio/vm/backend.hpp>
#include <random>

#include <catch2/catch.hpp>
#include "utils.hpp"

using namespace eosio::vm;

extern wasm_allocator wa;

struct softfloat_config {
   static constexpr bool use_softfloat = true;
};

struct hardfloat_config {
   static constexpr bool use_softfloat = false;
};

/*
 * (module
 *  (func (export "fn") (param f64) (result i64)
 *   (local.get 0)
 *   (f32.demote_f64)
 *   (i32.reinterpret_f32)
 *   (i64.extend_i32_u)
 *  )
 * )
 */
std::vector<uint8_t> f32_demote_f64_wasm = {
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60,
  0x01, 0x7c, 0x01, 0x7e, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x01, 0x02,
  0x66, 0x6e, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0xb6,
  0xbc, 0xad, 0x0b
};

struct multi_backend {
   explicit multi_backend(std::vector<uint8_t>& code) :
     soft_interpreter_backend(code),
     soft_jit_backend(code),
     hard_interpreter_backend(code),
     hard_jit_backend(code)
   {
      soft_interpreter_backend.set_wasm_allocator(&wa);
      hard_interpreter_backend.set_wasm_allocator(&wa);
      soft_jit_backend.set_wasm_allocator(&wa);
      hard_jit_backend.set_wasm_allocator(&wa);
   }
   backend<nullptr_t, interpreter, softfloat_config> soft_interpreter_backend;
   backend<nullptr_t, jit, softfloat_config> soft_jit_backend;
   backend<nullptr_t, interpreter, hardfloat_config> hard_interpreter_backend;
   backend<nullptr_t, jit, hardfloat_config> hard_jit_backend;
   template<typename... A>
   std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> call_with_return(A... a) {
      auto x0 = soft_interpreter_backend.call_with_return(nullptr, "env", "fn", a...)->to_ui64();
      auto x1 = soft_jit_backend.call_with_return(nullptr, "env", "fn", a...)->to_ui64();
      auto x2 = hard_interpreter_backend.call_with_return(nullptr, "env", "fn", a...)->to_ui64();
      auto x3 = hard_jit_backend.call_with_return(nullptr, "env", "fn", a...)->to_ui64();
      return {x0, x1, x2, x3};
   }
};

TEST_CASE("test f32.demote_f64", "[float_tests]") {
   backend<nullptr_t, interpreter, softfloat_config> soft_interpreter_backend(f32_demote_f64_wasm);
   backend<nullptr_t, jit, softfloat_config> soft_jit_backend(f32_demote_f64_wasm);
   backend<nullptr_t, interpreter, hardfloat_config> hard_interpreter_backend(f32_demote_f64_wasm);
   backend<nullptr_t, jit, hardfloat_config> hard_jit_backend(f32_demote_f64_wasm);
   soft_interpreter_backend.set_wasm_allocator(&wa);
   hard_interpreter_backend.set_wasm_allocator(&wa);
   soft_jit_backend.set_wasm_allocator(&wa);
   hard_jit_backend.set_wasm_allocator(&wa);

   for(int i = 0; i < (1 << 16); ++i) {
      for(int j = -1; j <= 1; ++j) {
         uint64_t argn = (static_cast<uint64_t>(i) << 48) + static_cast<uint64_t>(j);
         double arg = bit_cast<double>(argn);
         auto x0 = soft_interpreter_backend.call_with_return(nullptr, "env", "fn", arg)->to_ui64();
         auto x1 = soft_jit_backend.call_with_return(nullptr, "env", "fn", arg)->to_ui64();
         auto x2 = hard_interpreter_backend.call_with_return(nullptr, "env", "fn", arg)->to_ui64();
         auto x3 = hard_jit_backend.call_with_return(nullptr, "env", "fn", arg)->to_ui64();
         CHECK(x0 == x1);
         CHECK(x1 == x2);
         CHECK(x2 == x3);
      }
   }
}

/*
 * (module
 *  (func (export "fn") (param f32 f32) (result i64)
 *   (local.get 0)
 *   (local.get 1)
 *   (f32.min)
 *   (i32.reinterpret_f32)
 *   (i64.extend_i32_u)
 *  )
 * )
 */
std::vector<uint8_t> f32_min_wasm = {
   0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60,
   0x02, 0x7d, 0x7d, 0x01, 0x7e, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x01,
   0x02, 0x66, 0x6e, 0x00, 0x00, 0x0a, 0x0b, 0x01, 0x09, 0x00, 0x20, 0x00,
   0x20, 0x01, 0x96, 0xbc, 0xad, 0x0b
};

TEST_CASE("test f32.min", "[float_tests]") {
   multi_backend bkend{f32_min_wasm};
   for(int i = 0; i < (1 << 11); ++i) {
      for(int j = -1; j <= 1; ++j) {
         for(int k = 0; k < (1 << 11); ++k) {
            for(int l = -1; l <= 1; ++l) {
               float arg1 = bit_cast<float>((static_cast<uint32_t>(i) << 21) + static_cast<uint32_t>(j));
               float arg2 = bit_cast<float>((static_cast<uint32_t>(k) << 21) + static_cast<uint32_t>(l));
               auto [x0, x1, x2, x3] = bkend.call_with_return(arg1, arg2);
               CHECK(x0 == x1);
               CHECK(x1 == x2);
               CHECK(x2 == x3);
            }
         }
      }
   }
}

/*
 * (module
 *  (func (export "fn") (param f32 f32) (result i64)
 *   (local.get 0)
 *   (local.get 1)
 *   (f32.max)
 *   (i32.reinterpret_f32)
 *   (i64.extend_i32_u)
 *  )
 * )
 */
std::vector<uint8_t> f32_max_wasm = {
   0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60,
   0x02, 0x7d, 0x7d, 0x01, 0x7e, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x01,
   0x02, 0x66, 0x6e, 0x00, 0x00, 0x0a, 0x0b, 0x01, 0x09, 0x00, 0x20, 0x00,
   0x20, 0x01, 0x97, 0xbc, 0xad, 0x0b
};

TEST_CASE("test f32.max", "[float_tests]") {
   multi_backend bkend{f32_max_wasm};
   for(int i = 0; i < (1 << 11); ++i) {
      for(int j = -1; j <= 1; ++j) {
         for(int k = 0; k < (1 << 11); ++k) {
            for(int l = -1; l <= 1; ++l) {
               float arg1 = bit_cast<float>((static_cast<uint32_t>(i) << 21) + static_cast<uint32_t>(j));
               float arg2 = bit_cast<float>((static_cast<uint32_t>(k) << 21) + static_cast<uint32_t>(l));
               auto [x0, x1, x2, x3] = bkend.call_with_return(arg1, arg2);
               CHECK(x0 == x1);
               CHECK(x1 == x2);
               CHECK(x2 == x3);
            }
         }
      }
   }
}

/*
 * (module
 *  (func (export "fn") (param f64 f64) (result i64)
 *   (local.get 0)
 *   (local.get 1)
 *   (f64.min)
 *   (i64.reinterpret_f64)
 *  )
 * )
 */
std::vector<uint8_t> f64_min_wasm = {
   0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60,
   0x02, 0x7c, 0x7c, 0x01, 0x7e, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x01,
   0x02, 0x66, 0x6e, 0x00, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x20, 0x00,
   0x20, 0x01, 0xa4, 0xbd, 0x0b
};

TEST_CASE("test f64.min", "[float_tests]") {
   multi_backend bkend{f64_min_wasm};
   for(int i = 0; i < (1 << 14); ++i) {
      for(int j = -1; j <= 1; ++j) {
         for(int k = 0; k < (1 << 14); ++k) {
            for(int l = -1; l <= 1; ++l) {
               double arg1 = bit_cast<double>((static_cast<uint64_t>(i) << 50) + static_cast<uint64_t>(j));
               double arg2 = bit_cast<double>((static_cast<uint64_t>(k) << 50) + static_cast<uint64_t>(l));
               auto [x0, x1, x2, x3] = bkend.call_with_return(arg1, arg2);
               CHECK(x0 == x1);
               CHECK(x1 == x2);
               CHECK(x2 == x3);
            }
         }
      }
   }
}

/*
 * (module
 *  (func (export "fn") (param f64 f64) (result i64)
 *   (local.get 0)
 *   (local.get 1)
 *   (f64.max)
 *   (i64.reinterpret_f64)
 *  )
 * )
 */
std::vector<uint8_t> f64_max_wasm = {
   0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60,
   0x02, 0x7c, 0x7c, 0x01, 0x7e, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x01,
   0x02, 0x66, 0x6e, 0x00, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x20, 0x00,
   0x20, 0x01, 0xa5, 0xbd, 0x0b
};

TEST_CASE("test f64.max", "[float_tests]") {
   multi_backend bkend{f64_max_wasm};
   for(int i = 0; i < (1 << 14); ++i) {
      for(int j = -1; j <= 1; ++j) {
         for(int k = 0; k < (1 << 14); ++k) {
            for(int l = -1; l <= 1; ++l) {
               double arg1 = bit_cast<double>((static_cast<uint64_t>(i) << 50) + static_cast<uint64_t>(j));
               double arg2 = bit_cast<double>((static_cast<uint64_t>(k) << 50) + static_cast<uint64_t>(l));
               auto [x0, x1, x2, x3] = bkend.call_with_return(arg1, arg2);
               CHECK(x0 == x1);
               CHECK(x1 == x2);
               CHECK(x2 == x3);
            }
         }
      }
   }
}
