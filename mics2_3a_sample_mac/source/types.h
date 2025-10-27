#ifndef _TYPES_H_
#define _TYPES_H_

// --------------------
// release configurations
// --------------------

// コンパイル時の設定などは以下のconfig.hを変更すること。
#include "config.h"

// --------------------
//      include
// --------------------

// あまりたくさんここに書くとコンパイルが遅くなるので書きたくないのだが…。

#include "extra/bitop.h"

#include <algorithm> // std::max()を使うので仕方ない
#include <climits>   // INT_MAXがこのheaderで必要なので仕方ない
#include <iostream>  // iostreamに対する<<使うので仕方ない
#include <string>    // std::string使うので仕方ない

// --------------------
//      手番
// --------------------

// 手番
enum Color {
  BLACK = 0 /*先手*/,
  WHITE = 1 /*後手*/,
  COLOR_NB /* =2 */,
  COLOR_ZERO = 0,
};

// 相手番を返す
constexpr Color operator~(Color c) { return (Color)(c ^ 1); }

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(Color c) { return COLOR_ZERO <= c && c < COLOR_NB; }

// 出力用(USI形式ではない)　デバッグ用。
std::ostream &operator<<(std::ostream &os, Color c);

// --------------------
//        筋
// --------------------

//  例) FILE_3なら3筋。
enum File : int {
  FILE_1,
  FILE_2,
  FILE_3,
  FILE_4,
  FILE_5,
  FILE_NB,
  FILE_ZERO = 0
};

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(File f) { return FILE_ZERO <= f && f < FILE_NB; }

// USIの指し手文字列などで筋を表す文字列をここで定義されたFileに変換する。
constexpr File toFile(char c) { return (File)(c - '1'); }

// Fileを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → ８
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 8
std::string pretty(File f);

// USI形式でFileを出力する
static std::ostream &operator<<(std::ostream &os, File f) {
  os << (char)('1' + f);
  return os;
}

// --------------------
//        段
// --------------------

// 例) RANK_4なら4段目。
enum Rank : int {
  RANK_1,
  RANK_2,
  RANK_3,
  RANK_4,
  RANK_5,
  RANK_NB,
  RANK_ZERO = 0
};

// 正常な値であるかを検査する。assertで使う用。
constexpr bool is_ok(Rank r) { return RANK_ZERO <= r && r < RANK_NB; }

// 移動元、もしくは移動先の升のrankを与えたときに、そこが成れるかどうかを判定する。
constexpr bool canPromote(const Color c, const Rank fromOrToRank) {
  // ASSERT_LV1(is_ok(c) && is_ok(fromOrToRank));
  // 先手5bit(5段) + 後手5bit(5段) = 10bitのbit列に対して、判定すればいい。
  // このcastにおいて、VC++2015ではwarning C4800が出る。
  return static_cast<bool>(0x1001u & (1u << ((c << 3) + fromOrToRank)));
}

// 後手の段なら先手から見た段を返す。
// 例) relative_rank(WHITE,RANK_1) == RANK_5
constexpr Rank relative_rank(Color c, Rank r) {
  return c == BLACK ? r : (Rank)(4 - r);
}

// USIの指し手文字列などで段を表す文字列をここで定義されたRankに変換する。
constexpr Rank toRank(char c) { return (Rank)(c - 'a'); }

// Rankを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → 八
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 8
std::string pretty(Rank r);

// USI形式でRankを出力する
static std::ostream &operator<<(std::ostream &os, Rank r) {
  os << (char)('a' + r);
  return os;
}

// --------------------
//        升目
// --------------------

// 盤上の升目に対応する定数。
// 盤上右上(１一が0)、左下(９九)が80
// 方角を表現するときにマイナスの値を使うので符号型である必要がある。
enum Square : int32_t {
  // 以下、盤面の右上から左下までの定数。
  // これを定義していなくとも問題ないのだが、デバッガでSquare型を見たときに
  // どの升であるかが表示されることに価値がある。
  SQ_11,
  SQ_12,
  SQ_13,
  SQ_14,
  SQ_15,
  SQ_21,
  SQ_22,
  SQ_23,
  SQ_24,
  SQ_25,
  SQ_31,
  SQ_32,
  SQ_33,
  SQ_34,
  SQ_35,
  SQ_41,
  SQ_42,
  SQ_43,
  SQ_44,
  SQ_45,
  SQ_51,
  SQ_52,
  SQ_53,
  SQ_54,
  SQ_55,

  // ゼロと末尾
  SQ_ZERO = 0,
  SQ_NB = 25,

  // 方角に関する定数。StockfishだとNORTH=北=盤面の下を意味するようだが、
  // わかりにくいのでやねうら王ではストレートな命名に変更する。
  SQ_D = +1, // 下(Down)
  SQ_R = -5, // 右(Right)
  SQ_U = -1, // 上(Up)
  SQ_L = +5, // 左(Left)

