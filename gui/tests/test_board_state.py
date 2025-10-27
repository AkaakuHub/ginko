"""BoardState unit tests for move validation and drops."""

from __future__ import annotations

from pathlib import Path
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))


from ginko_gui.main import BoardState, Piece  # noqa: E402  pylint: disable=wrong-import-position


@pytest.fixture()
def board_state() -> BoardState:
    state = BoardState()
    state.reset()
    return state


def test_pawn_drop_forbidden_on_final_rank(board_state: BoardState) -> None:
    board_state.hands["b"]["P"] = 1
    board_state.side_to_move = "b"
    with pytest.raises(ValueError):
        board_state.apply_move("P*1a")


def test_pawn_drop_forbidden_double_pawn(board_state: BoardState) -> None:
    board_state.board["1c"] = Piece("b", "P")
    board_state.hands["b"]["P"] = 1
    board_state.side_to_move = "b"
    with pytest.raises(ValueError):
        board_state.apply_move("P*1d")


def test_pawn_must_promote_on_last_rank(board_state: BoardState) -> None:
    board_state.board.clear()
    board_state.hands["b"].clear()
    board_state.board["1b"] = Piece("b", "P")
    board_state.side_to_move = "b"
    board_state.apply_move("1b1a")
    piece = board_state.board["1a"]
    assert piece is not None
    assert piece.kind == "+P"


def test_pawn_promotion_applies(board_state: BoardState) -> None:
    board_state.board.clear()
    board_state.hands["b"].clear()
    board_state.board["2b"] = Piece("b", "P")
    board_state.side_to_move = "b"
    board_state.apply_move("2b2a+")
    piece = board_state.board["2a"]
    assert piece is not None
    assert piece.kind == "+P"
    assert board_state.side_to_move == "w"
