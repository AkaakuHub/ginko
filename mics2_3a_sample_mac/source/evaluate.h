#ifndef _EVALUATE_H_
#define _EVALUATE_H_

#include "position.h"
#include "types.h"

namespace Eval {
// Apery(WCSC26)の駒割り
enum {
  // 駒の評価値
  PawnValue = 100,
  SilverValue = 600,
  GoldValue = 700,
  BishopValue = 900,
  RookValue = 1000,
  ProPawnValue = 400,
  ProSilverValue = 650,
  HorseValue = 1100,
  DragonValue = 1200,
  KingValue = 15000,
};

// 駒の価値のテーブル(後手の駒は負の値)
extern int PieceValue[PIECE_NB];

Value evaluate(const Position &pos);
} // namespace Eval

#endif