  // 斜めの方角などを意味する定数。
  SQ_RU = int(SQ_U) + int(SQ_R),   // 右上(Right Up)
  SQ_RD = int(SQ_D) + int(SQ_R),   // 右下(Right Down)
  SQ_LU = int(SQ_U) + int(SQ_L),   // 左上(Left Up)
  SQ_LD = int(SQ_D) + int(SQ_L),   // 左下(Left Down)
  SQ_RUU = int(SQ_RU) + int(SQ_U), // 右上上
  SQ_LUU = int(SQ_LU) + int(SQ_U), // 左上上
  SQ_RDD = int(SQ_RD) + int(SQ_D), // 右下下
  SQ_LDD = int(SQ_LD) + int(SQ_D), // 左下下
};

// sqが盤面の内側を指しているかを判定する。assert()などで使う用。
// 駒は駒落ちのときにSQ_NBに移動するので、値としてSQ_NBは許容する。
constexpr bool is_ok(Square sq) { return SQ_ZERO <= sq && sq <= SQ_NB; }

// sqが盤面の内側を指しているかを判定する。assert()などで使う用。玉は盤上にないときにSQ_NBを取るのでこの関数が必要。
constexpr bool is_ok_plus1(Square sq) { return SQ_ZERO <= sq && sq < SQ_NB; }

// 与えられたSquareに対応する筋を返すテーブル。file_of()で用いる。
constexpr File SquareToFile[SQ_NB] = {
    FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_2, FILE_2, FILE_2, FILE_2,
    FILE_2, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_4, FILE_4, FILE_4,
    FILE_4, FILE_4, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5,
};

// 与えられたSquareに対応する筋を返す。
// →　行数は長くなるが速度面においてテーブルを用いる。
constexpr File file_of(Square sq) {
  /* return (File)(sq / 9); */ /*ASSERT_LV2(is_ok(sq));*/
  return SquareToFile[sq];
}

// 与えられたSquareに対応する段を返すテーブル。rank_of()で用いる。
constexpr Rank SquareToRank[SQ_NB] = {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_1, RANK_2, RANK_3, RANK_4,
    RANK_5, RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_1, RANK_2, RANK_3,
    RANK_4, RANK_5, RANK_1, RANK_2, RANK_3, RANK_4, RANK_5,
};

// 与えられたSquareに対応する段を返す。
// →　行数は長くなるが速度面においてテーブルを用いる。
constexpr Rank rank_of(Square sq) {
  /* return (Rank)(sq % 9); */ /*ASSERT_LV2(is_ok(sq));*/
  return SquareToRank[sq];
}

// 筋(File)と段(Rank)から、それに対応する升(Square)を返す。
constexpr Square operator|(File f, Rank r) {
  Square sq = (Square)(f * 5 + r); /* ASSERT_LV2(is_ok(sq));*/
  return sq;
}

// ２つの升のfileの差、rankの差のうち大きいほうの距離を返す。sq1,sq2のどちらかが盤外ならINT_MAXが返る。
constexpr int dist(Square sq1, Square sq2) {
  return (!is_ok(sq1) || !is_ok(sq2))
             ? INT_MAX
             : std::max(abs(file_of(sq1) - file_of(sq2)),
                        abs(rank_of(sq1) - rank_of(sq2)));
}

// 移動元、もしくは移動先の升sqを与えたときに、そこが成れるかどうかを判定する。
constexpr bool canPromote(const Color c, const Square fromOrTo) {
  ASSERT_LV2(is_ok(fromOrTo));
  return canPromote(c, rank_of(fromOrTo));
}

// 移動元と移動先の升を与えて、成れるかどうかを判定する。
// (移動元か移動先かのどちらかが敵陣であれば成れる)
constexpr bool canPromote(const Color c, const Square from, const Square to) {
  return canPromote(c, from) || canPromote(c, to);
}

// Squareを綺麗に出力する(USI形式ではない)
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。例 → ８八
// "PRETTY_JP"をdefineしていなければ、数字のみの表示になる。例 → 88
static std::string pretty(Square sq) {
  return pretty(file_of(sq)) + pretty(rank_of(sq));
}

// USI形式でSquareを出力する
static std::ostream &operator<<(std::ostream &os, Square sq) {
  os << file_of(sq) << rank_of(sq);
  return os;
}

// --------------------
//   壁つきの升表現
// --------------------

// This trick is invented by yaneurao in 2016.

// 長い利きを更新するときにある升からある方向に駒にぶつかるまでずっと利きを更新していきたいことがあるが、
// sqの升が盤外であるかどうかを判定する簡単な方法がない。そこで、Squareの表現を拡張して盤外であることを検出
// できるようにする。

