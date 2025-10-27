#ifndef _SEARCH_H_
#define _SEARCH_H_

#include "misc.h"
#include "position.h"
#include <vector>


namespace Search {
// root(探索開始局面)での指し手として使われる。それぞれのroot moveに対して、
// その指し手で進めたときのscore(評価値)とPVを持っている。(PVはfail
// lowしたときには信用できない)
// scoreはnon-pvの指し手では-VALUE_INFINITEで初期化される。
struct RootMove {
  // pv[0]には、コンストラクタの引数で渡されたmを設定する。
  explicit RootMove(Move m) : pv(1, m) {}

  bool operator==(const Move &m) const { return pv[0] == m; }

  bool operator<(const RootMove &m) const {
    return m.score != score ? m.score < score : m.previousScore < previousScore;
  }

  // 今回のスコア
  Value score = -VALUE_INFINITE;

  // 前回のスコア
  Value previousScore = -VALUE_INFINITE;

  // rootから最大、何手目まで探索したか(選択深さの最大)
  int selDepth = 0;

  // この指し手で進めたときのpv
  std::vector<Move> pv;
};

typedef std::vector<RootMove> RootMoves;

// 探索開始局面で思考対象とする指し手の集合。
extern RootMoves rootMoves;

// 今回のgoコマンドでの探索ノード数。
extern uint64_t Nodes;

// 探索中にこれがtrueになったら探索を即座に終了すること。
extern bool Stop;

// goコマンドでの探索時に用いる、持ち時間設定などが入った構造体
struct LimitsType {
  LimitsType() {
    time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = movetime =
        TimePoint(0);
    depth = perft = infinite = 0;
    nodes = 0;
    byoyomi[WHITE] = byoyomi[BLACK] = TimePoint(0);
  }

  // 時間制御を行うのか。
  // 詰み専用探索、思考時間0、探索深さが指定されている、探索ノードが指定されている、思考時間無制限
  // であるときは、時間制御に意味がないのでやらない。
  bool use_time_management() const {
    return !(movetime | depth | nodes | perft | infinite);
  }

  // time[]   : 残り時間(ms換算で)
  // inc[]    : 1手ごとに増加する時間(フィッシャールール)
  // movetime : 思考時間固定(0以外が指定してあるなら) : 単位は[ms]
  TimePoint time[COLOR_NB], inc[COLOR_NB], movetime;

  // depth    : 探索深さ固定(0以外を指定してあるなら)
  // perft    : perft(performance
  // test)中であるかのフラグ。非0なら、perft時の深さが入る。 infinite :
  // 思考時間無制限かどうかのフラグ。非0なら無制限。
  int depth, perft, infinite;

  // 今回のgoコマンドでの探索ノード数
  int64_t nodes;

  // 秒読み(ms換算で)
  TimePoint byoyomi[COLOR_NB];
};

extern LimitsType Limits;

// 探索部の初期化
void init();

// 探索部のclear
void clear();

// 探索を開始する
void start_thinking(const Position &rootPos, StateListPtr &states,
                    LimitsType limits);

// 探索本体
void search(Position &rootPos);
} // namespace Search

#endif // !SEARCH_H_
