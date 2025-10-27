// #include "position.h"
#include "search.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <stack>

using namespace std;

std::string SFEN_HIRATE = "rbsgk/4p/5/P4/KGSBR b - 1";

namespace Zobrist {
Key zero;
Key side;
Key psq[SQ_NB][PIECE_NB];
Key hand[COLOR_NB][PIECE_HAND_NB];
} // namespace Zobrist

// ----------------------------------
//           CheckInfo
// ----------------------------------

// 王手情報の初期化
template <bool doNullMove> void Position::set_check_info(StateInfo *si) const {
  if (!doNullMove) {
    // null
    // moveのときは前の局面でこの情報は設定されているので更新する必要がない。
    si->blockersForKing[WHITE] =
        slider_blockers(BLACK, king_square(WHITE), si->pinners[WHITE]);
    si->blockersForKing[BLACK] =
        slider_blockers(WHITE, king_square(BLACK), si->pinners[BLACK]);
  }

  Square ksq = king_square(~sideToMove);

  Bitboard occ = pieces();
  Color them = ~sideToMove;

  si->checkSquares[PAWN] = pawnEffect(them, ksq);
  si->checkSquares[SILVER] = silverEffect(them, ksq);
  si->checkSquares[BISHOP] = bishopEffect(ksq, occ);
  si->checkSquares[ROOK] = rookEffect(ksq, occ);
  si->checkSquares[GOLD] = goldEffect(them, ksq);

  si->checkSquares[KING] = ZERO_BB;

  si->checkSquares[PRO_PAWN] = si->checkSquares[GOLD];
  si->checkSquares[PRO_SILVER] = si->checkSquares[GOLD];
  si->checkSquares[HORSE] = si->checkSquares[BISHOP] | kingEffect(ksq);
  si->checkSquares[DRAGON] = si->checkSquares[ROOK] | kingEffect(ksq);
}

// ----------------------------------
//       Zorbrist keyの初期化
// ----------------------------------

#define SET_HASH(x, p0, p1, p2, p3)                                            \
  {                                                                            \
    x = (p0);                                                                  \
    auto dummy_func = [](u64, u64, u64) {};                                    \
    dummy_func(p1, p2, p3);                                                    \
  }

void Position::init() {
  PRNG rng(20201130);

  SET_HASH(Zobrist::side, 1, 0, 0, 0);
  SET_HASH(Zobrist::zero, 0, 0, 0, 0);

  for (auto pc : Piece())
    for (auto sq : SQ)
      if (pc)
        SET_HASH(Zobrist::psq[sq][pc], rng.rand<Key>() & ~1ULL, rng.rand<Key>(),
                 rng.rand<Key>(), rng.rand<Key>());

  for (auto c : COLOR)
    for (Piece pr : {PAWN, SILVER, BISHOP, ROOK, GOLD})
      SET_HASH(Zobrist::hand[c][pr], rng.rand<Key>() & ~1ULL, rng.rand<Key>(),
               rng.rand<Key>(), rng.rand<Key>());
}

// sfen文字列で盤面を設定する
void Position::set(std::string sfen, StateInfo *si) {
  std::memset(this, 0, sizeof(Position));

  std::memset(si, 0, sizeof(StateInfo));
  st = si;

  // --- 盤面
  File f = FILE_5;
  Rank r = RANK_1;

  std::istringstream ss(sfen);
  // 盤面を読むときにスペースが盤面と手番とのセパレーターなのでそこを読み飛ばさないようにnoskipwsを指定しておく。
  ss >> std::noskipws;

  uint8_t token;
  bool promote = false;
  size_t idx;

  while ((ss >> token) && !isspace(token)) {
    // 数字は、空の升の数なのでその分だけ筋(File)を進める
    if (isdigit(token))
      f -= File(token - '0');
    // '/'は次の段を意味する
    else if (token == '/') {
      f = FILE_5;
      ++r;
    }
    // '+'は次の駒が成駒であることを意味する
    else if (token == '+')
      promote = true;
    else if ((idx = PieceToCharBW.find(token)) != string::npos) {
      // 盤面の(f,r)の駒を設定する
      auto sq = f | r;
      auto pc = Piece(idx + (promote ? u32(PIECE_PROMOTE) : 0));
      put_piece(sq, pc);

      // 1升進める
      --f;

      // 成りフラグ、戻しておく。
      promote = false;
    }
  }

  // put_piece()を使ったので更新しておく。
  // set_state()で駒種別のbitboardを参照するのでそれまでにこの関数を呼び出す必要がある。
  update_bitboards();

  // kingSquare[]の更新
  update_kingSquare();

  // --- 手番

  ss >> token;
  sideToMove = (token == 'w' ? WHITE : BLACK);
  ss >> token; // 手番と手駒とを分割スペース

  // --- 手駒

  hand[BLACK] = hand[WHITE] = (Hand)0;
  int ct = 0;
  while ((ss >> token) && !isspace(token)) {
    // 手駒なし
    if (token == '-')
      break;

    if (isdigit(token))
      // 駒の枚数。
      ct = (token - '0');
    else if ((idx = PieceToCharBW.find(token)) != string::npos) {
      // 個数が省略されていれば1という扱いをする。
      ct = max(ct, 1);
      add_hand(hand[color_of(Piece(idx))], type_of(Piece(idx)), ct);
      ct = 0;
    }
  }

  // --- 手数(平手の初期局面からの手数)

  gamePly = 0;
  ss >> std::skipws >> gamePly;

  // --- StateInfoの更新

  set_state(st);

  // --- validation
#if ASSERT_LV >= 3
  if (!is_ok(*this))
    std::cout << "info string Illigal Position?" << endl;
#endif
}

