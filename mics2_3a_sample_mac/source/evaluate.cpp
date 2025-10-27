#include "evaluate.h"

namespace Eval {
int PieceValue[PIECE_NB] = {
    0,
    PawnValue,
    0,
    0,
    SilverValue,
    BishopValue,
    RookValue,
    GoldValue,
    KingValue,
    ProPawnValue,
    0,
    0,
    ProSilverValue,
    HorseValue,
    DragonValue,
    0,

    0,
    -PawnValue,
    0,
    0,
    -SilverValue,
    -BishopValue,
    -RookValue,
    -GoldValue,
    -KingValue,
    -ProPawnValue,
    0,
    0,
    -ProSilverValue,
    -HorseValue,
    -DragonValue,
    0,
};

Value evaluate(const Position &pos) {
  Value score = VALUE_ZERO;

  // 盤上の駒の評価
  for (Square sq : SQ)
    score += PieceValue[pos.piece_on(sq)];

  // 手駒の評価
  for (Color c : COLOR)
    for (Piece pc : {PAWN, SILVER, BISHOP, ROOK, GOLD}) {
      // 手駒の枚数を取得
      int cnt = hand_count(pos.hand_of(c), pc);
      score += (c == BLACK ? 1 : -1) * Value(cnt * (PieceValue[pc]));
    }
  // 手番側から見た評価値を返す
  return pos.side_to_move() == BLACK ? score : -score;
}

} // namespace Eval