// bit 0..7   : Squareと同じ意味
// bit 8      : Squareからのborrow用に1にしておく
// bit 9..12  :
// いまの升から盤外まで何升右に升があるか(ここがマイナスになるとborrowでbit13が1になる)
// bit 13..16 : いまの升から盤外まで何升上に(略
// bit 17..20 : いまの升から盤外まで何升下に(略
// bit 21..24 : いまの升から盤外まで何升左に(略
enum SquareWithWall : int32_t {
  // 相対移動するときの差分値
  SQWW_R = SQ_R - (1 << 9) + (1 << 21),
  SQWW_U = SQ_U - (1 << 13) + (1 << 17),
  SQWW_D = -int(SQWW_U),
  SQWW_L = -int(SQWW_R),
  SQWW_RU = int(SQWW_R) + int(SQWW_U),
  SQWW_RD = int(SQWW_R) + int(SQWW_D),
  SQWW_LU = int(SQWW_L) + int(SQWW_U),
  SQWW_LD = int(SQWW_L) + int(SQWW_D),

  // SQ_11の地点に対応する値(他の升はこれ相対で事前に求めテーブルに格納)
  SQWW_11 = SQ_11 | (1 << 8) /* bit8 is 1 */ | (0 << 9) /*右に0升*/ |
            (0 << 13) /*上に0升*/ | (4 << 17) /*下に4升*/ |
            (4 << 21) /*左に4升*/,

  // SQWW_RIGHTなどを足して行ったときに盤外に行ったときのborrow bitの集合
  SQWW_BORROW_MASK = (1 << 12) | (1 << 16) | (1 << 20) | (1 << 24),
};

// 型変換。下位8bit == Square
constexpr Square sqww_to_sq(SquareWithWall sqww) { return Square(sqww & 0xff); }

extern SquareWithWall sqww_table[SQ_NB];

// 型変換。Square型から。
static SquareWithWall to_sqww(Square sq) { return sqww_table[sq]; }

// 盤内か。壁(盤外)だとfalseになる。
constexpr bool is_ok(SquareWithWall sqww) {
  return (sqww & SQWW_BORROW_MASK) == 0;
}

// 単にSQの升を出力する。
static std::ostream &operator<<(std::ostream &os, SquareWithWall sqww) {
  os << sqww_to_sq(sqww);
  return os;
}

// --------------------
//        方角
// --------------------

// Long Effect
// Libraryの一部。これは8近傍、24近傍の利きを直列化したり方角を求めたりするライブラリ。
namespace Effect8 {
// 方角を表す。遠方駒の利きや、玉から見た方角を表すのに用いる。
// bit0..右上、bit1..右、bit2..右下、bit3..上、bit4..下、bit5..左上、bit6..左、bit7..左下
// 同時に複数のbitが1であることがありうる。
enum Directions : uint8_t {
  DIRECTIONS_ZERO = 0,
  DIRECTIONS_RU = 1,
  DIRECTIONS_R = 2,
  DIRECTIONS_RD = 4,
  DIRECTIONS_U = 8,
  DIRECTIONS_D = 16,
  DIRECTIONS_LU = 32,
  DIRECTIONS_L = 64,
  DIRECTIONS_LD = 128,
  DIRECTIONS_CROSS = DIRECTIONS_U | DIRECTIONS_D | DIRECTIONS_R | DIRECTIONS_L,
  DIRECTIONS_DIAG =
      DIRECTIONS_RU | DIRECTIONS_RD | DIRECTIONS_LU | DIRECTIONS_LD,
};

// sq1にとってsq2がどのdirectionにあるか。
// "Direction"ではなく"Directions"を返したほうが、縦横十字方向や、斜め方向の位置関係にある場合、
// DIRECTIONS_CROSSやDIRECTIONS_DIAGのような定数が使えて便利。
extern Directions direc_table[SQ_NB][SQ_NB];
static Directions directions_of(Square sq1, Square sq2) {
  return direc_table[sq1][sq2];
}

// Directionsをpopしたもの。複数の方角を同時に表すことはない。
// おまけで桂馬の移動も追加しておく。
enum Direct {
  DIRECT_RU,
  DIRECT_R,
  DIRECT_RD,
  DIRECT_U,
  DIRECT_D,
  DIRECT_LU,
  DIRECT_L,
  DIRECT_LD,
  DIRECT_NB,
  DIRECT_ZERO = 0
};

// Directionsに相当するものを引数に渡して1つ方角を取り出す。
static Direct pop_directions(Directions &d) { return (Direct)pop_lsb(d); }

// ある方角の反対の方角(180度回転させた方角)を得る。
constexpr Direct operator~(Direct d) {
  // Directの定数値を変更したら、この関数はうまく動作しない。
  static_assert(Effect8::DIRECT_R == 1, "");
  static_assert(Effect8::DIRECT_L == 6, "");
  // ASSERT_LV3(d < DIRECT_NB);
  return Direct(7 - d);
}

// DirectからDirectionsへの逆変換
constexpr Directions to_directions(Direct d) { return Directions(1 << d); }

constexpr bool is_ok(Direct d) { return DIRECT_ZERO <= d && d < DIRECT_NB; }

// DirectをSquareWithWall型の差分値で表現したもの。
constexpr SquareWithWall DirectToDeltaWW_[DIRECT_NB] = {
    SQWW_RU, SQWW_R, SQWW_RD, SQWW_U, SQWW_D, SQWW_LU, SQWW_L, SQWW_LD,
};
constexpr SquareWithWall DirectToDeltaWW(Direct d) { /* ASSERT_LV3(is_ok(d)); */
  return DirectToDeltaWW_[d];
}
} // namespace Effect8

