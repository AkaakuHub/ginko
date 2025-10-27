#ifndef _BITBOARD_H_
#define _BITBOARD_H_

#include "types.h"

// --------------------
//     Bitboard
// --------------------

// Bitboard関連のテーブル初期化のための関数
namespace Bitboards {
void init();
}

// Bitboardクラスは、コンストラクタでの初期化が保証できないので(オーバーヘッドがあるのでやりたくないので)
// GCC 7.1.0以降で警告が出るのを回避できない。ゆえに、このクラスではこの警告を抑制する。
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

// Bitboard本体クラス

struct alignas(16) Bitboard {
  u32 p;

  Bitboard &operator=(const Bitboard &rhs) {
    this->p = rhs.p;
    return *this;
  }

  Bitboard(const Bitboard &bb) { this->p = bb.p; }

  // --- ctor

  // 初期化しない。このとき中身は不定。
  Bitboard() {}

  // pの値を直接指定しての初期化。(Bitboard定数の初期化のときのみ用いる)
  Bitboard(u32 p);

  // sqの升が1のBitboardとして初期化する。
  Bitboard(Square sq);

  // 値を直接代入する。
  void set(u32 p);

  // --- property

  // Stockfishのソースとの互換性がよくなるようにboolへの暗黙の型変換書いておく。
  operator bool() const;

  // bit test命令
  // if (lhs & rhs)とか(lhs & sq) と書くべきところを
  // if (lhs.test(rhs)) とか(lhs.test(ssq))
  // 書くことでSSE命令を用いて高速化する。

  bool test(Bitboard rhs) const;
  bool test(Square sq) const { return test(Bitboard(sq)); }

  // --- operator

  // 下位bitから1bit拾ってそのbit位置を返す。
  // 絶対に1bitはnon zeroと仮定
  // while(to = bb.pop())
  //  make_move(from,to);
  // のように用いる。
  FORCE_INLINE Square pop() { return Square(pop_lsb(p)); }

  // このBitboardの値を変えないpop()
  FORCE_INLINE Square pop_c() const { return Square(LSB32(p)); }

  // 1のbitを数えて返す。
  int pop_count() const { return (int)(POPCNT32(p)); }

  // 代入型演算子

  Bitboard &operator|=(const Bitboard &b1) {
    this->p |= b1.p;
    return *this;
  }
  Bitboard &operator&=(const Bitboard &b1) {
    this->p &= b1.p;
    return *this;
  }
  Bitboard &operator^=(const Bitboard &b1) {
    this->p ^= b1.p;
    return *this;
  }
  Bitboard &operator+=(const Bitboard &b1) {
    this->p += b1.p;
    return *this;
  }
  Bitboard &operator-=(const Bitboard &b1) {
    this->p -= b1.p;
    return *this;
  }

  // 左シフト(縦型Bitboardでは左1回シフトで1段下の升に移動する)
  // ※　シフト演算子は歩の利きを求めるためだけに使う。
  Bitboard &operator<<=(int shift) { /*ASSERT_LV3(shift == 1);*/
    p = p << shift;
    return *this;
  }

  // 右シフト(縦型Bitboardでは右1回シフトで1段上の升に移動する)
  Bitboard &operator>>=(int shift) { /*ASSERT_LV3(shift == 1);*/
    p = p >> shift;
    return *this;
  }

  // 比較演算子

  bool operator==(const Bitboard &rhs) const { return this->p == rhs.p; }
  bool operator!=(const Bitboard &rhs) const { return this->p != rhs.p; }

  // 2項演算子

  Bitboard operator&(const Bitboard &rhs) const {
    return Bitboard(*this) &= rhs;
  }
  Bitboard operator|(const Bitboard &rhs) const {
    return Bitboard(*this) |= rhs;
  }
  Bitboard operator^(const Bitboard &rhs) const {
    return Bitboard(*this) ^= rhs;
  }
  Bitboard operator+(const Bitboard &rhs) const {
    return Bitboard(*this) += rhs;
  }
  Bitboard operator<<(const int i) const { return Bitboard(*this) <<= i; }
  Bitboard operator>>(const int i) const { return Bitboard(*this) >>= i; }

  // range-forで回せるようにするためのhack(少し遅いので速度が要求されるところでは使わないこと)
  Square operator*() { return pop(); }
  void operator++() {}
};

// 抑制していた警告を元に戻す。
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif

// --- Bitboardの実装

inline Bitboard::Bitboard(u32 p_) : p(p_) {}

