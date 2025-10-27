#include "usi.h"
#include "evaluate.h"
#include "misc.h"
#include "search.h"


#include <queue>
#include <sstream>


using namespace std;

void random_player_cmd(Position &pos, istringstream &is);
void user_test(Position &pos, istringstream &is);

void is_ready_cmd(Position &pos, StateListPtr &states) {
  // --- 初期化

  Search::clear();
  Search::Stop = false;

  // 平手局面に初期化する。
  states = StateListPtr(new StateList(1));
  pos.set_hirate(&states->back());

  cout << "readyok" << endl;
}

void position_cmd(Position &pos, istringstream &is, StateListPtr &states) {
  Move m;
  string token, sfen;

  is >> token;

  if (token == "startpos") {
    sfen = SFEN_HIRATE;
    is >> token;
  }
  // 局面がsfen形式で指定されているなら、その局面を読み込む。
  // 文字列"sfen"は省略可能。
  else {
    if (token != "sfen")
      sfen += token += " ";
    while (is >> token && token != "moves")
      sfen += token += " ";
  }

  // 新しく渡す局面なので古いものは捨てて新しいものを作る。
  states = StateListPtr(new StateList(1));
  pos.set(sfen, &states->back());

  while (is >> token && (m = USI::to_move(pos, token)) != MOVE_NONE) {
    // 1手進めるごとにStateInfoが積まれていく。これは千日手の検出のために必要。
    states->emplace_back();
    if (m == MOVE_NULL) // do_move に MOVE_NULL を与えると死ぬので
      pos.do_null_move(states->back());
    else
      pos.do_move(m, states->back());
  }
}

void go_cmd(const Position &pos, istringstream &is, StateListPtr &states) {
  Search::LimitsType limits;
  string token;

  // 思考時間時刻の初期化
  Time.reset();

  while (is >> token) {
    if (token == "btime")
      is >> limits.time[BLACK];
    else if (token == "wtime")
      is >> limits.time[WHITE];

    // フィッシャールール時における時間
    else if (token == "winc")
      is >> limits.inc[WHITE];
    else if (token == "binc")
      is >> limits.inc[BLACK];

    // 秒読み設定。
    else if (token == "byoyomi") {
      TimePoint t = 0;
      is >> t;

      // USIプロトコルでは、これが先手後手同じ値だと解釈する。
      limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = t;
    }

    // この探索深さで探索を打ち切る
    else if (token == "depth")
      is >> limits.depth;

    // この探索ノード数で探索を打ち切る
    else if (token == "nodes")
      is >> limits.nodes;

    // 時間無制限。
    else if (token == "infinite")
      limits.infinite = 1;
  }

  // goコマンドのデフォルトを1秒読みにする
  if (limits.byoyomi[BLACK] == 0 && limits.inc[BLACK] == 0 &&
      limits.time[BLACK] == 0)
    limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = 1000;

  Search::start_thinking(pos, states, limits);
}