// 与えられた3升が縦横斜めの1直線上にあるか。駒を移動させたときに開き王手になるかどうかを判定するのに使う。
// 例) 王がsq1,
// pinされている駒がsq2にあるときに、pinされている駒をsq3に移動させたときにaligned(sq1,sq2,sq3)であれば、
//  pinされている方向に沿った移動なので開き王手にはならないと判定できる。
// ただし玉はsq3として、sq1,sq2は同じ側にいるものとする。(玉を挟んでの一直線は一直線とはみなさない)
static bool aligned(Square sq1, Square sq2, Square sq3 /* is ksq */) {
  auto d1 = Effect8::directions_of(sq1, sq3);
  return d1 ? d1 == Effect8::directions_of(sq2, sq3) : false;
}

// --------------------
//     探索深さ
// --------------------

// 通常探索時の最大探索深さ
constexpr int MAX_PLY = 127;

// --------------------
//        評価値
// --------------------

// 置換表に格納するときにあまりbit数が多いともったいないので値自体は16bitで収まる範囲で。
enum Value : int32_t {
  VALUE_ZERO = 0,

  // 0手詰めのスコア(rootで詰んでいるときのscore)
  // 例えば、3手詰めならこの値より3少ない。
  VALUE_MATE = 32000,

  // Valueの取りうる最大値(最小値はこの符号を反転させた値)
  VALUE_INFINITE = 32001,

  // 無効な値
  VALUE_NONE = 32002,

  VALUE_MATE_IN_MAX_PLY =
      int(VALUE_MATE) - MAX_PLY, // MAX_PLYでの詰みのときのスコア。
  VALUE_MATED_IN_MAX_PLY =
      -int(VALUE_MATE_IN_MAX_PLY), // MAX_PLYで詰まされるときのスコア。

  // 千日手による優等局面への突入したときのスコア
  VALUE_SUPERIOR = 28000,

  // 評価関数
};

// ply手で詰ませるときのスコア
constexpr Value mate_in(int ply) { return (Value)(VALUE_MATE - ply); }

// ply手で詰まされるときのスコア
constexpr Value mated_in(int ply) { return (Value)(-VALUE_MATE + ply); }

// --------------------
//        駒
// --------------------

// USIプロトコルでやりとりするときの駒の表現
extern const char *USI_PIECE;

enum Piece : uint32_t {
  // 金の順番を飛の後ろにしておく
  // KINGを8にしておく
  // 成りを求めるときは pc |= 8; で求まる
  NO_PIECE = 0,
  PAWN = 1,        // 歩
  SILVER = 4,      // 銀
  BISHOP = 5,      // 角
  ROOK = 6,        // 飛
  GOLD = 7,        // 金
  KING = 8,        // 玉
  PRO_PAWN = 9,    // と
  PRO_SILVER = 12, // 成銀
  HORSE = 13,      // 馬
  DRAGON = 14,     // 龍
  QUEEN = 15,      // 未使用

  // 先後の区別のある駒
  // B(先手), W(後手)
  B_PAWN = 1,
  B_SILVER = 4,
  B_BISHOP,
  B_ROOK,
  B_GOLD,
  B_KING,
  B_PRO_PAWN = 9,
  B_PRO_SILVER = 12,
  B_HORSE,
  B_DRAGON,
  B_QUEEN,
  W_PAWN = 17,
  W_SILVER = 20,
  W_BISHOP,
  W_ROOK,
  W_GOLD,
  W_KING,
  W_PRO_PAWN = 25,
  W_PRO_SILVER = 28,
  W_HORSE,
  W_DRAGON,
  W_QUEEN,
  PIECE_NB, // 終端
  PIECE_ZERO = 0,

  // --- 特殊な定数

  PIECE_PROMOTE = 8, // 成り駒と非成り駒との差(この定数を足すと成り駒になる)
  PIECE_TYPE_NB = 16, // 駒種の数。(成りを含める)
  PIECE_WHITE = 16, // これを先手の駒に加算すると後手の駒になる。
  PIECE_RAW_NB = 8, // 非成駒の終端

  PIECE_HAND_ZERO = PAWN, // 手駒の開始位置
  PIECE_HAND_NB = KING,   // 手駒になる駒種の最大+1

