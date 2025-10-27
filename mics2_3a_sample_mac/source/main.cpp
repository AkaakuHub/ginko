#include "search.h"
#include "usi.h"


int main(int argc, char *argv[]) {
  // --- 全体的な初期化
  Bitboards::init();
  Position::init();
  Search::init();

  // USIコマンドの応答部
  USI::loop(argc, argv);
}
