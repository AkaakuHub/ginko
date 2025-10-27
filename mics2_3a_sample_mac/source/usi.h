#ifndef _USI_H_
#define _USI_H_

#include "types.h"

class Position;

namespace USI {
// USIメッセージ応答部(起動時に、各種初期化のあとに呼び出される)
void loop(int argc, char *argv[]);

// USIプロトコルの形式でValue型を出力する。
// 歩が100になるように正規化するので、operator <<(Value)をこういう仕様にすると
// 実際の値と異なる表示になりデバッグがしにくくなるから、そうはしていない。
std::string value(Value v);

// 指し手をUSI文字列に変換する。
std::string move(Move m /*, bool chess960*/);

// pv(読み筋)をUSIプロトコルに基いて出力する。
// depth : 反復深化のiteration深さ。
std::string pv(const Position &pos, int depth);

// 局面posとUSIプロトコルによる指し手を与えて
// もし可能なら等価で合法な指し手を返す。(合法でないときはMOVE_NONEを返す。"resign"に対してはMOVE_RESIGNを返す。)
// Stockfishでは第二引数にconstがついていないが、これはつけておく。
Move to_move(const Position &pos, const std::string &str);
} // namespace USI

// 外部からis_ready_cmd()を呼び出す。
// 局面は初期化されない。
void is_ready();

#endif
