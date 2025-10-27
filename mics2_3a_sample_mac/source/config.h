#ifndef _CONFIG_H_
#define _CONFIG_H_

#define ENGINE_NAME "Sample"
#define ENGINE_VERSION "1"
#define AUTHOR "unknown"

// ----------------------------
//      Target CPU
// ----------------------------

#if !defined(USE_MAKEFILE)

// --- ターゲットCPUの選択
// ターゲットCPUのところだけdefineしてください。(残りは自動的にdefineされます。)
// Visual Studioのプロジェクト設定で「構成のプロパティ」→「C /
// C++」→「コード生成」→「拡張命令セットを有効にする」
// のところの設定の変更も忘れずに。

#define USE_AVX2
// #define USE_SSE42
// #define USE_SSE41
// #define USE_SSSE3
// #define USE_SSE2
// #define NO_SSE

// -- BMI2命令を使う/使わない

// AVX2環境でBMI2が使えるときに、BMI2対応命令を使うのか(ZEN/ZEN2ではBMI2命令は使わないほうが速い)
// AMDのRyzenシリーズ(ZEN1/ZEN2)では、BMI2命令遅いので、これを定義しないほうが速い。
#define USE_BMI2

#else

// Makefileを使ってbuildする

#endif

// -- 以下、必要に応じてdefineする。

// デバッグ時の標準出力への局面表示などに日本語文字列を用いる。
#define PRETTY_JP

// デバッグ時の標準出力への局面表示のとき色を用いる。
#define FONT_COLOR

// --- assertのレベルを6段階で。
//  ASSERT_LV 0 : assertなし(全体的な処理が速い)
//  ASSERT_LV 1 : 軽量なassert
//  　　　…
//  ASSERT_LV 5 : 重度のassert(全体的な処理が遅い)

// ASSERT LVに応じたassert
#ifndef ASSERT_LV
#define ASSERT_LV 0
#endif

// --- ASSERTのリダイレクト
// ASSERTに引っかかったときに、それを"Error : x=1"のように標準出力に出力する。
#define USE_DEBUG_ASSERT

// --------------------
//      configure
// --------------------

// --- assertion tools

// DEBUGビルドでないとassertが無効化されてしまうので無効化されないASSERT
// 故意にメモリアクセス違反を起こすコード。
// USE_DEBUG_ASSERTが有効なときには、ASSERTの内容を出力したあと、3秒待ってから
// アクセス違反になるようなコードを実行する。
#if !defined(USE_DEBUG_ASSERT)
#define ASSERT(X)                                                              \
  {                                                                            \
    if (!(X))                                                                  \
      *(int *)1 = 0;                                                           \
  }
#else
#include <chrono>
#include <iostream>
#include <thread>

#define ASSERT(X)                                                              \
  {                                                                            \
    if (!(X)) {                                                                \
      std::cout << "\nError : ASSERT(" << #X << "), " << __FILE__ << "("       \
                << __LINE__ << "): " << __func__ << std::endl;                 \
      std::this_thread::sleep_for(std::chrono::microseconds(3000));            \
      *(int *)1 = 0;                                                           \
    }                                                                          \
  }
#endif

#define ASSERT_LV_EX(L, X)                                                     \
  {                                                                            \
    if (L <= ASSERT_LV)                                                        \
      ASSERT(X);                                                               \
  }
#define ASSERT_LV1(X) ASSERT_LV_EX(1, X)
#define ASSERT_LV2(X) ASSERT_LV_EX(2, X)
#define ASSERT_LV3(X) ASSERT_LV_EX(3, X)
#define ASSERT_LV4(X) ASSERT_LV_EX(4, X)
#define ASSERT_LV5(X) ASSERT_LV_EX(5, X)

// --- declaration of unreachablity

// switchにおいてdefaultに到達しないことを明示して高速化させる

// デバッグ時は普通にしとかないと変なアドレスにジャンプして原因究明に時間がかかる。
#if defined(_MSC_VER)
#define UNREACHABLE                                                            \
  ASSERT_LV3(false);                                                           \
  __assume(0);
#elif defined(__GNUC__)
#define UNREACHABLE                                                            \
  ASSERT_LV3(false);                                                           \
  __builtin_unreachable();
#else
#define UNREACHABLE ASSERT_LV3(false);
#endif

// --- output for Japanese notation

// PRETTY_JPが定義されているかどうかによって三項演算子などを使いたいので。
#if defined(PRETTY_JP)
constexpr bool pretty_jp = true;
#else
constexpr bool pretty_jp = false;
#endif

// ----------------------------
//      CPU environment
// ----------------------------

// ターゲットが64bitOSかどうか
#if (defined(_WIN64) && defined(_MSC_VER)) ||                                  \
    (defined(__GNUC__) && defined(__x86_64__)) || defined(IS_64BIT)
constexpr bool Is64Bit = true;
#ifndef IS_64BIT
#define IS_64BIT
#endif
#else
constexpr bool Is64Bit = false;
#endif

// TARGET_CPU、Makefileのほうで"ZEN2"のようにダブルコーテーション有りの文字列として定義されているはずだが、
// それが定義されていないならここでUSE_XXXオプションから推定する。
#if !defined(TARGET_CPU)
#if defined(USE_BMI2)
#define BMI2_STR "BMI2"
#else
#define BMI2_STR ""
#endif

#if defined(USE_AVX2)
#define TARGET_CPU "AVX2" BMI2_STR
#elif defined(USE_SSE42)
#define TARGET_CPU "SSE4.2"
#elif defined(USE_SSE41)
#define TARGET_CPU "SSE4.1"
#elif defined(USE_SSSE3)
#define TARGET_CPU "SSSE3"
#elif defined(USE_SSE2)
#define TARGET_CPU "SSE2"
#else
#define TARGET_CPU "noSSE"
#endif
#endif

// 上位のCPUをターゲットとするなら、その下位CPUの命令はすべて使えるはずなので…。

#if defined(USE_AVX2)
#define USE_SSE42
#endif

#if defined(USE_SSE42)
#define USE_SSE41
#endif

#if defined(USE_SSE41)
#define USE_SSSE3
#endif

#if defined(USE_SSSE3)
#define USE_SSE2
#endif

// ZEN1/ZEN2はBMI2命令を使わずにMagic Bitboardを使用する。
// ZEN3ではBMI2を使用する。
#if !defined(USE_BMI2)
#define USE_MAGIC_BITBOARD
#endif

#endif
