#ifndef _POSITION_H_
#define _POSITION_H_

#include "bitboard.h"

#include <deque>
#include <memory> // std::unique_ptr

// --------------------
//     局面の定数
// --------------------

// 平手の開始局面
extern std::string SFEN_HIRATE;

// --------------------
//     局面の情報
// --------------------

// StateInfoは、undo_move()で局面を戻すときに情報を元の状態に戻すのが面倒なものを詰め込んでおくための構造体。
// do_move()のときは、ブロックコピーで済むのでそこそこ高速。
struct StateInfo {
  // 遡り可能な手数(previousポインタを用いて局面を遡るときに用いる)
  int pliesFromNull;

  // この手番側の連続王手は何手前からやっているのか(連続王手の千日手の検出のときに必要)
  int continuousCheck[COLOR_NB];

  // 現局面で手番側に対して王手をしている駒のbitboard
  Bitboard checkersBB;

  // 自玉に対して(敵駒によって)pinされている駒
  Bitboard blockersForKing[COLOR_NB];

  // 自玉に対してpinしている(可能性のある)敵の大駒。
  // 自玉に対して上下左右方向にある敵の飛車、斜め十字方向にある敵の角
  Bitboard pinners[COLOR_NB];

  // 自駒の駒種Xによって敵玉が王手となる升のbitboard
  Bitboard checkSquares[PIECE_WHITE];

  // 直前の指し手
  Move lastMove;

  // lastMoveで移動させた駒(先後の区別なし)
  Piece lastMovedPieceType;

  // この局面のハッシュキー
  // ※　次の局面にdo_move()で進むときに最終的な値が設定される
  // board_key()は盤面のhash。hand_key()は手駒のhash。それぞれ加算したのがkey()
  // 局面のhash。 board_key()のほうは、手番も込み。

  Key board_key_;
  Key hand_key_;

  Key key() const { return board_key_ + hand_key_; }
  Key board_key() const { return board_key_; }
  Key hand_key() const { return hand_key_; }

  // この局面における手番側の持ち駒。優等局面の判定のために必要。
  Hand hand;

  // この局面で捕獲された駒。先後の区別あり。
  // ※　次の局面にdo_move()で進むときにこの値が設定される
  Piece capturedPiece;

  // 一つ前の局面に遡るためのポインタ。
  // この値としてnullptrが設定されているケースは、
  // 1) root node
  // 2) 直前がnull move
  // のみである。
  StateInfo *previous;

  void *operator new(std::size_t s);
  void operator delete(void *p) noexcept;
};

// setup
// moves("position"コマンドで設定される、現局面までの指し手)に沿った局面の状態を追跡するためのStateInfoのlist。
// 千日手の判定のためにこれが必要。std::dequeを使っているのは、StateInfoがポインターを内包しているので、resizeに対して
// 無効化されないように。
typedef std::deque<StateInfo> StateList;
typedef std::unique_ptr<StateList> StateListPtr;

// --------------------
//       局面
// --------------------

class Position {
public:
  // --- ctor

  Position() = default;
  Position &operator=(const Position &) = delete;

  // Positionで用いるZobristテーブルの初期化
  static void init();

  // sfen文字列で局面を設定する
  // 局面を遡るために、rootまでの局面の情報が必要であるから、それを引数のsiで渡してやる。
  // 遡る必要がない場合は、StateInfo si;に対して&siなどとして渡しておけば良い。
  // 内部的にmemset(si,0,sizeof(StateInfo))として、この渡されたインスタンスをクリアしている。
  void set(std::string sfen, StateInfo *si);

  // 局面のsfen文字列を取得する
  const std::string sfen() const;

  // 平手の初期局面を設定する。
  // siについては、上記のset()にある説明を読むこと。
  void set_hirate(StateInfo *si) { set(SFEN_HIRATE, si); }

  // --- properties

  // 現局面の手番を返す。
  Color side_to_move() const { return sideToMove; }

  // (将棋の)開始局面からの手数を返す。
  // 平手の開始局面なら1が返る。(0ではない)
  int game_ply() const { return gamePly; }

  // 盤面上の駒を返す。
  Piece piece_on(Square sq) const {
    ASSERT_LV3(sq <= SQ_NB);
    return board[sq];
  }

  // c側の手駒を返す。
  Hand hand_of(Color c) const {
    ASSERT_LV3(is_ok(c));
    return hand[c];
  }

  // c側の手駒を返す
  FORCE_INLINE Square king_square(Color c) const {
    ASSERT_LV3(is_ok(c));
    return kingSquare[c];
  }