// 局面のsfen文字列を取得する。
// Position::set()の逆変換。
const std::string Position::sfen() const {
  std::ostringstream ss;

  // --- 盤面
  int emptyCnt;
  for (Rank r = RANK_1; r <= RANK_5; ++r) {
    for (File f = FILE_5; f >= FILE_1; --f) {
      // それぞれの升に対して駒がないなら
      // その段の、そのあとの駒のない升をカウントする
      for (emptyCnt = 0; f >= FILE_1 && piece_on(f | r) == NO_PIECE; --f)
        ++emptyCnt;

      // 駒のなかった升の数を出力
      if (emptyCnt)
        ss << emptyCnt;

      // 駒があったなら、それに対応する駒文字列を出力
      if (f >= FILE_1)
        ss << piece_on(f | r);
    }

    // 最下段以外では次の行があるのでセパレーターである'/'を出力する。
    if (r < RANK_5)
      ss << '/';
  }

  // --- 手番
  ss << (sideToMove == WHITE ? " w " : " b ");
  ;

  // --- 手駒
  // 手駒の出力順はUSIプロトコルでは規定されていないが、
  // USI原案によると、飛、角、金、銀、桂、香、歩の順である。
  const Piece USI_Hand[5] = {ROOK, BISHOP, GOLD, SILVER, PAWN};
  int n;
  bool found = false;
  for (Color c : COLOR)
    for (Piece p : USI_Hand) {
      // 手駒の枚数
      n = hand_count(hand[c], p);

      if (n != 0) {
        // 手駒が1枚でも見つかった
        found = true;

        // その種類の駒の枚数。1ならば出力を省略
        if (n != 1)
          ss << n;

        ss << PieceToCharBW[make_piece(c, p)];
      }
    }

  // 手駒がない場合はハイフンを出力
  ss << (found ? " " : "- ");

  // --- 初期局面からの手数
  ss << gamePly;

  return ss.str();
}

void Position::set_state(StateInfo *si) const {
  // --- bitboard

  // この局面で自玉に王手している敵駒
  si->checkersBB = attackers_to(~sideToMove, king_square(sideToMove));

  // 王手情報の初期化
  set_check_info<false>(si);

  // --- hash keyの計算
  si->board_key_ = sideToMove == BLACK ? Zobrist::zero : Zobrist::side;
  si->hand_key_ = Zobrist::zero;
  for (auto sq : pieces()) {
    auto pc = piece_on(sq);
    si->board_key_ += Zobrist::psq[sq][pc];
  }
  for (auto c : COLOR)
    for (Piece pr : {PAWN, SILVER, BISHOP, ROOK, GOLD})
      si->hand_key_ +=
          Zobrist::hand[c][pr] *
          (int64_t)hand_count(hand[c],
                              pr); // 手駒はaddにする(差分計算が楽になるため)

  // --- hand
  si->hand = hand[sideToMove];
}

void Position::update_bitboards() {
  // 王・馬・龍を合成したbitboard
  byTypeBB[HDK] = pieces(KING, HORSE, DRAGON);

  // 金と同じ移動特性を持つ駒
  byTypeBB[GOLDS] = pieces(GOLD, PRO_PAWN, PRO_SILVER);

  // 以下、attackers_to()で頻繁に用いるのでここで1回計算しておいても、トータルでは高速化する。

  // 角と馬
  byTypeBB[BISHOP_HORSE] = pieces(BISHOP, HORSE);

  // 飛車と龍
  byTypeBB[ROOK_DRAGON] = pieces(ROOK, DRAGON);

  // 銀とHDK
  byTypeBB[SILVER_HDK] = pieces(SILVER, HDK);

  // 金相当の駒とHDK
  byTypeBB[GOLDS_HDK] = pieces(GOLDS, HDK);
}