// 値を直接代入する。
inline void Bitboard::set(u32 p_) { this->p = p_; }

inline Bitboard::operator bool() const { return POPCNT32(p) > 0; }

inline bool Bitboard::test(Bitboard rhs) const { return (*this & rhs); }

// --- Bitboard定数

// sqの升が1であるbitboard
extern Bitboard SquareBB[SQ_NB];
inline Bitboard::Bitboard(Square sq) { *this = SquareBB[sq]; }

// 全升が1であるBitboard
// p[0]の63bit目は0
extern Bitboard ALL_BB;

// 全升が0であるBitboard
extern Bitboard ZERO_BB;

// Square型との演算子
inline Bitboard operator|(const Bitboard &b, Square s) {
  return b | SquareBB[s];
}
inline Bitboard operator&(const Bitboard &b, Square s) {
  return b & SquareBB[s];
}
inline Bitboard operator^(const Bitboard &b, Square s) {
  return b ^ SquareBB[s];
}

// 単項演算子
inline Bitboard operator~(const Bitboard &a) { return a ^ ALL_BB; }

// range-forで回せるようにするためのhack(少し遅いので速度が要求されるところでは使わないこと)
inline const Bitboard begin(const Bitboard &b) { return b; }
inline const Bitboard end(const Bitboard &) { return ZERO_BB; }

// Bitboardの1の升を'*'、0の升を'.'として表示する。デバッグ用。
std::ostream &operator<<(std::ostream &os, const Bitboard &board);

// --------------------
//     Bitboard定数
// --------------------

// 各筋を表現するBitboard定数
extern Bitboard FILE1_BB;
extern Bitboard FILE2_BB;
extern Bitboard FILE3_BB;
extern Bitboard FILE4_BB;
extern Bitboard FILE5_BB;

// 各段を表現するBitboard定数
extern Bitboard RANK1_BB;
extern Bitboard RANK2_BB;
extern Bitboard RANK3_BB;
extern Bitboard RANK4_BB;
extern Bitboard RANK5_BB;

// 各筋を表現するBitboard配列
extern Bitboard FILE_BB[FILE_NB];

// 各段を表現するBitboard配列
extern Bitboard RANK_BB[RANK_NB];

// ForwardRanksBBの定義)
//    c側の香の利き = 飛車の利き & ForwardRanksBB[c][rank_of(sq)]
//
// すなわち、
// color == BLACKのとき、n段目よりWHITE側(1からn-1段目)を表現するBitboard。
// color == WHITEのとき、n段目よりBLACK側(n+1から9段目)を表現するBitboard。
// このアイデアはAperyのもの。
extern Bitboard ForwardRanksBB[COLOR_NB][RANK_NB];

// 先手から見て1段目からr段目までを表現するBB(US==WHITEなら、5段目から数える)
// ForwardRanksBB[BLACK][n-1] := RANK_1からRANK_[n-1]段目を表すBitboard
// ForwardRanksBB[WHITE][n-1] := RANK_[n+1]からRANK_5段目を表すBitboard
inline const Bitboard rank1_n_bb(Color US, const Rank r) {
  ASSERT_LV2(is_ok(r));
  return ForwardRanksBB[US][(US == BLACK ? r + 1 : 3 - r)];
}

// 敵陣を表現するBitboard。
extern Bitboard EnemyField[COLOR_NB];
inline const Bitboard enemy_field(Color Us) { return EnemyField[Us]; }

// 歩が打てる筋を得るためのmask。指し手生成で用いる。
// ~FILE_BB[SquareToFile[pawnSq]].p[Bitboard::part(pawnSq)]の意味。
extern u32 PAWN_DROP_MASKS[SQ_NB];

// 2升に挟まれている升を返すためのテーブル(その2升は含まない)
// この配列には直接アクセスせずにbetween_bb()を使うこと。
// 配列サイズが大きくてcache汚染がひどいのでシュリンクしてある。
extern Bitboard BetweenBB[89];
extern u16 BetweenIndex[SQ_NB][SQ_NB];

// 2升に挟まれている升を表すBitboardを返す。sq1とsq2が縦横斜めの関係にないときはZERO_BBが返る。
inline const Bitboard between_bb(Square sq1, Square sq2) {
  return BetweenBB[BetweenIndex[sq1][sq2]];
}

// 2升を通過する直線を返すためのテーブル
// 2つ目のindexは[0]:右上から左下、[1]:横方向、[2]:左上から右下、[3]:縦方向の直線。
// この配列には直接アクセスせず、line_bb()を使うこと。
extern Bitboard LineBB[SQ_NB][4];