  // 保持しているデータに矛盾がないかテストする。
  bool pos_is_ok() const;

  // moved_pieceの拡張版。
  // 成りの指し手のときは成りの指し手を返す。(移動後の駒)
  // 単にmoveの上位16bitを返す。
  Piece moved_piece_after(Move m) const { return Piece(m >> 16); }

  // 普通の千日手、連続王手の千日手等を判定する。
  // そこまでの局面と同一局面であるかを、局面を遡って調べる。
  // rep_ply : 遡る手数。デフォルトでは16手。あまり大きくすると速度低下を招く。
  RepetitionState is_repetition(int rep_ply = 16) const;

  // --- Bitboard

  // 先手か後手か、いずれかの駒がある場所が1であるBitboardが返る。
  Bitboard pieces() const { return byTypeBB[ALL_PIECES]; }

  // c == BLACK : 先手の駒があるBitboardが返る
  // c == WHITE : 後手の駒があるBitboardが返る
  Bitboard pieces(Color c) const {
    ASSERT_LV3(is_ok(c));
    return byColorBB[c];
  }

  // 駒がない升が1になっているBitboardが返る
  Bitboard empties() const { return pieces() ^ ALL_BB; }

  // 駒に対応するBitboardを得る。
  // ・引数でcの指定がないものは先後両方の駒が返る。
  // ・引数がPieceのものは、prはPAWN〜DRAGON , GOLDS(金相当の駒) ,
  // HDK(馬・龍・玉) ,
  //	  BISHOP_HORSE(角・馬) , ROOK_DRAGON(飛車・龍)。
  // ・引数でPieceを2つ取るものは２種類の駒のBitboardを合成したものが返る。

  Bitboard pieces(Piece pr) const {
    ASSERT_LV3(pr < PIECE_BB_NB);
    return byTypeBB[pr];
  }
  Bitboard pieces(Piece pr1, Piece pr2) const {
    return pieces(pr1) | pieces(pr2);
  }
  Bitboard pieces(Piece pr1, Piece pr2, Piece pr3) const {
    return pieces(pr1) | pieces(pr2) | pieces(pr3);
  }
  Bitboard pieces(Piece pr1, Piece pr2, Piece pr3, Piece pr4) const {
    return pieces(pr1) | pieces(pr2) | pieces(pr3) | pieces(pr4);
  }
  Bitboard pieces(Piece pr1, Piece pr2, Piece pr3, Piece pr4, Piece pr5) const {
    return pieces(pr1) | pieces(pr2) | pieces(pr3) | pieces(pr4) | pieces(pr5);
  }

  Bitboard pieces(Color c, Piece pr) const { return pieces(pr) & pieces(c); }
  Bitboard pieces(Color c, Piece pr1, Piece pr2) const {
    return pieces(pr1, pr2) & pieces(c);
  }
  Bitboard pieces(Color c, Piece pr1, Piece pr2, Piece pr3) const {
    return pieces(pr1, pr2, pr3) & pieces(c);
  }
  Bitboard pieces(Color c, Piece pr1, Piece pr2, Piece pr3, Piece pr4) const {
    return pieces(pr1, pr2, pr3, pr4) & pieces(c);
  }
  Bitboard pieces(Color c, Piece pr1, Piece pr2, Piece pr3, Piece pr4,
                  Piece pr5) const {
    return pieces(pr1, pr2, pr3, pr4, pr5) & pieces(c);
  }

  // --- 王手

  // 現局面で王手している駒
  Bitboard checkers() const { return st->checkersBB; }

  // c側の玉に対してpinしている駒
  Bitboard blockers_for_king(Color c) const { return st->blockersForKing[c]; }

  // 現局面で駒ptを動かしたときに王手となる升を表現するBitboard
  Bitboard check_squares(Piece pt) const {
    ASSERT_LV3(pt != NO_PIECE && pt < PIECE_WHITE);
    return st->checkSquares[pt];
  }

  // --- 利き

  Bitboard attackers_to(Color c, Square sq) const {
    return attackers_to(c, sq, pieces());
  }
  Bitboard attackers_to(Color c, Square sq, const Bitboard &occ) const;
  Bitboard attackers_to(Square sq) const { return attackers_to(sq, pieces()); }
  Bitboard attackers_to(Square sq, const Bitboard &occ) const;

  // 打ち歩詰め判定に使う。王に打ち歩された歩の升をpawn_sqとして、c側(王側)のpawn_sqへ利いている駒を列挙する。香が利いていないことは自明。
  Bitboard attackers_to_pawn(Color c, Square pawn_sq) const;

