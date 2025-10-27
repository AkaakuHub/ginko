#include "usi.h"

// ----------------------------------------
//    const
// ----------------------------------------

const char *USI_PIECE =
    ". P L N S B R G K +P+L+N+S+B+R+G+.p l n s b r g k +p+l+n+s+b+r+g+k";

// ----------------------------------------
//    tables
// ----------------------------------------

namespace Effect8 {
Directions direc_table[SQ_NB][SQ_NB];
}

std::string PieceToCharBW(" PLNSBRGK        plnsbrgk");

Piece BoardPieces[20] = {B_PAWN, B_SILVER,   B_BISHOP,     B_ROOK,  B_GOLD,
                         B_KING, B_PRO_PAWN, B_PRO_SILVER, B_HORSE, B_DRAGON,
                         W_PAWN, W_SILVER,   W_BISHOP,     W_ROOK,  W_GOLD,
                         W_KING, W_PRO_PAWN, W_PRO_SILVER, W_HORSE, W_DRAGON};
Piece HandPieces[5] = {PAWN, SILVER, BISHOP, ROOK, GOLD};

// ----------------------------------------
// operator<<(std::ostream& os,...)とpretty()
// ----------------------------------------

std::string pretty(File f) {
  return pretty_jp ? std::string("１２３４５").substr((size_t)f * 2, 2)
                   : std::to_string((size_t)f + 1);
}
std::string pretty(Rank r) {
  return pretty_jp ? std::string("一二三四五").substr((size_t)r * 2, 2)
                   : std::to_string((size_t)r + 1);
}

std::string pretty(Move m) {
  if (is_drop(m))
    return (pretty(move_to(m)) + pretty2(Piece(move_from(m))) +
            (pretty_jp ? "打" : "*"));
  else
    return pretty(move_from(m)) + pretty(move_to(m)) +
           (is_promote(m) ? (pretty_jp ? "成" : "+") : "");
}

std::string pretty(Move m, Piece movedPieceType) {
  if (is_drop(m))
    return (pretty(move_to(m)) + pretty2(movedPieceType) +
            (pretty_jp ? "打" : "*"));
  else
    return pretty(move_to(m)) + pretty2(movedPieceType) +
           (is_promote(m) ? (pretty_jp ? "成" : "+") : "") + "[" +
           pretty(move_from(m)) + "]";
}

std::string to_usi_string(Move m) { return USI::move(m); }

std::ostream &operator<<(std::ostream &os, Color c) {
  os << ((c == BLACK) ? (pretty_jp ? "先手" : "BLACK")
                      : (pretty_jp ? "後手" : "WHITE"));
  return os;
}

std::ostream &operator<<(std::ostream &os, Piece pc) {
  auto s = usi_piece(pc);
  if (s[1] == ' ')
    s.resize(1); // 手動trim
  os << s;
  return os;
}

std::string USI_PIECE_KANJI[] = {
    " 口", " 歩", " 香", " 桂", " 銀", " 角", " 飛", " 金", " 玉",
    " と", " 杏", " 圭", " 全", " 馬", " 龍", " 菌", " 王", "^歩",
    "^香", "^桂", "^銀", "^角", "^飛", "^金", "^玉", "^と", "^杏",
    "^圭", "^全", "^馬", "^龍", "^菌", "^王"};

// Pieceを綺麗に出力する(USI形式ではない)
// 先手の駒は大文字、後手の駒は小文字、成り駒は先頭に+がつく。盤面表示に使う。
std::string pretty(Piece pc) {
#if !defined(FONT_COLOR)
  return pretty_jp ? USI_PIECE_KANJI[pc]
                   : std::string(USI_PIECE).substr(pc * 2, 2);
#else
  // 色を変えたほうがわかりやすい。Linuxは簡単だが、MS-DOSは設定が面倒。
  // Linux : https://qiita.com/dojineko/items/49aa30018bb721b0b4a9
  // MS-DOS : https://one-person.hatenablog.jp/entry/2017/02/23/125809

  std::string result;
  if (pc != NO_PIECE)
    result = (color_of(pc) == BLACK) ? "\x1b[32;40;1m" : "\x1b[33;40;1m";
  result += pretty_jp ? USI_PIECE_KANJI[pc]
                      : std::string(USI_PIECE).substr(pc * 2, 2);
  if (pc != NO_PIECE)
    result += "\x1b[39;0m";
  return result;
#endif
}

std::ostream &operator<<(std::ostream &os, Hand hand) {
  for (Piece pr : HandPieces) {
    int c = hand_count(hand, pr);
    // 0枚ではないなら出力。
    if (c != 0) {
      // 1枚なら枚数は出力しない。2枚以上なら枚数を最初に出力
      // PRETTY_JPが指定されているときは、枚数は後ろに表示。
      const std::string cs = (c != 1) ? std::to_string(c) : "";
      std::cout << (pretty_jp ? "" : cs) << pretty(pr) << (pretty_jp ? cs : "");
    }
  }
  return os;
}

// RepetitionStateを文字列化する。PVの出力のときにUSI拡張として出力するのに用いる。
std::string to_usi_string(RepetitionState rs) {
  return ((rs == REPETITION_NONE)
              ? "rep_none"
              : // これはデバッグ用であり、実際には出力はしない。
              (rs == REPETITION_WIN)    ? "rep_win"
          : (rs == REPETITION_LOSE)     ? "rep_lose"
          : (rs == REPETITION_DRAW)     ? "rep_draw"
          : (rs == REPETITION_SUPERIOR) ? "rep_sup"
          : (rs == REPETITION_INFERIOR) ? "rep_inf"
                                        : "");
}

// 拡張USIプロトコルにおいてPVの出力に用いる。
std::ostream &operator<<(std::ostream &os, RepetitionState rs) {
  os << to_usi_string(rs);
  return os;
}

// 引き分け時のスコア(とそのdefault値)
Value drawValueTable[REPETITION_NB][COLOR_NB] = {
    {VALUE_ZERO, VALUE_ZERO},           // REPETITION_NONE
    {VALUE_MATE, VALUE_MATE},           // REPETITION_WIN
    {-VALUE_MATE, -VALUE_MATE},         // REPETITION_LOSE
    {VALUE_ZERO, VALUE_ZERO},           // REPETITION_DRAW
    {VALUE_SUPERIOR, VALUE_SUPERIOR},   // REPETITION_SUPERIOR
    {-VALUE_SUPERIOR, -VALUE_SUPERIOR}, // REPETITION_INFERIOR
};

#if defined(USE_GLOBAL_OPTIONS)
GlobalOptions_ GlobalOptions;
#endif
