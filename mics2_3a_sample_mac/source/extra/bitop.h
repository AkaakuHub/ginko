#ifndef _BITOP_H_
#define _BITOP_H_

#include "../types.h"

//
//   bit operation library
//

#include <cstddef> // std::size_t
#include <cstdint> // uint64_tなどの定義

// ターゲット環境でSSE,AVX,AVX2が搭載されていない場合はこれらの命令をsoftware emulationにより実行する。
// software emulationなので多少遅いが、SSE2,SSE4.1,SSE4.2,AVX,AVX2,AVX-512の使えない環境でそれに合わせたコードを書く労力が省ける。

// ----------------------------
//   include intrinsic header
// ----------------------------

#if defined(USE_AVX2)
#include <immintrin.h>
#elif defined(USE_SSE42)
#include <nmmintrin.h>
#elif defined(USE_SSE41)
#include <smmintrin.h>
#elif defined(USE_SSSE3)
#include <tmmintrin.h>
#elif defined(USE_SSE2)
#include <emmintrin.h>
#elif defined(IS_ARM)
#include <arm_neon.h>
#include <mm_malloc.h> // for _mm_alloc()
#else
#if defined (__GNUC__)
#include <mm_malloc.h> // for _mm_alloc()
#endif 
#endif


// ----------------------------
//      type define(uint)
// ----------------------------

typedef  uint8_t  u8;
typedef   int8_t  s8;
typedef uint16_t u16;
typedef  int16_t s16;
typedef uint32_t u32;
typedef  int32_t s32;
typedef uint64_t u64;
typedef  int64_t s64;

// ----------------------------
//      inline化の強制
// ----------------------------

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define FORCE_INLINE __forceinline
#elif defined(__INTEL_COMPILER)
#define FORCE_INLINE inline
#elif defined(__GNUC__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

// ----------------------------
//      PEXT(AVX2の命令)
// ----------------------------

#if defined(USE_AVX2) && defined(USE_BMI2)

// for BMI2 : hardwareによるpext実装

// ZEN/ZEN2では、PEXT命令はμOPでのemulationで実装されているらしく、すこぶる遅いらしい。
// PEXT命令を使わず、この下にあるsoftware emulationによるPEXT実装を用いたほうがまだマシらしい。(どうなってんの…)

#define PEXT32(a,b) _pext_u32((u32)(a),(u32)(b))
#if defined (IS_64BIT)
#define PEXT64(a,b) _pext_u64(a,b)
#else
// PEXT32を2回使った64bitのPEXTのemulation
#define PEXT64(a,b) ( u64(PEXT32( (a)>>32 , (b)>>32) << (u32)POPCNT32(b)) | u64(PEXT32(u32(a),u32(b))) )
#endif

#else

// for non-BMI2 : software emulationによるpext実装(やや遅い。とりあえず動くというだけ。)
// ただし64-bitでもまとめて処理できる点や、magic bitboardのような巨大テーブルを用いない点において優れている(かも)
inline uint64_t pext(uint64_t val, uint64_t mask)
{
  uint64_t res = 0;
  for (uint64_t bb = 1; mask; bb += bb) {
    if ((int64_t)val & (int64_t)mask & -(int64_t)mask)
      res |= bb;
    // マスクを1bitずつ剥がしていく実装なので処理時間がbit長に依存しない。
    // ゆえに、32bit用のpextを別途用意する必要がない。
    mask &= mask - 1;
  }
  return res;
}

inline uint32_t PEXT32(uint32_t a, uint32_t b) { return (uint32_t)pext(a, b); }
inline uint64_t PEXT64(uint64_t a, uint64_t b) { return pext(a, b); }

#endif


// ----------------------------
//     POPCNT(SSE4.2の命令)
// ----------------------------

#if defined (USE_SSE42)

#if defined (IS_64BIT)
#define POPCNT32(a) _mm_popcnt_u32(a)
#define POPCNT64(a) _mm_popcnt_u64(a)
#else
#define POPCNT32(a) _mm_popcnt_u32((u32)(a))
// 32bit環境では32bitのpop_count 2回でemulation。
#define POPCNT64(a) (POPCNT32((a)>>32) + POPCNT32(a))
#endif

#else

// software emulationによるpopcnt(やや遅い)