  // --- Position::pieces()で用いる定数。空いてるところを順番に用いる。
  ALL_PIECES = 0, // 駒がある升を示すBitboardが返る。
  GOLDS = QUEEN,  // 金と同じ移動特性を持つ駒のBitboardが返る。
  HDK,            // H=Horse,D=Dragon,K=Kingの合体したBitboardが返る。
  BISHOP_HORSE,   // BISHOP,HORSEを合成したBitboardが返る。
  ROOK_DRAGON,    // ROOK,DRAGONを合成したBitboardが返る。
  SILVER_HDK,     // SILVER,HDKを合成したBitboardが返る。
  GOLDS_HDK,      // GOLDS,HDKを合成したBitboardが返る。
  PIECE_BB_NB,    // デリミタ

  // 指し手生成(GeneratePieceMove =
  // GPM)でtemplateの引数として使うマーカー的な値。変更する可能性があるのでユーザーは使わないでください。
  // 連続的な値にしておくことで、テーブルジャンプしやすくする。
  GPM_BR = 16,   // Bishop Rook
  GPM_GBR = 17,  // Gold Bishop Rook
  GPM_GHD = 18,  // Gold Horse Dragon
  GPM_GHDK = 19, // Gold Horse Dragon King
};

// USIプロトコルで駒を表す文字列を返す。
// 駒打ちの駒なら先頭に"D"。
static std::string usi_piece(Piece pc) {
  return std::string((pc & 32) ? "D" : "") +
         std::string(USI_PIECE).substr((pc & 31) * 2, 2);
}

// 駒に対して、それが先後、どちらの手番の駒であるかを返す。
constexpr Color color_of(Piece pc) {
  //	return (pc & PIECE_WHITE) ? WHITE : BLACK;
  static_assert(PIECE_WHITE == 16 && WHITE == 1 && BLACK == 0, "");
  return (Color)((pc & PIECE_WHITE) >> 4);
}

// 後手の歩→先手の歩のように、後手という属性を取り払った駒種を返す
constexpr Piece type_of(Piece pc) { return (Piece)(pc & 15); }

// 成ってない駒を返す。後手という属性も消去する。
// 例) 成銀→銀 , 後手の馬→先手の角
// ただし、pc == KINGでの呼び出しはNO_PIECEが返るものとする。
constexpr Piece raw_type_of(Piece pc) { return (Piece)(pc & 7); }

// pcとして先手の駒を渡し、cが後手なら後手の駒を返す。cが先手なら先手の駒のまま。pcとしてNO_PIECEは渡してはならない。
constexpr Piece
make_piece(Color c,
           Piece pt) { /*ASSERT_LV3(color_of(pt) == BLACK && pt!=NO_PIECE); */
  return (Piece)((c << 4) + pt);
}

// pcが遠方駒であるかを判定する。BISHOP(5),ROOK(6),HORSE(13),DRAGON(14)
constexpr bool has_long_effect(Piece pc) { return ((pc + 1) & 6) == 6; }

// Pieceの整合性の検査。assert用。
constexpr bool is_ok(Piece pc) { return NO_PIECE <= pc && pc < PIECE_NB; }

// Pieceを綺麗に出力する(USI形式ではない)
// 先手の駒は大文字、後手の駒は小文字、成り駒は先頭に+がつく。盤面表示に使う。
// "PRETTY_JP"をdefineしていれば、日本語文字での表示になる。
std::string pretty(Piece pc);

// ↑のpretty()だと先手の駒を表示したときに先頭にスペースが入るので、それが嫌な場合はこちらを用いる。
static std::string pretty2(Piece pc) {
  ASSERT_LV1(color_of(pc) == BLACK);
  auto s = pretty(pc);
  return s.substr(1, s.length() - 1);
}

// USIで、盤上の駒を表現する文字列
// ※　歩Pawn 銀Silver 角Bishop 飛Rook 金Gold 王King
extern std::string PieceToCharBW;

// PieceをUSI形式で表示する。
std::ostream &operator<<(std::ostream &os, Piece pc);

// --------------------
//       指し手
// --------------------

// 指し手 bit0..6 = 移動先のSquare、bit7..13 =
// 移動元のSquare(駒打ちのときは駒種)、bit14 = 駒打ちか、bit15 = 成りか
// 32bit形式の指し手の場合、上位16bitには、この指し手によってto(移動後の升)に来る駒。
enum Move : uint32_t {

  MOVE_NONE = 0, // 無効な移動

  MOVE_NULL =
      (1 << 7) +
      1, // NULL
         // MOVEを意味する指し手。Square(1)からSquare(1)への移動は存在しないのでここを特殊な記号として使う。
  MOVE_RESIGN =
      (2 << 7) +
      2, // << で出力したときに"resign"と表示する投了を意味する指し手。

  MOVE_DROP = 1 << 14,    // 駒打ちフラグ
  MOVE_PROMOTE = 1 << 15, // 駒成りフラグ
};