// このクラスが保持しているkingSquare[]の更新。
void Position::update_kingSquare() {
  for (auto c : COLOR) {
    auto b = pieces(c, KING);
    ASSERT_LV3(b);
    kingSquare[c] = b.pop();
  }
}

// 局面を出力する。(USI形式ではない) デバッグ用。
std::ostream &operator<<(std::ostream &os, const Position &pos) {
  for (Rank r = RANK_1; r <= RANK_5; ++r) {
    for (File f = FILE_5; f >= FILE_1; --f)
      os << pretty(pos.board[f | r]);
    os << endl;
  }

#if !defined(PRETTY_JP)
  // 手駒
  os << "BLACK HAND : " << pos.hand[BLACK]
     << " , WHITE HAND : " << pos.hand[WHITE] << endl;

  // 手番
  os << "Turn = " << pos.sideToMove << endl;
#else
  os << "先手 手駒 : " << pos.hand[BLACK]
     << " , 後手 手駒 : " << pos.hand[WHITE] << endl;
  os << "手番 = " << pos.sideToMove << endl;
#endif

  // sfen文字列もおまけで表示させておく。(デバッグのときに便利)
  os << "sfen " << pos.sfen() << endl;

  return os;
}

// 開始局面からこの局面にいたるまでの指し手を表示する。
std::string Position::moves_from_start(bool is_pretty) const {
  StateInfo *p = st;
  std::stack<StateInfo *> states;
  while (p->previous != nullptr) {
    states.push(p);
    p = p->previous;
  }

  stringstream ss;
  while (states.size()) {
    auto &top = states.top();
    if (is_pretty)
      ss << pretty(top->lastMove, top->lastMovedPieceType) << ' ';
    else
      ss << top->lastMove << ' ';
    states.pop();
  }
  return ss.str();
}

// ----------------------------------
//      ある升へ利いている駒など
// ----------------------------------

// Position::slider_blockers()
// は、c側の長い利きを持つ駒(sliders)から、升sへの利きを
// 遮っている先後の駒の位置をBitboardで返す。ただし、２重に遮っている場合はそれらの駒は返さない。
// もし、この関数のこの返す駒を取り除いた場合、升sに対してsliderによって利きがある状態になる。
// 升sにある玉に対してこの関数を呼び出した場合、それはpinされている駒と両王手の候補となる駒である。
// また、升sにある玉は~c側のKINGであるとする。

Bitboard Position::slider_blockers(Color c, Square s, Bitboard &pinners) const {
  Bitboard blockers = ZERO_BB;

  // pinnders は返し値。
  pinners = ZERO_BB;

  Bitboard snipers = ((pieces(ROOK_DRAGON) & rookStepEffect(s)) |
                      (pieces(BISHOP_HORSE) & bishopStepEffect(s))) &
                     pieces(c);

  while (snipers) {
    Square sniperSq = snipers.pop();
    Bitboard b = between_bb(s, sniperSq) & pieces();

    if (b && !more_than_one(b)) {
      blockers |= b;
      if (b & pieces(~c))
        // sniperと玉に挟まれた駒が玉と同じ色の駒であるなら、pinnerに追加。
        pinners |= sniperSq;
    }
  }
  return blockers;
}

// sに利きのあるc側の駒を列挙する。
// (occが指定されていなければ現在の盤面において。occが指定されていればそれをoccupied
// bitboardとして)
Bitboard Position::attackers_to(Color c, Square sq, const Bitboard &occ) const {
  ASSERT_LV3(is_ok(c) && sq <= SQ_NB);

  Color them = ~c;

  // sの地点に敵駒ptをおいて、その利きに自駒のptがあればsに利いているということだ。
  return ((pawnEffect(them, sq) & pieces(PAWN)) |
          (silverEffect(them, sq) & pieces(SILVER_HDK)) |
          (goldEffect(them, sq) & pieces(GOLDS_HDK)) |
          (bishopEffect(sq, occ) & pieces(BISHOP_HORSE)) |
          (rookEffect(sq, occ) & pieces(ROOK_DRAGON))) &
         pieces(c); // 先後混在しているのでc側の駒だけ最後にマスクする。
}