// 2升を通過する直線を返すためのBitboardを返す。sq1とsq2が縦横斜めの関係にないときに呼び出してはならない。
inline const Bitboard line_bb(Square sq1, Square sq2) {
  static_assert(Effect8::DIRECT_RU == 0 && Effect8::DIRECT_LD == 7, "");
  auto directions = Effect8::directions_of(sq1, sq2);
  ASSERT_LV3(directions != 0);
  static const int a[8] = {0, 1, 2, 3, 3, 2, 1, 0};
  return LineBB[sq1][a[(int)Effect8::pop_directions(directions)]];
}

#if 0
// →　高速化のために、Effect8::directions_ofを使って実装しているのでコメントアウト。(shogi.hにおいて)
inline bool aligned(Square s1, Square s2, Square s3) {
	return LineBB[s1][s2] & s3;
}
#endif

// sqの升にいる敵玉に王手となるc側の駒ptの候補を得るテーブル。第2添字は(pr-1)を渡して使う。
// 直接アクセスせずに、check_candidate_bb()、around24_bb()を用いてアクセスすること。
extern Bitboard CheckCandidateBB[SQ_NB][KING - 1][COLOR_NB];
extern Bitboard CheckCandidateKingBB[SQ_NB];

// sqの升にいる敵玉に王手となるus側の駒ptの候補を得る
// pr == ROOKは無条件全域なので代わりにHORSEで王手になる領域を返す。
// pr == KINGで呼び出してはならない。それは、around24_bb()のほうを用いる。
inline const Bitboard check_candidate_bb(Color us, Piece pr, Square sq) {
  ASSERT_LV3(PAWN <= pr && pr < KING && sq <= SQ_NB && is_ok(us));
  return CheckCandidateBB[sq][pr - 1][us];
}

// ある升の24近傍のBitboardを返す。
inline const Bitboard around24_bb(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return CheckCandidateKingBB[sq];
}

// --------------------
// 利きのためのテーブル
// --------------------

// 利きのためのライブラリ
// 注意)
// ここのテーブルを直接参照せず、kingEffect()など、利きの関数を経由して用いること。

// --- 近接駒の利き

// 具体的なPiece名を指定することがほとんどなので1本の配列になっているメリットがあまりないので配列を分ける。

extern Bitboard KingEffectBB[SQ_NB];
extern Bitboard GoldEffectBB[SQ_NB][COLOR_NB];
extern Bitboard SilverEffectBB[SQ_NB][COLOR_NB];
extern Bitboard KnightEffectBB[SQ_NB][COLOR_NB];
extern Bitboard PawnEffectBB[SQ_NB][COLOR_NB];

// 盤上の駒をないものとして扱う、遠方駒の利き。香、角、飛
extern Bitboard LanceStepEffectBB[SQ_NB][COLOR_NB];
extern Bitboard BishopStepEffectBB[SQ_NB];
extern Bitboard RookStepEffectBB[SQ_NB];

// -- 飛車の縦の利き

// 飛車の縦方向の利きを求めるときに、指定した升sqの属するfileのbitをshiftし、
// index を求める為に使用する。(from Apery)
extern u8 Slide[SQ_NB];
extern u32 RookFileEffect[RANK_NB + 1][8];

#if !defined(USE_MAGIC_BITBOARD)

// --- 角の利き

extern Bitboard BishopEffect[2][68 + 1];
extern Bitboard BishopEffectMask[2][SQ_NB];
extern int BishopEffectIndex[2][SQ_NB];

// --- 飛車の横の利き

extern Bitboard RookRankEffect[FILE_NB + 1][8];

#else

// Apery型の遠方駒の利きの実装

// --- 角の利き

extern Bitboard BishopAttack[128];
extern int BishopAttackIndex[SQ_NB];
extern Bitboard BishopBlockMask[SQ_NB];

extern const int BishopBlockBits[SQ_NB];
extern const int BishopShiftBits[SQ_NB];

// --- 飛車の利き

extern Bitboard RookAttack[784];
extern int RookAttackIndex[SQ_NB];
extern Bitboard RookBlockMask[SQ_NB];

extern const int RookBlockBits[SQ_NB];
extern const int RookShiftBits[SQ_NB];

// BMI2命令がないなら、Magicテーブルが必要

extern const u32 RookMagic[SQ_NB];
extern const u32 BishopMagic[SQ_NB];