  // attackers_to()で駒があればtrueを返す版。(利きの情報を持っているなら、軽い実装に変更できる)
  // kingSqの地点からは玉を取り除いての利きの判定を行なう。
  bool effected_to(Color c, Square sq) const {
    return attackers_to(c, sq, pieces());
  }
  bool effected_to(Color c, Square sq, Square kingSq) const {
    return attackers_to(c, sq, pieces() ^ kingSq);
  }

  // 升sに対して、c側の大駒に含まれる長い利きを持つ駒の利きを遮っている駒のBitboardを返す(先後の区別なし)
  // ※　Stockfishでは、sildersを渡すようになっているが、大駒のcolorを渡す実装のほうが優れているので変更。
  // [Out]
  // pinnersとは、pinされている駒が取り除かれたときに升sに利きが発生する大駒である。これは返し値。
  // また、升sにある玉は~c側のKINGであるとする。
  Bitboard slider_blockers(Color c, Square s, Bitboard &pinners) const;

  // --- 局面を進める/戻す

  // 指し手で局面を1手進める
  // m = 指し手。mとして非合法手を渡してはならない。
  // info =
  // StateInfo。局面を進めるときに捕獲した駒などを保存しておくためのバッファ。
  // このバッファはこのdo_move()の呼び出し元の責任において確保されている必要がある。
  // givesCheck = mの指し手によって王手になるかどうか。
  // この呼出までにst.checkInfo.update(pos)が呼び出されている必要がある。
  void do_move(Move m, StateInfo &newSt, bool givesCheck);

  // do_move()の4パラメーター版のほうを呼び出すにはgivesCheckも渡さないといけないが、
  // mで王手になるかどうかがわからないときはこちらの関数を用いる。
  void do_move(Move m, StateInfo &newSt) { do_move(m, newSt, gives_check(m)); }

  // 指し手で局面を1手戻す
  void undo_move(Move m);

  // null move用のdo_move()
  void do_null_move(StateInfo &st);
  // null move用のundo_move()
  void undo_null_move();

  // --- legality(指し手の合法性)のチェック

  // 生成した指し手(CAPTUREとかNON_CAPTUREとか)が、合法であるかどうかをテストする。
  //
  // 指し手生成で合法手であるか判定が漏れている項目についてチェックする。
  // 王手のかかっている局面についてはEVASION(回避手)で指し手が生成されているはずなので
  // ここでは王手のかかっていない局面における合法性のチェック。
  // 具体的には、
  //  1) 移動させたときに素抜きに合わないか
  //  2) 敵の利きのある場所への王の移動でないか
  // ※　連続王手の千日手などについては探索の問題なのでこの関数のなかでは行わない。
  // ※　それ以上のテストは行わないので、置換表から取ってきた指し手などについては、
  // pseudo_legal()を用いて、そのあとこの関数で判定すること。
  bool legal(Move m) const;

  // mがpseudo_legalな指し手であるかを判定する。
  // ※　pseudo_legalとは、擬似合法手(自殺手が含まれていて良い)
  // 置換表の指し手でdo_move()して良いのかの事前判定のために使われる。
  // 指し手生成ルーチンのテストなどにも使える。(指し手生成ルーチンはpseudo_legalな指し手を返すはずなので)
  // killerのような兄弟局面の指し手がこの局面において合法かどうかにも使う。
  // ※　置換表の検査だが、pseudo_legal()で擬似合法手かどうかを判定したあとlegal()で自殺手でないことを
  // 確認しなくてはならない。このためpseudo_legal()とlegal()とで重複する自殺手チェックはしていない。
  bool pseudo_legal(const Move m) const { return pseudo_legal_s<true>(m); }

  // All == false        : 歩や大駒の不成に対してはfalseを返すpseudo_legal()
  template <bool All> bool pseudo_legal_s(const Move m) const;

  // toの地点に歩を打ったときに打ち歩詰めにならないならtrue。
  // 歩をtoに打つことと、二歩でないこと、toの前に敵玉がいることまでは確定しているものとする。
  // 二歩の判定もしたいなら、legal_pawn_drop()のほうを使ったほうがいい。
  bool legal_drop(const Square to) const;

  // 二歩でなく、かつ打ち歩詰めでないならtrueを返す。
  bool legal_pawn_drop(const Color us, const Square to) const {
    return !((pieces(us, PAWN) & FILE_BB[file_of(to)]) // 二歩
             || ((pawnEffect(us, to) == Bitboard(king_square(~us)) &&
                  !legal_drop(to)))); // 打ち歩詰め
  }