// 指し手の移動元の升を返す。
constexpr Square move_from(Move m) { return Square((m >> 7) & 0x7f); }

// 指し手の移動先の升を返す。
constexpr Square move_to(Move m) { return Square(m & 0x7f); }

// 指し手が駒打ちか？
constexpr bool is_drop(Move m) { return (m & MOVE_DROP) != 0; }

// fromとtoをシリアライズする。駒打ちのときのfromは普通の移動の指し手とは異なる。
// この関数は、0 〜 ((SQ_NB+7) * SQ_NB - 1)までの値が返る。
constexpr int from_to(Move m) {
  return (int)(move_from(m) + (is_drop(m) ? (SQ_NB - 1) : 0)) * (int)SQ_NB +
         (int)move_to(m);
}

// 指し手が成りか？
constexpr bool is_promote(Move m) { return (m & MOVE_PROMOTE) != 0; }

// 駒打ち(is_drop()==true)のときの打った駒
// 先後の区別なし。PAWN〜ROOKまでの値が返る。
constexpr Piece move_dropped_piece(Move m) { return (Piece)((m >> 7) & 0x7f); }

// fromからtoに移動する指し手を生成して返す(16bitの指し手)
constexpr Move make_move(Square from, Square to) {
  return (Move)(to + (from << 7));
}

// fromからtoに移動する、成りの指し手を生成して返す(16bitの指し手)
constexpr Move make_move_promote(Square from, Square to) {
  return (Move)(to + (from << 7) + MOVE_PROMOTE);
}

// Pieceをtoに打つ指し手を生成して返す(16bitの指し手)
constexpr Move make_move_drop(Piece pt, Square to) {
  return (Move)(to + (pt << 7) + MOVE_DROP);
}

// 指し手がおかしくないかをテストする
// ただし、盤面のことは考慮していない。MOVE_NULLとMOVE_NONEであるとfalseが返る。
// これら２つの定数は、移動元と移動先が等しい値になっている。このテストだけをする。
// MOVE_WIN(宣言勝ちの指し手は)は、falseが返る。
constexpr bool is_ok(Move m) {
  // return move_from(m)!=move_to(m);
  // とやりたいところだが、駒打ちでfromのbitを使ってしまっているのでそれだとまずい。
  // 駒打ちのbitも考慮に入れるために次のように書く。
  return (m >> 7) != (m & 0x7f);
}

// 見た目に、わかりやすい形式で表示する
std::string pretty(Move m);

// 移動させた駒がわかっているときに指し手をわかりやすい表示形式で表示する。
std::string pretty(Move m, Piece movedPieceType);

// USI形式の文字列にする。
// USI::move(Move m)と同等。互換性のために残されている。
std::string to_usi_string(Move m);

// USI形式で指し手を表示する
static std::ostream &operator<<(std::ostream &os, Move m) {
  os << to_usi_string(m);
  return os;
}

// --------------------
//   拡張された指し手
// --------------------

// 指し手とオーダリングのためのスコアがペアになっている構造体。
// オーダリングのときにスコアで並べ替えしたいが、一つになっているほうが並び替えがしやすいのでこうしてある。
struct ExtMove {

  Move move; // 指し手(32bit)
  int value; // 指し手オーダリング(並び替え)のときのスコア(符号つき32bit)

  // Move型とは暗黙で変換できていい。

  operator Move() const { return move; }
  void operator=(Move m) { move = m; }

  // 望まない暗黙のMoveへの変換を禁止するために
  // 曖昧な変換でコンパイルエラーになるようにしておく。
  // cf. Fix involuntary conversions of ExtMove to Move :
  // https://github.com/official-stockfish/Stockfish/commit/d482e3a8905ee194bda3f67a21dda5132c21f30b
  operator float() const = delete;
};

// ExtMoveの並べ替えを行なうので比較オペレーターを定義しておく。
constexpr bool operator<(const ExtMove &first, const ExtMove &second) {
  return first.value < second.value;
}

static std::ostream &operator<<(std::ostream &os, ExtMove m) {
  os << m.move << '(' << m.value << ')';
  return os;
}

// --------------------
//       手駒
// --------------------

// 手駒
// 歩、銀、角、飛、金を4bitずつで持つ。こうすると16進数表示したときに綺麗に表示される。
enum Hand : uint32_t {
  HAND_ZERO = 0,
};

// 手駒のbit位置
constexpr int PIECE_BITS[PIECE_HAND_NB] = {0,         0 /*歩*/,  0 /*香*/,
                                           0 /*桂*/,  12 /*銀*/, 16 /*角*/,
                                           20 /*飛*/, 24 /*金*/};