#endif // !defined(USE_MAGIC_BITBOARD)

// --------------------
//   大駒・小駒の利き
// --------------------

// --- 近接駒

// 王の利き
inline Bitboard kingEffect(const Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return KingEffectBB[sq];
}

// 歩の利き
inline Bitboard pawnEffect(const Color c, const Square sq) {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
  return PawnEffectBB[sq][c];
}

// Bitboardに対する歩の利き
// color = BLACKのとき、51の升は49の升に移動するので、注意すること。
// (51の升にいる先手の歩は存在しないので、歩の移動に用いる分には問題ないが。)
inline Bitboard pawnEffect(const Color c, const Bitboard bb) {
  // Apery型の縦型Bitboardにおいては歩の利きはbit shiftで済む。
  ASSERT_LV3(is_ok(c));
  return c == BLACK ? (bb >> 1) : c == WHITE ? (bb << 1) : ZERO_BB;
}

// 桂の利き
inline Bitboard knightEffect(const Color c, const Square sq) {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
  return KnightEffectBB[sq][c];
}

// 銀の利き
inline Bitboard silverEffect(const Color c, const Square sq) {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
  return SilverEffectBB[sq][c];
}

// 金の利き
inline Bitboard goldEffect(const Color c, const Square sq) {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
  return GoldEffectBB[sq][c];
}

// --- 遠方仮想駒(盤上には駒がないものとして求める利き)

// 盤上の駒を考慮しない角の利き
inline Bitboard bishopStepEffect(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return BishopStepEffectBB[sq];
}

// 盤上の駒を考慮しない飛車の利き
inline Bitboard rookStepEffect(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return RookStepEffectBB[sq];
}

// 盤上の駒を考慮しない香の利き
inline Bitboard lanceStepEffect(Color c, Square sq) {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
  return LanceStepEffectBB[sq][c];
}

// 盤上の駒を無視するQueenの動き。
inline Bitboard queenStepEffect(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return rookStepEffect(sq) | bishopStepEffect(sq);
}

// 縦横十字の利き 利き長さ=1升分。
inline Bitboard cross00StepEffect(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return rookStepEffect(sq) & kingEffect(sq);
}

// 斜め十字の利き 利き長さ=1升分。
inline Bitboard cross45StepEffect(Square sq) {
  ASSERT_LV3(sq <= SQ_NB);
  return bishopStepEffect(sq) & kingEffect(sq);
}

// --- 遠方駒(盤上の駒の状態を考慮しながら利きを求める)

// 飛車の縦の利き(これはPEXTを用いていないのでどんな環境でも遅くはない)
inline Bitboard rookFileEffect(Square sq, const Bitboard &occupied) {
  ASSERT_LV3(sq <= SQ_NB);
  const int index = (occupied.p >> Slide[sq]) & 0x7;
  File f = file_of(sq);
  return Bitboard(RookFileEffect[rank_of(sq)][index] << int(f | RANK_1));
}

// 香 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard lanceEffect(Color c, Square sq, const Bitboard &occupied) {
  // rootFileEffect()は遅くないのでこれを利用する。
  return rookFileEffect(sq, occupied) & lanceStepEffect(c, sq);
}

#if !defined(USE_MAGIC_BITBOARD)

// Haswellのpext()を呼び出す。occupied = occupied bitboard , mask =
// 利きの算出に絡む升が1のbitboard
// この関数で戻ってきた値をもとに利きテーブルを参照して、遠方駒の利きを得る。
// PEXT命令を使うのでZEN1/ZEN2では遅い。
inline uint32_t occupiedToIndex(const Bitboard &occupied,
                                const Bitboard &mask) {
  return PEXT32(occupied.p, mask.p);
}

// 角の右上と左下方向への利き
inline Bitboard bishopEffect0(Square sq, const Bitboard &occupied) {
  ASSERT_LV3(sq <= SQ_NB);
  const Bitboard block0(occupied & BishopEffectMask[0][sq]);
  return BishopEffect[0][BishopEffectIndex[0][sq] +
                         occupiedToIndex(block0, BishopEffectMask[0][sq])];
}

// 角の左上と右下方向への利き
inline Bitboard bishopEffect1(Square sq, const Bitboard &occupied) {
  ASSERT_LV3(sq <= SQ_NB);
  const Bitboard block1(occupied & BishopEffectMask[1][sq]);
  return BishopEffect[1][BishopEffectIndex[1][sq] +
                         occupiedToIndex(block1, BishopEffectMask[1][sq])];
}