inline int32_t POPCNT32(uint32_t a) {
  a = (a & UINT32_C(0x55555555)) + (a >> 1 & UINT32_C(0x55555555));
  a = (a & UINT32_C(0x33333333)) + (a >> 2 & UINT32_C(0x33333333));
  a = (a & UINT32_C(0x0f0f0f0f)) + (a >> 4 & UINT32_C(0x0f0f0f0f));
  a = (a & UINT32_C(0x00ff00ff)) + (a >> 8 & UINT32_C(0x00ff00ff));
  a = (a & UINT32_C(0x0000ffff)) + (a >> 16 & UINT32_C(0x0000ffff));
  return (int32_t)a;
}
inline int32_t POPCNT64(uint64_t a) {
  a = (a & UINT64_C(0x5555555555555555)) + (a >> 1 & UINT64_C(0x5555555555555555));
  a = (a & UINT64_C(0x3333333333333333)) + (a >> 2 & UINT64_C(0x3333333333333333));
  a = (a & UINT64_C(0x0f0f0f0f0f0f0f0f)) + (a >> 4 & UINT64_C(0x0f0f0f0f0f0f0f0f));
  a = (a & UINT64_C(0x00ff00ff00ff00ff)) + (a >> 8 & UINT64_C(0x00ff00ff00ff00ff));
  a = (a & UINT64_C(0x0000ffff0000ffff)) + (a >> 16 & UINT64_C(0x0000ffff0000ffff));
  return (int32_t)a + (int32_t)(a >> 32);
}
#endif

// ----------------------------
//     BSF(bitscan forward)
// ----------------------------

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) // && defined(_WIN64)

#if defined (IS_64BIT)
// 1である最下位のbitのbit位置を得る。0を渡してはならない。
FORCE_INLINE int LSB32(uint32_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanForward(&index, v); return index; }
FORCE_INLINE int LSB64(uint64_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanForward64(&index, v); return index; }

// 1である最上位のbitのbit位置を得る。0を渡してはならない。
FORCE_INLINE int MSB32(uint32_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanReverse(&index, v); return index; }
FORCE_INLINE int MSB64(uint64_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanReverse64(&index, v); return index; }

#else

// 32bit環境では64bit版を要求されたら2回に分けて実行。
FORCE_INLINE int LSB32(uint32_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanForward(&index, v); return index; }
FORCE_INLINE int LSB64(uint64_t v) { ASSERT_LV3(v != 0); return uint32_t(v) ? LSB32(uint32_t(v)) : 32 + LSB32(uint32_t(v >> 32)); }

FORCE_INLINE int MSB32(uint32_t v) { ASSERT_LV3(v != 0); unsigned long index; _BitScanReverse(&index, v); return index; }
FORCE_INLINE int MSB64(uint64_t v) { ASSERT_LV3(v != 0); return uint32_t(v >> 32) ? 32 + MSB32(uint32_t(v >> 32)) : MSB32(uint32_t(v)); }
#endif

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) || defined(__aarch64__) || defined(__ANDROID__) )

FORCE_INLINE int LSB32(const u32 v) { ASSERT_LV3(v != 0); return __builtin_ctzll(v); }
FORCE_INLINE int LSB64(const u64 v) { ASSERT_LV3(v != 0); return __builtin_ctzll(v); }
FORCE_INLINE int MSB32(const u32 v) { ASSERT_LV3(v != 0); return 63 ^ __builtin_clzll(v); }
FORCE_INLINE int MSB64(const u64 v) { ASSERT_LV3(v != 0); return 63 ^ __builtin_clzll(v); }

#endif

// ----------------------------
//    BSLR
// ----------------------------

// 最下位bitをresetする命令。

#if defined(USE_AVX2) & defined(IS_64BIT)
// これは、BMI1の命令であり、ZEN1/ZEN2であっても使ったほうが速い。
#define BLSR(x) _blsr_u64(x)
#else
#define BLSR(x) (x & (x-1))
#endif

// ----------------------------
//    pop_lsb
// ----------------------------

// 1である最下位bitを1bit取り出して、そのbit位置を返す。0を渡してはならない。
// sizeof(T)<=4 なら LSB32(b)で済むのだが、これをコンパイル時に評価させるの、どう書いていいのかわからん…。
// デフォルトでLSB32()を呼ぶようにしてuint64_tのときだけ64bit版を用意しておく。

template <typename T> FORCE_INLINE int pop_lsb(T& b) { int index = LSB32(b);  b = T(BLSR(b)); return index; }
FORCE_INLINE int pop_lsb(u64& b) { int index = LSB64(b);  b = BLSR(b); return index; }

#endif