// Piece(歩,香,桂,銀,金,角,飛)を手駒に変換するテーブル
constexpr Hand PIECE_TO_HAND[PIECE_HAND_NB] = {(Hand)0,
                                               (Hand)(1 << PIECE_BITS[PAWN]),
                                               (Hand)0,
                                               (Hand)0,
                                               (Hand)(1 << PIECE_BITS[SILVER]),
                                               (Hand)(1 << PIECE_BITS[BISHOP]),
                                               (Hand)(1 << PIECE_BITS[ROOK]),
                                               (Hand)(1 << PIECE_BITS[GOLD])};

// その持ち駒を表現するのに必要なbit数のmask(例えば3bitなら2の3乗-1で7)
constexpr int PIECE_BIT_MASK[PIECE_HAND_NB] = {0, 3, 0, 0, 3, 3, 3, 3};

constexpr u32 PIECE_BIT_MASK2[PIECE_HAND_NB] = {
    0,
    PIECE_BIT_MASK[PAWN] << PIECE_BITS[PAWN],
    0,
    0,
    PIECE_BIT_MASK[SILVER] << PIECE_BITS[SILVER],
    PIECE_BIT_MASK[BISHOP] << PIECE_BITS[BISHOP],
    PIECE_BIT_MASK[ROOK] << PIECE_BITS[ROOK],
    PIECE_BIT_MASK[GOLD] << PIECE_BITS[GOLD]};

// 駒の枚数が格納されているbitが1となっているMASK。(駒種を得るときに使う)
constexpr u32 HAND_BIT_MASK = PIECE_BIT_MASK2[PAWN] | PIECE_BIT_MASK2[SILVER] |
                              PIECE_BIT_MASK2[BISHOP] | PIECE_BIT_MASK2[ROOK] |
                              PIECE_BIT_MASK2[GOLD];

// 余らせてあるbitの集合。
constexpr u32 HAND_BORROW_MASK = (HAND_BIT_MASK << 1) & ~HAND_BIT_MASK;

// 手駒pcの枚数を返す。
// このASSERTを有効化するとconstexprにならないのでコメントアウトしておく。
// 返し値は引き算するときに符号を意識したくないのでintにしておく。
constexpr int hand_count(
    Hand hand,
    Piece pr) { /* ASSERT_LV2(PIECE_HAND_ZERO <= pr && pr < PIECE_HAND_NB); */
  return (int)(hand >> PIECE_BITS[pr]) & PIECE_BIT_MASK[pr];
}

// 手駒pcを持っているかどうかを返す。
constexpr u32 hand_exists(
    Hand hand,
    Piece pr) { /* ASSERT_LV2(PIECE_HAND_ZERO <= pr && pr < PIECE_HAND_NB); */
  return hand & PIECE_BIT_MASK2[pr];
}

// 歩以外の手駒を持っているか
constexpr u32 hand_except_pawn_exists(Hand hand) {
  return hand & (HAND_BIT_MASK ^ PIECE_BIT_MASK2[PAWN]);
}

// 手駒にpcをc枚加える
constexpr void add_hand(Hand &hand, Piece pr, int c = 1) {
  hand = (Hand)(hand + PIECE_TO_HAND[pr] * c);
}

// 手駒からpcをc枚減ずる
constexpr void sub_hand(Hand &hand, Piece pr, int c = 1) {
  hand = (Hand)(hand - PIECE_TO_HAND[pr] * c);
}

// 手駒h1のほうがh2より優れているか。(すべての種類の手駒がh2のそれ以上ある)
// 優等局面の判定のとき、局面のhash key(StateInfo::key()
// )が一致していなくて、盤面のhash key(StateInfo::board_key() )が
// 一致しているときに手駒の比較に用いるので、手駒がequalというケースは前提により除外されているから、この関数を以ってsuperiorであるという判定が出来る。
constexpr bool hand_is_equal_or_superior(Hand h1, Hand h2) {
  return ((h1 - h2) & HAND_BORROW_MASK) == 0;
}

// 手駒を表示する(USI形式ではない) デバッグ用
std::ostream &operator<<(std::ostream &os, Hand hand);

// --------------------
//    指し手生成器
// --------------------

// 局面の合法手の最大数
constexpr int MAX_MOVES = 600;

// 生成する指し手の種類
enum MOVE_GEN_TYPE {
  // LEGAL/LEGAL_ALL以外は自殺手が含まれることがある(pseudo-legal)ので、do_moveの前にPosition::legal()でのチェックが必要。

  NON_CAPTURES, // 駒を取らない指し手
  CAPTURES,     // 駒を取る指し手

  CAPTURES_PRO_PLUS, // CAPTURES + 価値のかなりあると思われる成り(歩だけ)
  NON_CAPTURES_PRO_MINUS, // NON_CAPTURES -
                          // 価値のかなりあると思われる成り(歩だけ)

  CAPTURES_PRO_PLUS_ALL,      // CAPTURES_PRO_PLUS + 角・飛の不成を含む
  NON_CAPTURES_PRO_MINUS_ALL, // NON_CAPTURES_PRO_MINUS + 角・飛の不成を含む

