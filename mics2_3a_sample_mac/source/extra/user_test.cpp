#include "../types.h"
#include "../misc.h"
#include "../position.h"

// userコマンドのテスト関数
void user_test(Position& pos, std::istringstream& is)
{
  StateInfo si;
  ExtMove m = MoveList<LEGAL_ALL>(pos).at(0);
  pos.do_move(m.move, si);
  std::cout << m.move << std::endl;
  std::cout << pos << std::endl;

  pos.undo_move(m.move);
  std::cout << pos << std::endl;
}