// sに利きのあるc側の駒を列挙する。先後両方。
// (occが指定されていなければ現在の盤面において。occが指定されていればそれをoccupied
// bitboardとして)
Bitboard Position::attackers_to(Square sq, const Bitboard &occ) const {
  ASSERT_LV3(sq <= SQ_NB);

  // sqの地点に敵駒ptをおいて、その利きに自駒のptがあればsqに利いているということだ。
  return
      // 先手の歩・銀・金・HDK
      (((pawnEffect(WHITE, sq) & pieces(PAWN)) |
        (silverEffect(WHITE, sq) & pieces(SILVER_HDK)) |
        (goldEffect(WHITE, sq) & pieces(GOLDS_HDK))) &
       pieces(BLACK)) |

      // 後手の歩・銀・金・HDK
      (((pawnEffect(BLACK, sq) & pieces(PAWN)) |
        (silverEffect(BLACK, sq) & pieces(SILVER_HDK)) |
        (goldEffect(BLACK, sq) & pieces(GOLDS_HDK))) &
       pieces(WHITE))

      // 先後の角・飛
      | (bishopEffect(sq, occ) & pieces(BISHOP_HORSE)) |
      (rookEffect(sq, occ) & pieces(ROOK_DRAGON));
}

// 打ち歩詰め判定に使う。王に打ち歩された歩の升をpawn_sqとして、c側(王側)のpawn_sqへ利いている駒を列挙する。
inline Bitboard Position::attackers_to_pawn(Color c, Square pawn_sq) const {
  ASSERT_LV3(is_ok(c) && pawn_sq <= SQ_NB);

  Color them = ~c;
  const Bitboard &occ = pieces();

  // 馬と龍
  const Bitboard bb_hd = kingEffect(pawn_sq) & pieces(HORSE, DRAGON);
  // 馬、龍の利きは考慮しないといけない。しかしここに玉が含まれるので玉は取り除く必要がある。
  // bb_hdは銀と金のところに加えてしまうことでテーブル参照を一回減らす。

  // sの地点に敵駒ptをおいて、その利きに自駒のptがあればsに利いているということだ。
  // 打ち歩詰め判定なので、その打たれた歩を歩、王で取れることはない。(王で取れないことは事前にチェック済)
  return ((silverEffect(them, pawn_sq) & (pieces(SILVER) | bb_hd)) |
          (goldEffect(them, pawn_sq) & (pieces(GOLDS) | bb_hd)) |
          (bishopEffect(pawn_sq, occ) & pieces(BISHOP_HORSE)) |
          (rookEffect(pawn_sq, occ) & pieces(ROOK_DRAGON))) &
         pieces(c);
}

bool Position::gives_check(Move m) const {
  ASSERT_LV3(is_ok(m));

  const Square to = move_to(m);

  if (st->checkSquares[type_of(moved_piece_after(m))] & to)
    return true;

  const Square from = move_from(m);

  return !is_drop(m) && ((blockers_for_king(~sideToMove) & from) &&
                         !aligned(from, to, king_square(~sideToMove)));
}

// 現局面で指し手がないかをテストする。指し手生成ルーチンを用いるので速くない。探索中には使わないこと。
bool Position::is_mated() const {
  // 不成で詰めろを回避できるパターンはないのでLEGAL_ALLである必要はない。
  return MoveList<LEGAL>(*this).size() == 0;
}

// ----------------------------------
//      指し手の合法性のテスト
// ----------------------------------

bool Position::legal_drop(const Square to) const {
  const auto us = sideToMove;

  // 打とうとする歩の利きに相手玉がいることは前提条件としてクリアしているはず。
  ASSERT_LV3(pawnEffect(us, to) == Bitboard(king_square(~us)));

  // この歩に利いている自駒(歩を打つほうの駒)がなければ詰みには程遠いのでtrue
  if (!effected_to(us, to))
    return true;

  // ここに利いている敵の駒があり、その駒で取れるなら打ち歩詰めではない
  Bitboard b = attackers_to_pawn(~us, to);

  // 敵玉に対するpinしている駒(自駒も含むが、bが敵駒なので問題ない。)
  Bitboard pinned = blockers_for_king(~us);

  // pinされていない駒が1つでもあるなら、相手はその駒で取って何事もない。
  if (b & (~pinned | FILE_BB[file_of(to)]))
    return true;

  // 玉の退路を探す
  // 自駒がなくて、かつ、to(はすでに調べたので)以外の地点

  // 相手玉の場所
  Square sq_king = king_square(~us);

  Bitboard escape_bb = kingEffect(sq_king) & ~pieces(~us);
  escape_bb ^= to;
  auto occ =
      pieces() ^
      to; // toには歩をおく前提なので、ここには駒があるものとして、これでの利きの遮断は考えないといけない。
  while (escape_bb) {
    Square king_to = escape_bb.pop();
    if (!attackers_to(us, king_to, occ))
      return true; // 退路が見つかったので打ち歩詰めではない。
  }

  // すべての検査を抜けてきたのでこれは打ち歩詰めの条件を満たしている。
  return false;
}