  EVASIONS, // 王手の回避(指し手生成元で王手されている局面であることがわかっているときはこちらを呼び出す)
  EVASIONS_ALL, // EVASIONS + 歩の不成なども含む。

  NON_EVASIONS, // 王手の回避ではない手(指し手生成元で王手されていない局面であることがわかっているときのすべての指し手)
  NON_EVASIONS_ALL, // NON_EVASIONS + 歩の不成などを含む。

  // 以下の2つは、pos.legalを内部的に呼び出すので生成するのに時間が少しかかる。棋譜の読み込み時などにしか使わない。
  LEGAL, // 合法手すべて。ただし、角・飛の不成は生成しない。
  LEGAL_ALL, // 合法手すべて

  CHECKS,     // 王手となる指し手(歩の不成などは含まない)
  CHECKS_ALL, // 王手となる指し手(歩の不成なども含む)

  QUIET_CHECKS, // 王手となる指し手(歩の不成などは含まない)で、CAPTURESの指し手は含まない指し手
  QUIET_CHECKS_ALL, // 王手となる指し手(歩の不成なども含む)でCAPTURESの指し手は含まない指し手

  RECAPTURES, // 指定升への移動の指し手のみを生成する。(歩の不成などは含まない)
  RECAPTURES_ALL, // 指定升への移動の指し手のみを生成する。(歩の不成なども含む)
};

class Position; // 前方宣言

// 指し手を生成器本体
// gen_typeとして生成する指し手の種類をシてする。gen_allをfalseにすると歩の不成、香の8段目の不成は生成しない。通常探索中はそれでいいはず。
// mlist : 指し手を返して欲しい指し手生成バッファのアドレス
// 返し値 : 生成した指し手の終端
struct CheckInfo;
template <MOVE_GEN_TYPE gen_type>
ExtMove *generateMoves(const Position &pos, ExtMove *mlist);
template <MOVE_GEN_TYPE gen_type>
ExtMove *generateMoves(const Position &pos, ExtMove *mlist,
                       Square recapSq); // RECAPTURES,RECAPTURES_ALL専用

// MoveGeneratorのwrapper。範囲forで回すときに便利。
template <MOVE_GEN_TYPE GenType> struct MoveList {
  // 局面をコンストラクタの引数に渡して使う。すると指し手が生成され、lastが初期化されるので、
  // このclassのbegin(),end()が正常な値を返すようになる。
  // lastは内部のバッファを指しているので、このクラスのコピーは不可。

  explicit MoveList(const Position &pos)
      : last(generateMoves<GenType>(pos, mlist)) {}

  explicit MoveList(const Position& pos, Square sq)
   : last(generateMoves<GenType>(pos, mlist, sq)) {
    static_assert(GenType == RECAPTURES || GenType == RECAPTURES_ALL);
  }
  
  // 内部的に持っている指し手生成バッファの先頭
  const ExtMove *begin() const { return mlist; }

  // 生成された指し手の末尾のひとつ先
  const ExtMove *end() const { return last; }

  // 生成された指し手の数
  size_t size() const { return last - mlist; }

  // i番目の要素を返す
  const ExtMove at(size_t i) const {
    ASSERT_LV3(0 <= i && i < size());
    return begin()[i];
  }

private:
  ExtMove mlist[MAX_MOVES], *last;
};

// 局面のハッシュキー
// 盤面(盤上の駒 + 手駒)に対して、Zobrist Hashでそれに対応する値を計算する。
typedef uint64_t Key;

// --------------------
//        探索
// --------------------

// 千日手の状態
enum RepetitionState {
  REPETITION_NONE, // 千日手ではない
  REPETITION_WIN,  // 連続王手の千日手による勝ち
  REPETITION_LOSE, // 連続王手の千日手による負け
  REPETITION_DRAW, // 連続王手ではない普通の千日手
  REPETITION_SUPERIOR, // 優等局面(盤上の駒が同じで手駒が相手より優れている)
  REPETITION_INFERIOR, // 劣等局面(盤上の駒が同じで手駒が相手より優れている)
  REPETITION_NB,
};

constexpr bool is_ok(RepetitionState rs) {
  return REPETITION_NONE <= rs && rs < REPETITION_NB;
}

// RepetitionStateを文字列化する。PVの出力のときにUSI拡張として出力するのに用いる。
extern std::string to_usi_string(RepetitionState rs);

// RepetitionStateを文字列化して出力する。PVの出力のときにUSI拡張として出力するのに用いる。
std::ostream &operator<<(std::ostream &os, RepetitionState rs);

// 引き分け時のスコア
extern Value drawValueTable[REPETITION_NB][COLOR_NB];
static Value draw_value(RepetitionState rs,
                        Color c) { /* ASSERT_LV3(is_ok(rs)); */
  return drawValueTable[rs][c];
}

// --------------------
//  operators and macros
// --------------------

#include "extra/macros.h"

#endif
