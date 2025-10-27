#ifndef _MISC_H_
#define _MISC_H_

#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <vector>

#include "types.h"

// --------------------
//  engine info
// --------------------

// "USI"コマンドに応答するために表示する。
const std::string engine_info();

// 使用したコンパイラについての文字列を返す。
const std::string compiler_info();

// --------------------
//  logger
// --------------------

// cin/coutへの入出力をファイルにリダイレクトを開始/終了する。
void start_logger(bool b);

// --------------------
//  Time[ms] wrapper
// --------------------

typedef std::chrono::milliseconds::rep TimePoint;
static_assert(sizeof(TimePoint) == sizeof(int64_t),
              "TimePoint should be 64 bits");

// ms単位で現在時刻を返す
static TimePoint now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// --------------------
//       乱数
// --------------------
struct PRNG {
  PRNG(u64 seed) : s(seed) { ASSERT_LV1(seed); }

  // 時刻などでseedを初期化する。
  PRNG() {
    // C++11のrandom_device()によるseedの初期化
    // std::random_device rd; s = (u64)rd() + ((u64)rd() << 32);
    // →　msys2のgccでbuildすると同じ値を返すっぽい。なんぞこれ…。

    // time値とか、thisとか色々加算しておく。
    s = (u64)(time(NULL)) + ((u64)(this) << 32) +
        (u64)(std::chrono::high_resolution_clock::now()
                  .time_since_epoch()
                  .count());
  }

  // 乱数を一つ取り出す。
  template <typename T> T rand() { return T(rand64()); }

  // 0からn-1までの乱数を返す。(一様分布ではないが現実的にはこれで十分)
  u64 rand(u64 n) { return rand<u64>() % n; }

  // 内部で使用している乱数seedを返す。
  u64 get_seed() const { return s; }

private:
  u64 s;
  u64 rand64() {
    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
  }
};

// 乱数のseedを表示する。(デバッグ用)
static std::ostream &operator<<(std::ostream &os, PRNG &prng) {
  os << "PRNG::seed = " << std::hex << prng.get_seed() << std::dec;
  return os;
}

// -----------------------
//  探索のときに使う時間管理用
// -----------------------

struct Timer {
  // タイマーを初期化する。以降、elapsed()でinit()してからの経過時間が得られる。
  void reset() { startTime = now(); }

  // 探索開始からの経過時間。単位は[ms]
  // 探索node数に縛りがある場合、elapsed()で探索node数が返ってくる仕様にすることにより、一元管理できる。
  TimePoint elapsed() const;

private:
  // 探索開始時間
  TimePoint startTime;
};

extern Timer Time;

#endif