// mがこの局面においてpseudo_legalかどうかを判定するための関数。
template <bool All> bool Position::pseudo_legal_s(const Move m) const {
  const Color us = sideToMove;
  const Square to = move_to(m); // 移動先

  if (is_drop(m)) {
    const Piece pr = move_dropped_piece(m);
    // 上位32bitに移動後の駒が格納されている。それと一致するかのテスト
    if (moved_piece_after(m) !=
        Piece(pr + (us == WHITE ? u32(PIECE_WHITE) : 0)))
      return false;

    ASSERT_LV3(PAWN <= pr && pr < KING);

    // 打つ先の升が埋まっていたり、その手駒を持っていなかったりしたら駄目。
    if (piece_on(to) != NO_PIECE || hand_count(hand[us], pr) == 0)
      return false;

    if (in_check()) {
      // 王手されている局面なので合駒でなければならない
      Bitboard target = checkers();
      Square checksq = target.pop();

      // 王手している駒を1個取り除いて、もうひとつあるということは王手している駒が
      // 2つあったということであり、両王手なので合い利かず。
      if (target)
        return false;

      // 王と王手している駒との間の升に駒を打っていない場合、それは王手を回避していることに
      // ならないので、これは非合法手。
      if (!(between_bb(checksq, king_square(us)) & to))
        return false;
    }

    // 歩のとき、二歩および打ち歩詰めであるなら非合法手
    if (pr == PAWN && !legal_pawn_drop(us, to))
      return false;

    // 打てない段に打つ歩はそもそも生成されていない。

  } else {
    const Square from = move_from(m);
    const Piece pc = piece_on(from);

    // 動かす駒が自駒でなければならない
    if (pc == NO_PIECE || color_of(pc) != us)
      return false;

    // toに移動できないといけない。
    if (!(effects_from(pc, from, pieces()) & to))
      return false;

    // toの地点に自駒があるといけない
    if (pieces(us) & to)
      return false;

    Piece pt = type_of(pc);
    if (is_promote(m)) {
      // --- 成る指し手

      // 成れない駒の成りではないことを確かめないといけない。
      static_assert(GOLD == 7, "GOLD must be 7.");
      if (pt >= GOLD)
        return false;

      if (moved_piece_after(m) != pc + PIECE_PROMOTE)
        return false;
    } else {
      // --- 成らない指し手

      if (moved_piece_after(m) != pc)
        return false;

      // 駒打ちのところに書いた理由により、不成で進めない升への指し手のチェックも不要。
      // 間違い　→　駒種をmoveに含めていないならこのチェック必要だわ。
      // 52から51銀のような指し手がkillerやcountermoveに登録されていたとして、52に歩があると
      // 51歩不成という指し手を生成してしまう…。
      // あと、歩や大駒が敵陣において成らない指し手も不要なのでは..。

      if (All) {
        // 歩と香に関しては1段目への不成は不可。桂は、桂飛びが出来る駒は桂しかないので
        // 移動元と移動先がこれであるかぎり、それは桂の指し手生成によって生成されたものだから
        // これが非合法手であることはない。

        if (pt == PAWN)
          if ((us == BLACK && rank_of(to) == RANK_1) ||
              (us == WHITE && rank_of(to) == RANK_5))
            return false;
      } else {
        // 歩の不成と香の2段目への不成を禁止。
        // 大駒の不成を禁止
        switch (pt) {
        case PAWN:
          if (enemy_field(us) & to)
            return false;
          break;

        case BISHOP:
        case ROOK:
          if (enemy_field(us) & (Bitboard(from) | Bitboard(to)))
            return false;
          break;

        default:
          break;
        }
      }
    }

    // 王手している駒があるのか
    if (checkers()) {
      // このとき、指し手生成のEVASIONで生成される指し手と同等以上の条件でなければならない。

      // 動かす駒は王以外か？
      if (type_of(pc) != KING) {
        // 両王手なら王の移動をさせなければならない。
        if (more_than_one(checkers()))
          return false;

        // 指し手は、王手を遮断しているか、王手している駒の捕獲でなければならない。
        // ※　王手している駒と王の間に王手している駒の升を足した升が駒の移動先であるか。
        // 例) 王■■■^飛
        // となっているときに■の升か、^飛
        // のところが移動先であれば王手は回避できている。
        // (素抜きになる可能性はあるが、そのチェックはここでは不要)
        if (!((between_bb(checkers().pop(), king_square(us)) | checkers()) &
              to))
          return false;
      }

      // 玉の自殺手のチェックはlegal()のほうで調べているのでここではやらない。
    }
  }

  return true;
}

