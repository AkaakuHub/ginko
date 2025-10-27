#include "../types.h"

// USI拡張コマンドのうち、開発上のテスト関係のコマンド。
// 思考エンジンの実行には関係しない。GitHubにはcommitしないかも。

#include <unordered_set>
#include <cmath>               // sqrt() , fabs()
#include <sstream>

#include "../position.h"
#include "../misc.h"

using namespace std;

// ----------------------------------
//      USI拡張コマンド "test"
// ----------------------------------

// ランダムプレイヤーで行なうテスト

void random_player(Position& pos, uint64_t loop_max)
{
	const int MAX_PLY = 256; // 256手までテスト

	StateInfo state[MAX_PLY]; // StateInfoを最大手数分だけ
	Move moves[MAX_PLY]; // 局面の巻き戻し用に指し手を記憶
	int ply; // 初期局面からの手数

	StateInfo si;
	pos.set_hirate(&si);

	PRNG prng(20201130);

	for (uint64_t i = 0; i < loop_max; ++i)
	{
		for (ply = 0; ply < MAX_PLY; ++ply)
		{
			MoveList<LEGAL_ALL> mg(pos); // 全合法手の生成

			// 合法な指し手がなかった == 詰み
			if (mg.size() == 0)
				break;

			// 局面がおかしくなっていないかをテストする
			ASSERT_LV3(is_ok(pos));

			// ここで生成された指し手がすべて合法手であるかテストをする
			for (auto m : mg)
			{
				ASSERT_LV3(pos.pseudo_legal(m));
				ASSERT_LV2(pos.legal(m));
			}

			// 生成された指し手のなかからランダムに選び、その指し手で局面を進める。
			Move m = mg.begin()[prng.rand(mg.size())];

			pos.do_move(m, state[ply]);
			moves[ply] = m;

		}

		// 局面を巻き戻してみる(undo_moveの動作テストを兼ねて)
		while (ply > 0)
		{
			pos.undo_move(moves[--ply]);
		}

		// 1000回に1回ごとに'.'を出力(進んでいることがわかるように)
		if ((i % 1000) == 0)
			cout << ".";
	}
}

// ランダムプレイヤー(指し手をランダムに選ぶプレイヤー)による自己対戦テスト
// これを1000万回ほどまわせば、指し手生成などにバグがあればどこかで引っかかるはず。

void random_player_cmd(Position& pos, istringstream& is)
{
	uint64_t loop_max = 100000000; // 1億回
	is >> loop_max;
	cout << "Random Player test , loop_max = " << loop_max << endl;
	random_player(pos, loop_max);
	cout << "finished." << endl;
}