void USI::loop(int argc, char *argv[]) {
  // 探索開始局面(root)を格納するPositionクラス
  Position pos;

  string cmd, token;

  // 局面を遡るためのStateInfoのlist。
  StateListPtr states(new StateList(1));

  // 先行入力されているコマンド
  // コマンドは前から取り出すのでqueueを用いる。
  queue<string> cmds;

  // 引数として指定されたものを一つのコマンドとして実行する機能
  // ただし、','が使われていれば、そこでコマンドが区切れているものとして解釈する。

  for (int i = 1; i < argc; ++i) {
    string s = argv[i];

    // sから前後のスペースを除去しないといけない。
    while (*s.rbegin() == ' ')
      s.pop_back();
    while (*s.begin() == ' ')
      s = s.substr(1, s.size() - 1);

    if (s != ",")
      cmd += s + " ";
    else {
      cmds.push(cmd);
      cmd = "";
    }
  }
  if (cmd.size() != 0)
    cmds.push(cmd);

  do {
    if (cmds.size() == 0) {
      if (!std::getline(cin, cmd)) // 入力が来るかEOFがくるまでここで待機する。
        cmd = "quit";
    } else {
      cmd = cmds.front();
      cmds.pop();
    }

    istringstream is(cmd);

    token.clear();
    is >> skipws >> token;

    if (token == "usi")
      cout << engine_info() << "usiok" << endl;

    else if (token == "go")
      go_cmd(pos, is, states);

    else if (token == "position")
      position_cmd(pos, is, states);

    else if (token == "usinewgame")
      continue;

    else if (token == "isready")
      is_ready_cmd(pos, states);

    // 以下、デバッグのためのカスタムコマンド(非USIコマンド)
    // 探索中には使わないようにすべし。

    // 現在の局面を表示する。(デバッグ用)
    else if (token == "d")
      cout << pos << endl;

    // 現在の局面について評価関数を呼び出して、その値を返す。
    else if (token == "eval")
      cout << "eval = " << Eval::evaluate(pos) << endl;

    else if (token == "compiler")
      cout << compiler_info() << endl;

    else if (token == "sfen")
      position_cmd(pos, is, states);

    // ログファイルの書き出しのon
    else if (token == "log")
      start_logger(true);

    // この局面での指し手をすべて出力
    else if (token == "moves") {
      for (auto m : MoveList<LEGAL_ALL>(pos))
        cout << m.move << ' ';
      cout << endl;
    }

    // この局面が詰んでいるかの判定
    else if (token == "mated")
      cout << pos.is_mated() << endl;

    // この局面のhash keyの値を出力
    else if (token == "key")
      cout << hex << pos.state()->key() << dec << endl;

    // ランダムプレイヤーによるテスト
    else if (token == "rp")
      random_player_cmd(pos, is);

    // ユーザーによるテスト用コマンド
    else if (token == "user")
      user_test(pos, is);

    else {
      if (!token.empty())
        cout << "No such option: " << token << endl;
    }

  } while (token != "quit");
}

std::string USI::pv(const Position &pos, int depth) {
  std::stringstream ss;
  TimePoint elapsed = Time.elapsed() + 1;

  const auto &rootMoves = Search::rootMoves;
  uint64_t nodes_searched = Search::Nodes;

  bool updated = rootMoves[0].score != VALUE_INFINITE;

  if (depth == 1 && !updated)
    goto END;

  {
    int d = updated ? depth : depth - 1;
    Value v = updated ? rootMoves[0].score : rootMoves[0].previousScore;

    if (v == -VALUE_INFINITE)
      goto END;

    if (ss.rdbuf()->in_avail())
      ss << endl;

    ss << "info"
       << " depth " << d << " seldepth " << rootMoves[0].selDepth << " score "
       << USI::value(v);

    ss << " nodes " << nodes_searched << " nps "
       << nodes_searched * 1000 / elapsed;

    ss << " time " << elapsed;
    ss << " pv " << rootMoves[0].pv[0];
  }

END:;
  return ss.str();
}

std::string USI::value(Value v) {
  ASSERT_LV3(-VALUE_INFINITE < v && v < VALUE_INFINITE);

  std::stringstream s;

  if (v == VALUE_NONE)
    s << "none";
  else if (abs(v) < VALUE_MATE_IN_MAX_PLY)
    s << "cp " << v * 100 / int(Eval::PawnValue);
  else if (v == -VALUE_MATE)
    s << "mate -0";
  else
    s << "mate " << (v > 0 ? VALUE_MATE - v : -VALUE_MATE - v);

  return s.str();
}

std::string USI::move(Move m) {
  std::stringstream ss;
  if (!is_ok(m)) {
    ss << ((m == MOVE_RESIGN) ? "resign"
           : (m == MOVE_NULL) ? "null"
           : (m == MOVE_NONE) ? "none"
                              : "");
  } else if (is_drop(m)) {
    ss << move_dropped_piece(m);
    ss << '*';
    ss << move_to(m);
  } else {
    ss << move_from(m);
    ss << move_to(m);
    if (is_promote(m))
      ss << '+';
  }
  return ss.str();
}

Move USI::to_move(const Position &pos, const std::string &str) {
  if (str == "resign")
    return MOVE_RESIGN;

  if (str == "null")
    return MOVE_NULL;

  for (const ExtMove &ms : MoveList<LEGAL_ALL>(pos))
    if (str == move(ms.move))
      return ms.move;

  return MOVE_NONE;
}