// 生成した指し手(CAPTUREとかNON_CAPTUREとか)が、合法であるかどうかをテストする。
bool Position::legal(Move m) const {
  if (is_drop(m))
    // 打ち歩詰めは指し手生成で除外されている。
    return true;
  else {
    Color us = sideToMove;
    Square from = move_from(m);

    ASSERT_LV5(color_of(piece_on(move_from(m))) == us);
    ASSERT_LV5(piece_on(king_square(us)) == make_piece(us, KING));

    // もし移動させる駒が玉であるなら、行き先の升に相手側の利きがないかをチェックする。
    if (type_of(piece_on(from)) == KING)
      return !effected_to(~us, move_to(m), from);

    // blockers_for_king()は、pinされている駒(自駒・敵駒)を表現するが、fromにある駒は自駒であることは
    // わかっているのでこれで良い。
    return !(blockers_for_king(us) & from) ||
           aligned(from, move_to(m), king_square(us));
  }
}

// ----------------------------------
//      局面を進める/戻す
// ----------------------------------

// 指し手で盤面を1手進める。
template <Color Us>
void Position::do_move_impl(Move m, StateInfo &new_st, bool givesCheck) {
  // MOVE_NONEはもちろん、MOVE_NULL , MOVE_RESIGNなどお断り。
  ASSERT_LV3(is_ok(m));

  ASSERT_LV3(&new_st != st);

  // 探索ノード数 ≒do_move()の呼び出し回数のインクリメント。
  Search::Nodes += 1;

  // ----------------------
  //  StateInfoの更新
  // ----------------------

  // 現在の局面のhash keyはこれで、これを更新していき、次の局面のhash
  // keyを求めてStateInfo::key_に格納。
  auto k = st->board_key_ ^ Zobrist::side;
  auto h = st->hand_key_;

  // 現在

  // StateInfoを遡れるようにpreviousを設定しておいてやる。
  StateInfo *prev;
  new_st.previous = prev = st;
  st = &new_st;

  // --- 手数がらみのカウンターのインクリメント

  // 初期盤面からの手数
  ++gamePly;

  // st->previousで遡り可能な手数カウンタ
  st->pliesFromNull = prev->pliesFromNull + 1;

  // 直前の指し手を保存する
  st->lastMove = m;
  st->lastMovedPieceType =
      is_drop(m) ? (Piece)move_from(m) : type_of(piece_on(move_from(m)));

  // ----------------------
  //    盤面の更新処理
  // ----------------------

  // 移動先の升
  Square to = move_to(m);
  ASSERT_LV2(is_ok(to));

  if (is_drop(m)) {
    // --- 駒打ち

    // 移動先の升は空のはず
    ASSERT_LV2(piece_on(to) == NO_PIECE);

    Piece pc = moved_piece_after(m);
    Piece pr = raw_type_of(pc);
    ASSERT_LV2(PAWN <= pr && pr < PIECE_HAND_NB);

    // Zobrist keyの更新
    k += Zobrist::psq[to][pc];
    h -= Zobrist::hand[Us][pr];

    put_piece(to, pc);

    // 駒打ちなので手駒が減る。
    sub_hand(hand[Us], pr);

    if (givesCheck) {
      // 王手している駒のbitboardを更新する。
      // 駒打ちなのでこの駒で王手になったに違いない。駒打ちで両王手はありえないので王手している駒はいまtoに置いた駒のみ。
      st->checkersBB = Bitboard(to);
      st->continuousCheck[Us] = prev->continuousCheck[Us] + 2;
    } else {
      st->checkersBB = ZERO_BB;
      st->continuousCheck[Us] = 0;
    }

    // 駒打ちは捕獲した駒がない。
    st->capturedPiece = NO_PIECE;

    // put_piece()などを用いたのでupdateする
    update_bitboards();
  } else {
    // -- 駒の移動
    Square from = move_from(m);
    ASSERT_LV2(is_ok(from));

    // 移動させる駒
    Piece moved_pc = piece_on(from);
    ASSERT_LV2(moved_pc != NO_PIECE);

    // 移動先に駒の配置
    // もし成る指し手であるなら、成った後の駒を配置する。
    Piece moved_after_pc = moved_piece_after(m);

    // 移動先の升にある駒
    Piece to_pc = piece_on(to);
    if (to_pc != NO_PIECE) {
      // --- capture(駒の捕獲)

      // 玉を取る指し手が実現することはない。この直前の局面で玉を逃げる指し手しか合法手ではないし、
      // 玉を逃げる指し手がないのだとしたら、それは詰みの局面であるから。
      ASSERT_LV1(type_of(to_pc) != KING);

      Piece pr = raw_type_of(to_pc);

      // 駒取りなら現在の手番側の駒が増える。
      add_hand(hand[Us], pr);

      // 捕獲される駒の除去
      remove_piece(to);

      // 捕獲された駒が盤上から消えるので局面のhash keyを更新する
      k -= Zobrist::psq[to][to_pc];
      h += Zobrist::hand[Us][pr];

      // 捕獲した駒をStateInfoに保存しておく。(undo_moveのため)
      st->capturedPiece = to_pc;
    } else {
      // 駒を取らない指し手
      st->capturedPiece = NO_PIECE;
    }

    // 移動元の升からの駒の除去
    remove_piece(from);

    // 移動先の升に駒を配置
    put_piece(to, moved_after_pc);

    if (type_of(moved_pc) == KING) {
      kingSquare[Us] = to;
    }

    // fromにあったmoved_pcがtoにmoved_after_pcとして移動した。
    k -= Zobrist::psq[from][moved_pc];
    k += Zobrist::psq[to][moved_after_pc];

    // put_piece()などを用いたのでupdateする。
    update_bitboards();

    // 王手している駒のbitboardを更新する。
    if (givesCheck) {
      st->checkersBB = attackers_to(Us, king_square(~Us));
      st->continuousCheck[Us] = prev->continuousCheck[Us] + 2;

    } else {

      st->checkersBB = ZERO_BB;
      st->continuousCheck[Us] = 0;
    }
  }
  // 相手番のほうは関係ないので前ノードの値をそのまま受け継ぐ。
  st->continuousCheck[~Us] = prev->continuousCheck[~Us];

  // 相手番に変更する。
  sideToMove = ~Us;

  // 更新されたhash keyをStateInfoに書き戻す。
  st->board_key_ = k;
  st->hand_key_ = h;

  st->hand = hand[sideToMove];

  // このタイミングで王手関係の情報を更新しておいてやる。
  set_check_info<false>(st);
}