  // --- StateInfo

  // 現在の局面に対応するStateInfoを返す。
  // たとえば、state()->capturedPieceであれば、前局面で捕獲された駒が格納されている。
  StateInfo *state() const { return st; }

  // 指し手mで王手になるかを判定する。
  // 前提条件 : 指し手mはpseudo-legal(擬似合法)の指し手であるものとする。
  // (つまり、mのfromにある駒は自駒であることは確定しているものとする。)
  bool gives_check(Move m) const;

  // 現局面で指し手がないかをテストする。指し手生成ルーチンを用いるので速くない。探索中には使わないこと。
  bool is_mated() const;

  // --- Accessing hash keys

  // StateInfo::key()への簡易アクセス。
  Key key() const { return st->key(); }

  // --- misc

  // 現局面で王手がかかっているか
  bool in_check() const { return checkers(); }

  // 開始局面からこの局面にいたるまでの指し手を表示する。
  std::string moves_from_start(bool is_pretty) const;

  // 局面を出力する。(USI形式ではない) デバッグ用。
  friend std::ostream &operator<<(std::ostream &os, const Position &pos);

private:
  // StateInfoの初期化(初期化するときに内部的に用いる)
  void set_state(StateInfo *si) const;

  // 王手になるbitboard等を更新する。set_state()とdo_move()のときに自動的に行われる。
  // null moveのときは利きの更新を少し端折れるのでフラグを渡すことに。
  template <bool doNullMove> void set_check_info(StateInfo *si) const;

  // do_move()の先後分けたもの。内部的に呼び出される。
  template <Color Us> void do_move_impl(Move m, StateInfo &st, bool givesCheck);

  // undo_move()の先後分けたもの。内部的に呼び出される。
  template <Color Us> void undo_move_impl(Move m);

  // --- Bitboards

  // 盤上の先手/後手/両方の駒があるところが1であるBitboard
  Bitboard byColorBB[COLOR_NB];

  // 駒が存在する升を表すBitboard。先後混在。
  // pieces()の引数と同じく、ALL_PIECES,HDKなどのPieceで定義されている特殊な定数が使える。
  Bitboard byTypeBB[PIECE_BB_NB];

  // put_piece()やremove_piece()、xor_piece()を用いたときは、最後にupdate_bitboards()を呼び出して
  // bitboardの整合性を保つこと。
  void put_piece(Square sq, Piece pc);

  // 駒を盤面から取り除き、内部的に保持しているBitboardも更新する。
  void remove_piece(Square sq);

  // sqの地点にpcを置く/取り除く、したとして内部で保持しているBitboardを更新する。
  // 最後にupdate_bitboards()を呼び出すこと。
  void xor_piece(Square sq, Piece pc);

  // put_piece(),remove_piece(),xor_piece()を用いたあとに呼び出す必要がある。
  void update_bitboards();

  // このクラスが保持しているkingSquare[]の更新
  void update_kingSquare();

  // 盤面、25升分の駒
  Piece board[SQ_NB];

  // 手駒
  Hand hand[COLOR_NB];

  // 手番
  Color sideToMove;

  // 玉の位置
  Square kingSquare[COLOR_NB];

  // 初期局面からの手数(初期局面 == 1)
  int gamePly;

  StateInfo *st;
};

inline void Position::xor_piece(Square sq, Piece pc) {
  // 先手・後手の駒のある場所を示すoccupied bitboardの更新
  byColorBB[color_of(pc)] ^= sq;

  // 先手 or 後手の駒のある場所を示すoccupied bitboardの更新
  byTypeBB[ALL_PIECES] ^= sq;

  // 駒別のBitboardの更新
  // これ以外のBitboardの更新は、update_bitboards()で行なう。
  byTypeBB[type_of(pc)] ^= sq;
}

// 駒を配置して、内部的に保持しているBitboardも更新する。
inline void Position::put_piece(Square sq, Piece pc) {
  ASSERT_LV2(board[sq] == NO_PIECE);
  board[sq] = pc;
  xor_piece(sq, pc);
}

// 駒を盤面から取り除き、内部的に保持しているBitboardも更新する。
inline void Position::remove_piece(Square sq) {
  Piece pc = board[sq];
  ASSERT_LV3(pc != NO_PIECE);
  board[sq] = NO_PIECE;
  xor_piece(sq, pc);
}

inline bool is_ok(Position &pos) { return pos.pos_is_ok(); }

// 盤面を出力する。(USI形式ではない) デバッグ用。
std::ostream &operator<<(std::ostream &os, const Position &pos);

#endif