// 角 : occupied bitboardを考慮しながら角の利きを求める
inline Bitboard bishopEffect(Square sq, const Bitboard &occupied) {
  return bishopEffect0(sq, occupied) | bishopEffect1(sq, occupied);
}

// 飛車の横の利き(これはPEXTを使っているのでPEXTが遅い環境だと遅い)
inline Bitboard rookRankEffect(Square sq, const Bitboard &occupied) {
  ASSERT_LV3(sq <= SQ_NB);
  // 将棋盤をシフトして、SQ_31, SQ_21, SQ_11に飛車の横方向の情報を持ってくる。
  // このbitを直列化して7bit取り出して、これがindexとなる。
  // しかし、r回の右シフトを以下の変数uに対して行なうと計算完了まで待たされるので、
  // PEXT32()の第二引数のほうを左シフトしておく。
  int r = rank_of(sq);
  u32 index = PEXT32(occupied.p >> 5, 0b10000100001 << r);
  return RookRankEffect[file_of(sq)][index] << r;
}

// 飛 : occupied bitboardを考慮しながら飛車の利きを求める
inline Bitboard rookEffect(Square sq, const Bitboard &occupied) {
  return rookFileEffect(sq, occupied) | rookRankEffect(sq, occupied);
}

#else

// Aperyの遠方駒の実装
// Magic Bitboardで処理する。

// magic number を使って block の模様から利きのテーブルへのインデックスを算出
inline u32 occupiedToIndex(const Bitboard &block, const u32 magic,
                           const int shiftBits) {
  const int mask = (1 << (32 - shiftBits)) - 1;
  return ((block.p * magic) >> shiftBits) & mask;
}

inline Bitboard rookEffect(const Square sq, const Bitboard &occupied) {
  const Bitboard block(occupied & RookBlockMask[sq]);
  return RookAttack[RookAttackIndex[sq] +
                    occupiedToIndex(block, RookMagic[sq], RookShiftBits[sq])];
}

inline Bitboard bishopEffect(const Square sq, const Bitboard &occupied) {
  const Bitboard block(occupied & BishopBlockMask[sq]);
  return BishopAttack[BishopAttackIndex[sq] +
                      occupiedToIndex(block, BishopMagic[sq],
                                      BishopShiftBits[sq])];
}

// 飛車の横の利き(一度、飛車の利きを求めてからマスクしているのでやや遅い。どうしても必要な時だけ使う)
inline Bitboard rookRankEffect(Square sq, const Bitboard &occupied) {
  return rookEffect(sq, occupied) & RANK_BB[rank_of(sq)];
}

#endif // !defined(USE_MAGIC_BITBOARD)

// --- 馬と龍

// 馬 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard horseEffect(Square sq, const Bitboard &occupied) {
  return bishopEffect(sq, occupied) | kingEffect(sq);
}

// 龍 : occupied bitboardを考慮しながら香の利きを求める
inline Bitboard dragonEffect(Square sq, const Bitboard &occupied) {
  return rookEffect(sq, occupied) | kingEffect(sq);
}

// --------------------
//   汎用性のある利き
// --------------------

// 盤上sqに駒pc(先後の区別あり)を置いたときの利き。
// pc == QUEENだと馬+龍の利きが返る。
extern Bitboard effects_from(Piece pc, Square sq, const Bitboard &occ);

// --------------------
//   Bitboard tools
// --------------------

// 2bit以上あるかどうかを判定する。縦横斜め方向に並んだ駒が2枚以上であるかを判定する。この関係にないと駄目。
// この関係にある場合、Bitboard::merge()によって被覆しないことがBitboardのレイアウトから保証されている。
inline bool more_than_one(const Bitboard &bb) { return POPCNT32(bb.p) > 1; }

// Aperyで使われているforeachBBマクロに似たマクロ。
// Bitboard
// bbに対して、1であるbitのSquareがsqに入ってきて、このときにTを呼び出す。
// bb.p[0]のbitに対してはT(0)と呼び出す。bb.p[1]のbitに対してはT(1)と呼び出す。
// 使用例) foreachBB(bb,sq,[&](int part){ ... } );
// bbは破壊しないものとする。
template <typename T>
FORCE_INLINE void foreachBB(Bitboard &bb, Square &sq, T t) {

  u64 p_ = bb.p;
  while (p_) {
    sq = (Square)pop_lsb(p_);
    t();
  }
}

#endif // #ifndef _BITBOARD_H_