// 指し手で盤面を1手戻す。do_move()の逆変換。
template <Color Us> void Position::undo_move_impl(Move m) {
  // Usは1手前の局面での手番(に呼び出し元でしてある)

  auto to = move_to(m);
  ASSERT_LV2(is_ok(to));

  // --- 移動後の駒
  Piece moved_after_pc = moved_piece_after(m);

  // 移動前の駒
  Piece moved_pc =
      is_promote(m) ? (moved_after_pc - PIECE_PROMOTE) : moved_after_pc;

  if (is_drop(m)) {
    // --- 駒打ち

    // toの場所にある駒を手駒に戻す
    Piece pt = raw_type_of(moved_after_pc);

    add_hand(hand[Us], pt);

    // toの場所から駒を消す
    remove_piece(to);

  } else {

    // --- 通常の指し手

    auto from = move_from(m);
    ASSERT_LV2(is_ok(from));

    // toの場所から駒を消す
    remove_piece(to);

    // toの地点には捕獲された駒があるならその駒が盤面に戻り、手駒から減る。
    // 駒打ちの場合は捕獲された駒があるということはありえない。
    // (なので駒打ちの場合は、st->capturedTypeを設定していないから参照してはならない)
    if (st->capturedPiece != NO_PIECE) {
      Piece to_pc = st->capturedPiece;

      // 盤面のtoの地点に捕獲されていた駒を復元する
      put_piece(to, to_pc);
      put_piece(from, moved_pc);

      // 手駒から減らす
      sub_hand(hand[Us], raw_type_of(to_pc));
    } else {
      put_piece(from, moved_pc);
    }

    if (type_of(moved_pc) == KING)
      kingSquare[Us] = from;
  }

  // put_piece()などを使ったのでbitboardを更新する。
  // kingSquareは自前で更新したのでupdate_kingSquare()を呼び出す必要はない。
  update_bitboards();

  // --- 相手番に変更
  sideToMove = Us; // Usは先後入れ替えて呼び出されているはず。

  // --- StateInfoを巻き戻す
  st = st->previous;

  --gamePly;
}

// do_move()を先後分けたdo_move_impl<>()を呼び出す。
void Position::do_move(Move m, StateInfo &newSt, bool givesCheck) {
  if (sideToMove == BLACK)
    do_move_impl<BLACK>(m, newSt, givesCheck);
  else
    do_move_impl<WHITE>(m, newSt, givesCheck);
}

// undo_move()を先後分けたdo_move_impl<>()を呼び出す。
void Position::undo_move(Move m) {
  if (sideToMove == BLACK)
    undo_move_impl<WHITE>(
        m); // 1手前の手番が返らないとややこしいので入れ替えておく。
  else
    undo_move_impl<BLACK>(m);
}

