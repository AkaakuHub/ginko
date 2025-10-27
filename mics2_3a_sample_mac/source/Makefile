
# ビルドターゲット
# normal     : 通常使用用
# tournament : 大会で使う用


# ターゲットCPU
# 利用できるCPU拡張命令を指定する。
# ARM系ならOTHERを指定する。

# TARGET_CPU = AVX2
# TARGET_CPU = SSE42
# TARGET_CPU = SSE41
# TARGET_CPU = SSSE3
# TARGET_CPU = SSE2
# TARGET_CPU = NO_SSE
TARGET_CPU = OTHER
# TARGET_CPU = ZEN1
# TARGET_CPU = ZEN2

# デバッガーを使用するか
DEBUG = OFF
# DEBUG = ON


# CEDの端末でコンパイルする場合はg++8を指定する
# COMPILER = g++8
COMPILER = g++
# COMPILER = clang++


# 標準的なコンパイルオプション
CFLAGS   = -std=c++17 -fno-exceptions -fno-rtti -Wextra -Ofast -MMD -MP -fpermissive
WCFLAGS  =
LDFLAGS  =
LIBS     =
INCLUDE  = # -I../include

# デバッガーを使わないなら、NDEBUGをdefineする。
ifeq ($(DEBUG),OFF)
	CFLAGS += -DNDEBUG
endif

# clang用にCFLAGSなどを変更
ifeq ($(findstring clang++,$(COMPILER)),clang++)
	# stdlib
	CFLAGS += -stdlib=libstdc++

	# C++17のfilesystem
	# LDFLAGS += -lstdc++fs

	# 関数の引数が関数本体で使われていないときに警告出るのうざすぎるので抑制。
	CFLAGS += -Wno-unused-parameter

	# static リンクを行う際に __cxa_guard_acquire __cxa_guard_release の生成を抑止
	#   undefined reference to `__imp___cxa_guard_acquire'
	#   undefined reference to `__imp___cxa_guard_release'
	# static 変数を初期化したかを pthread_mutex_t でロックを取って確認し、
	# 最初の実行なら初期化するスレッドセーフなコードを生成するためのもの。
	# → 本当に消してしまっても大丈夫か？
	WCFLAGS += -fno-threadsafe-statics

else
	ifeq ($(findstring g++,$(COMPILER)),g++)
		# mingw g++ で AVX512 向けビルドを行おうとするとエラーになる問題の回避
		# https://stackoverflow.com/questions/43152633/invalid-register-for-seh-savexmm-in-cygwin
		# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65782
		WCFLAGS += -fno-asynchronous-unwind-tables

		# C++17のfilesystem
		# LDFLAGS += -lstdc++fs

	endif
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += $(WCFLAGS)
	LDFLAGS += -static -Wl,--stack,25000000
	TARGET = minishogi-by-gcc.exe
else
	CFLAGS += -D_LINUX
	TARGET = minishogi-by-gcc
endif

# リンク時最適化。
# CFLAGSとLDFLAGSの両方で-fltoを指定する必要がある。
# cf. https://gcc.gnu.org/onlinedocs/gcc-6.3.0/gcc/Optimize-Options.html#Optimize-Options
LTOFLAGS = -flto

# wstringを使うためにこのシンボル定義が必要。
CFLAGS  += -DUNICODE

# stripの指示。(実行ファイルのサイズを小さく)
LDFLAGS += -Wl,-s

# mingw64では-D_WIN64,-D_WIN32は環境に応じて自動で設定されるので指定すべきではない。
# CFLAGS += -D_WIN64

# これを指定していると、各CPU向けの実行ファイルが生成されないので指定すべきではない。
# CFLAGS   += -march=native

# デバッグ情報を付加
# CFLAGS  += -g3 -ggdb

ifeq ($(findstring g++,$(COMPILER)),g++)
	OPENMP   = -fopenmp
	OPENMP_LDFLAGS =
else ifeq ($(findstring clang++,$(COMPILER)),clang++)
	ifeq ($(MSYSTEM),MINGW64)
		# MSYS2 MINGW64 env
		# libgompを指定した場合、ビルドは通るがOpenMPは無効化される？
		OPENMP = -fopenmp=libgomp
		OPENMP_LDFLAGS =
	else
		ifeq ($(findstring w64-mingw32,$(COMPILER)),w64-mingw32)
			# Ubuntu mingw-w64 clang++ env (experimental)
			OPENMP = -fopenmp=libgomp
			OPENMP_LDFLAGS =
		else
			# Other (normal clang++/libomp env)
			OPENMP = -fopenmp
			OPENMP_LDFLAGS = -lomp
		endif
	endif
endif

CFLAGS += -DNO_EXCEPTIONS
LDFLAGS += -lpthread
LDFLAGS += -v

OBJDIR   = ../obj
ifeq "$(strip $(OBJDIR))" ""
	OBJDIR = ..
endif

SOURCES  = \
	main.cpp            \
	types.cpp           \
	bitboard.cpp        \
	misc.cpp            \
	movegen.cpp         \
	position.cpp        \
	usi.cpp             \
	evaluate.cpp        \
	search.cpp          \
	extra/rp_cmd.cpp    \
	extra/user_test.cpp \

ifeq ($(TARGET_CPU),ZEN1)
	CFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver1

else ifeq ($(TARGET_CPU),ZEN2)
	CFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver2

else ifeq ($(TARGET_CPU),AVX2)
	CFLAGS += -DUSE_AVX2 -DUSE_BMI2 -mbmi -mbmi2 -mavx2 -march=corei7-avx

else ifeq ($(TARGET_CPU),SSE42)
	CFLAGS += -DUSE_SSE42 -msse4.2 -march=corei7

else ifeq ($(TARGET_CPU),SSE41)
	CFLAGS += -DUSE_SSE41 -msse4.1 -march=core2

else ifeq ($(TARGET_CPU),SSSE3)
	CFLAGS += -DUSE_SSSE3 -msse3 -march=core2

else ifeq ($(TARGET_CPU),SSE2)
	CFLAGS += -DUSE_SSE2 -msse2 -march=core2

else ifeq ($(TARGET_CPU),NO_SSE)
	# 32bit用。-m32は、MSYS2 MinGW-64だと無視されるので、
	# MinGW-64の32bit環境用でコンパイルする必要がある。
	CFLAGS += -DNO_SSE -m32 -march=pentium3

else ifeq ($(TARGET_CPU),OTHER)
	CFLAGS += -DNO_SSE

endif


CFLAGS += -DUSE_MAKEFILE

OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS  = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) $(LIBS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html


# ビルドターゲット

# 通常使用。
normal:
	$(MAKE) CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS) $(LTOFLAGS)' $(TARGET)

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET) ${OBJECTS:.o=.gcda}

-include $(DEPENDS)
