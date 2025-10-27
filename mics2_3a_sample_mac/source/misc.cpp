#include <fstream>
#include <sstream>

#include "misc.h"

using namespace std;

namespace {

// --------------------
//  logger
// --------------------

// logging用のhack。streambufをこれでhookしてしまえば追加コードなしで普通に
// cinからの入力とcoutへの出力をファイルにリダイレクトできる。
// cf. http://groups.google.com/group/comp.lang.c++/msg/1d941c0f26ea0d81
struct Tie : public streambuf {
  Tie(streambuf *buf_, streambuf *log_) : buf(buf_), log(log_) {}

  int sync() override { return log->pubsync(), buf->pubsync(); }
  int overflow(int c) override { return write(buf->sputc((char)c), "<< "); }
  int underflow() override { return buf->sgetc(); }
  int uflow() override { return write(buf->sbumpc(), ">> "); }

  int write(int c, const char *prefix) {
    static int last = '\n';
    if (last == '\n')
      log->sputn(prefix, 3);
    return last = log->sputc((char)c);
  }

  streambuf *buf, *log; // 標準入出力 , ログファイル
};

struct Logger {
  static void start(bool b) {
    static Logger log;

    if (b && !log.file.is_open()) {
      log.file.open("io_log.txt", ifstream::out);
      cin.rdbuf(&log.in);
      cout.rdbuf(&log.out);
      cout << "start logger" << endl;
    } else if (!b && log.file.is_open()) {
      cout << "end logger" << endl;
      cout.rdbuf(log.out.buf);
      cin.rdbuf(log.in.buf);
      log.file.close();
    }
  }

private:
  Tie in, out; // 標準入力とファイル、標準出力とファイルのひも付け
  ofstream file; // ログを書き出すファイル

  // clangだとここ警告が出るので一時的に警告を抑制する。
#pragma warning(disable : 4068) // MSVC用の不明なpragmaの抑制
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Logger() : in(cin.rdbuf(), file.rdbuf()), out(cout.rdbuf(), file.rdbuf()) {}
#pragma clang diagnostic pop

  ~Logger() { start(false); }
};

} // namespace

// Trampoline helper to avoid moving Logger to misc.h
void start_logger(bool b) { Logger::start(b); }

// --------------------
//  engine info
// --------------------

const string engine_info() {
  stringstream ss;

  ss << "id name " << ENGINE_NAME << ' ' << ENGINE_VERSION << ' '
     << (Is64Bit ? "64" : "32") << TARGET_CPU << endl
     << "id author by " << AUTHOR << endl;

  return ss.str();
}

// 使用したコンパイラについての文字列を返す。
const std::string compiler_info() {

#define stringify2(x) #x
#define stringify(x) stringify2(x)
#define make_version_string(major, minor, patch)                               \
  stringify(major) "." stringify(minor) "." stringify(patch)

  /// Predefined macros hell:
  ///
  /// __GNUC__           Compiler is gcc, Clang or Intel on Linux
  /// __INTEL_COMPILER   Compiler is Intel
  /// _MSC_VER           Compiler is MSVC or Intel on Windows
  /// _WIN32             Building on Windows (any)
  /// _WIN64             Building on Windows 64 bit

  std::string compiler = "\nCompiled by ";

#ifdef __clang__
  compiler += "clang++ ";
  compiler += make_version_string(__clang_major__, __clang_minor__,
                                  __clang_patchlevel__);
#elif __INTEL_COMPILER
  compiler += "Intel compiler ";
  compiler += "(version ";
  compiler +=
      stringify(__INTEL_COMPILER) " update " stringify(__INTEL_COMPILER_UPDATE);
  compiler += ")";
#elif _MSC_VER
  compiler += "MSVC ";
  compiler += "(version ";
  compiler += stringify(_MSC_FULL_VER) "." stringify(_MSC_BUILD);
  compiler += ")";
#elif __GNUC__
  compiler += "g++ (GNUC) ";
  compiler +=
      make_version_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
  compiler += "Unknown compiler ";
  compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
  compiler += " on Apple";
#elif defined(__CYGWIN__)
  compiler += " on Cygwin";
#elif defined(__MINGW64__)
  compiler += " on MinGW64";
#elif defined(__MINGW32__)
  compiler += " on MinGW32";
#elif defined(__ANDROID__)
  compiler += " on Android";
#elif defined(__linux__)
  compiler += " on Linux";
#elif defined(_WIN64)
  compiler += " on Microsoft Windows 64-bit";
#elif defined(_WIN32)
  compiler += " on Microsoft Windows 32-bit";
#else
  compiler += " on unknown system";
#endif

#ifdef __VERSION__
  // __VERSION__が定義されているときだけ、その文字列を出力する。(MSVCだと定義されていないようだ..)
  compiler += "\n __VERSION__ macro expands to: ";
  compiler += __VERSION__;
#else
  compiler += "(undefined macro)";
#endif

  compiler += "\n";

  return compiler;
}

// --------------------
//  Timer
// --------------------

TimePoint Timer::elapsed() const { return TimePoint(now() - startTime); }

Timer Time;