// null move searchに使われる。手番だけ変更する。
void Position::do_null_move(StateInfo &newSt) {
  ASSERT_LV3(!checkers());
  ASSERT_LV3(&newSt != st);

  std::memcpy(&newSt, st, sizeof(StateInfo));
  newSt.previous = st;
  st = &newSt;

  st->pliesFromNull = 0;

  sideToMove = ~sideToMove;

  set_check_info<true>(st);
}

void Position::undo_null_move() {
  ASSERT_LV3(!checkers());

  st = st->previous;
  sideToMove = ~sideToMove;
}

// ----------------------------------
//      千日手判定
// ----------------------------------

RepetitionState Position::is_repetition(int repPly /* = 16 */) const {
  // repPlyまで遡る
  // 現在の局面と同じhash
  // keyを持つ局面があれば、それは千日手局面であると判定する。

  // 　rootより遡るなら、2度出現する(3度目の同一局面である)必要がある。
  //   rootより遡らないなら、1度目(2度目の同一局面である)で千日手と判定する。

  // pliesFromNullが未初期化になっていないかのチェックのためのassert
  ASSERT_LV3(st->pliesFromNull >= 0);

  // 遡り可能な手数。
  // 最大でもrepPly手までしか遡らないことにする。
  int end = std::min(repPly, st->pliesFromNull);

  // 少なくとも4手かけないと千日手にはならないから、4手前から調べていく。
  if (end < 4)
    return REPETITION_NONE;

  StateInfo *stp = st->previous->previous;

  for (int i = 4; i <= end; i += 2) {
    stp = stp->previous->previous;

    // board_key : 盤上の駒のみのhash(手駒を除く)
    // 盤上の駒が同じ状態であるかを判定する。
    if (stp->board_key() == st->board_key()) {
      // 手駒が一致するなら同一局面である。(2手ずつ遡っているので手番は同じである)
      if (stp->hand == st->hand) {
        // 自分が王手をしている連続王手の千日手なのか？
        if (i <= st->continuousCheck[sideToMove])
          return REPETITION_LOSE;

        // 相手が王手をしている連続王手の千日手なのか？
        if (i <= st->continuousCheck[~sideToMove])
          return REPETITION_WIN;

        // 先手側の負け
        if (sideToMove == BLACK)
          return REPETITION_LOSE;

        // 後手側の勝ち
        if (sideToMove == WHITE)
          return REPETITION_WIN;

        return REPETITION_DRAW; // error
      } else {
        // 優等局面か劣等局面であるか。(手番が相手番になっている場合はいま考えない)
        if (hand_is_equal_or_superior(st->hand, stp->hand))
          return REPETITION_SUPERIOR;
        if (hand_is_equal_or_superior(stp->hand, st->hand))
          return REPETITION_INFERIOR;
      }
    }
  }

  // 同じhash keyの局面が見つからなかったので…。
  return REPETITION_NONE;
}

bool Position::pos_is_ok() const {
#if 1
  // 1) 盤上の駒と手駒を合わせて12駒あるか。

  // それぞれの駒のあるべき枚数
  const int ptc0[KING] = {2 /*玉*/, 2 /*歩*/, 0 /*香*/, 0 /*桂*/,
                          2 /*銀*/, 2 /*角*/, 2 /*飛*/, 2 /*金*/};
  // カウント用の変数
  int ptc[PIECE_WHITE] = {0};

  int count = 0;
  for (auto sq : SQ) {
    Piece pc = piece_on(sq);
    if (pc != NO_PIECE) {
      ++count;
      ++ptc[raw_type_of(pc)];
    }
  }
  for (auto c : COLOR)
    for (Piece pr : {PAWN, SILVER, BISHOP, ROOK, GOLD}) {
      int ct = hand_count(hand[c], pr);
      count += ct;
      ptc[pr] += ct;
    }

  if (count != 12)
    return false;

  // 2) それぞれの駒の枚数は合っているか
  for (Piece pt = PIECE_ZERO; pt < KING; ++pt)
    if (ptc[pt] != ptc0[pt])
      return false;
#endif

  // 3) 王手している駒
  if (st->checkersBB != attackers_to(~sideToMove, king_square(sideToMove)))
    return false;

  // 4) 相手玉が取れるということはないか
  if (effected_to(sideToMove, king_square(~sideToMove)))
    return false;

  // 5) occupied bitboardは合っているか
  if ((pieces() != (pieces(BLACK) | pieces(WHITE))) ||
      (pieces(BLACK) & pieces(WHITE)))
    return false;

  // 6) 王手している駒は敵駒か
  if (checkers() & pieces(side_to_move()))
    return false;

  // 二歩のチェックなど云々かんぬん..面倒くさいので省略。

  return true;
}

// 明示的な実体化
template bool Position::pseudo_legal_s<false>(const Move m) const;
template bool Position::pseudo_legal_s<true>(const Move m) const;
