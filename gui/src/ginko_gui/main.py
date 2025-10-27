"""5x5将棋GUI本体。盤面描画とUSIエンジン連携を担当。"""

from __future__ import annotations

import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional, Tuple

from PySide6.QtCore import QPointF, QRectF, Qt, QSize, Signal, Slot
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from .audio import AudioManager
from .engine_client import EngineClient, EngineConfig


BOARD_FILES = 5
BOARD_RANKS = 5
FILE_VALUES = [5, 4, 3, 2, 1]
RANK_VALUES = ["a", "b", "c", "d", "e"]

PROMOTE_MAP = {"P": "+P", "S": "+S", "B": "+B", "R": "+R"}
DEMOTE_MAP = {v: k for k, v in PROMOTE_MAP.items()}
KANJI_MAP = {
    "K": "玉",
    "G": "金",
    "S": "銀",
    "B": "角",
    "R": "飛",
    "P": "歩",
    "+P": "と",
    "+S": "全",
    "+B": "馬",
    "+R": "竜",
}
PIECE_ORDER = ["R", "B", "G", "S", "P"]
PROMOTABLE = {"P", "S", "B", "R"}


def opponent(color: str) -> str:
    return "w" if color == "b" else "b"


def coord_to_indices(coord: str) -> Tuple[int, int]:
    file_value = int(coord[0])
    rank_value = coord[1]
    col = BOARD_FILES - file_value
    row = ord(rank_value) - ord("a")
    return row, col


def indices_to_coord(row: int, col: int) -> str:
    file_value = BOARD_FILES - col
    rank_value = chr(ord("a") + row)
    return f"{file_value}{rank_value}"


@dataclass
class Piece:
    color: str  # 'b' or 'w'
    kind: str   # e.g. 'P', '+P'


class BoardState:
    """盤面と持ち駒の状態を管理する。"""

    def __init__(self) -> None:
        self.board: dict[str, Piece] = {}
        self.hands: dict[str, Counter[str]] = {
            "b": Counter(),
            "w": Counter(),
        }
        self.side_to_move: str = "b"
        self.last_move: Optional[Tuple[Optional[str], str]] = None
        self.reset()

    def reset(self) -> None:
        self.board.clear()
        self.hands["b"].clear()
        self.hands["w"].clear()
        self.side_to_move = "b"
        self.last_move = None
        self._load_from_sfen("rbsgk/4p/5/P4/KGSBR b - 1")

    def load_history(self, moves: list[str]) -> None:
        self.reset()
        for move in moves:
            self.apply_move(move)

    def apply_move(self, move: str) -> None:
        if move == "resign":
            self.side_to_move = opponent(self.side_to_move)
            self.last_move = None
            return

        side = self.side_to_move
        if "*" in move:
            piece_char, dest = move.split("*")
            piece_kind = piece_char.upper()
            if piece_kind == "P":
                if self._is_promotion_rank(dest, side):
                    raise ValueError("歩は最終段に打てません")
                if self._has_pawn_on_file(side, dest[0]):
                    raise ValueError("二歩は禁止です")
            if self.hands[side][piece_kind] <= 0:
                raise ValueError("指定の持ち駒がありません")
            if dest in self.board:
                raise ValueError("移動先が空いていません")
            self.hands[side][piece_kind] -= 1
            if self.hands[side][piece_kind] == 0:
                del self.hands[side][piece_kind]
            self.board[dest] = Piece(color=side, kind=piece_kind)
            self.last_move = (None, dest)
        else:
            promote = move.endswith("+")
            from_sq = move[:2]
            to_sq = move[2:4]
            piece = self.board.get(from_sq)
            if piece is None:
                raise ValueError("移動元に駒がありません")
            if piece.color != side:
                raise ValueError("相手の駒は動かせません")
            target = self.board.get(to_sq)
            if target and target.color == side:
                raise ValueError("自駒の上には移動できません")

            del self.board[from_sq]

            if target:
                captured_kind = self._demote_kind(target.kind)
                self.hands[side][captured_kind] += 1

            base = self._base_kind(piece.kind)
            must_promote = base == "P" and self._is_promotion_rank(to_sq, side)
            if promote and base not in PROMOTABLE:
                raise ValueError("この駒は成れません")
            if must_promote:
                promote = True
            if promote and base in PROMOTABLE:
                piece.kind = PROMOTE_MAP.get(base, piece.kind)

            self.board[to_sq] = piece
            self.last_move = (from_sq, to_sq)

        self.side_to_move = opponent(side)

    def piece_at(self, coord: str) -> Optional[Piece]:
        return self.board.get(coord)

    def hand_counts(self, color: str) -> Counter[str]:
        return self.hands[color]

    @staticmethod
    def _base_kind(kind: str) -> str:
        return DEMOTE_MAP.get(kind, kind)

    @staticmethod
    def _demote_kind(kind: str) -> str:
        return DEMOTE_MAP.get(kind, kind)

    @staticmethod
    def _is_promotion_rank(coord: str, color: str) -> bool:
        rank = coord[1]
        if color == "b":
            return rank == "a"
        return rank == "e"

    def _has_pawn_on_file(self, color: str, file_char: str) -> bool:
        for coord, piece in self.board.items():
            if piece.color == color and self._base_kind(piece.kind) == "P" and coord[0] == file_char:
                return True
        return False

    def _load_from_sfen(self, sfen: str) -> None:
        board_part, turn_part, hand_part, _ply = sfen.split()
        ranks = board_part.split("/")
        for rank_index, rank_str in enumerate(ranks):
            file_value = BOARD_FILES
            i = 0
            while i < len(rank_str):
                ch = rank_str[i]
                if ch.isdigit():
                    file_value -= int(ch)
                    i += 1
                    continue
                promoted = False
                if ch == "+":
                    promoted = True
                    i += 1
                    ch = rank_str[i]
                color = "b" if ch.isupper() else "w"
                kind = ch.upper()
                if promoted:
                    kind = f"+{kind}"
                coord = f"{file_value}{RANK_VALUES[rank_index]}"
                self.board[coord] = Piece(color=color, kind=kind)
                file_value -= 1
                i += 1

        self.side_to_move = "b" if turn_part.lower() == "b" else "w"
        if hand_part != "-":
            count_buffer = ""
            for ch in hand_part:
                if ch.isdigit():
                    count_buffer += ch
                    continue
                count = int(count_buffer) if count_buffer else 1
                count_buffer = ""
                color = "b" if ch.isupper() else "w"
                kind = ch.upper()
                self.hands[color][kind] += count


class BoardWidget(QWidget):
    """5x5盤面を描画し、クリックイベントを通知する。"""

    square_clicked = Signal(str)

    def __init__(self, state: BoardState, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._state = state
        self._selected_square: Optional[str] = None
        self._drop_mode = False
        self._highlight_targets: set[str] = set()
        self.setMinimumSize(QSize(480, 480))

    def set_board_state(self, state: BoardState) -> None:
        self._state = state
        self.update()

    def set_selection(self, square: Optional[str], drop_mode: bool) -> None:
        self._selected_square = square
        self._drop_mode = drop_mode
        self.update()

    def set_highlight_targets(self, targets: Iterable[str]) -> None:
        self._highlight_targets = set(targets)
        self.update()

    def paintEvent(self, event) -> None:  # type: ignore[override]
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        margin = 30
        available_width = self.width() - margin * 2
        available_height = self.height() - margin * 2
        square = min(available_width / BOARD_FILES, available_height / BOARD_RANKS)
        total_width = square * BOARD_FILES
        total_height = square * BOARD_RANKS
        left = (self.width() - total_width) / 2
        top = (self.height() - total_height) / 2

        light = QColor(237, 203, 151)
        dark = QColor(203, 163, 102)

        for row in range(BOARD_RANKS):
            for col in range(BOARD_FILES):
                rect = QRectF(left + col * square, top + row * square, square, square)
                color = light if (row + col) % 2 == 0 else dark
                painter.fillRect(rect, color)

                painter.setPen(QPen(Qt.black, 1))
                painter.drawRect(rect)

        if self._state.last_move:
            from_sq, to_sq = self._state.last_move
            highlight = QColor(255, 230, 150, 180)
            for coord in filter(None, [from_sq, to_sq]):
                row, col = coord_to_indices(coord)  # type: ignore[arg-type]
                rect = QRectF(left + col * square, top + row * square, square, square)
                painter.fillRect(rect, highlight)

        if self._highlight_targets:
            target_fill = QColor(120, 220, 150, 120)
            target_outline = QPen(QColor(30, 120, 60), 2, Qt.DashLine)
            painter.setPen(target_outline)
            for coord in self._highlight_targets:
                row, col = coord_to_indices(coord)
                rect = QRectF(left + col * square, top + row * square, square, square)
                painter.fillRect(rect, target_fill)
                painter.drawRect(rect)
            painter.setPen(QPen(Qt.black, 1))

        if self._selected_square:
            row, col = coord_to_indices(self._selected_square)
            rect = QRectF(left + col * square, top + row * square, square, square)
            painter.fillRect(rect, QColor(120, 180, 255, 120))

        font = QFont(self.font())
        font.setPointSizeF(square * 0.4)
        painter.setFont(font)

        for coord, piece in self._state.board.items():
            row, col = coord_to_indices(coord)
            rect = QRectF(left + col * square, top + row * square, square, square)
            label = KANJI_MAP.get(piece.kind, piece.kind)
            painter.setPen(Qt.black if piece.color == "b" else Qt.darkRed)
            painter.drawText(rect, Qt.AlignCenter, label)

        painter.setPen(Qt.black)
        font_small = QFont(self.font())
        font_small.setPointSizeF(square * 0.25)
        painter.setFont(font_small)

        for idx, file_value in enumerate(FILE_VALUES):
            text = str(file_value)
            x = left + (BOARD_FILES - idx - 0.5) * square
            painter.drawText(QPointF(x - square * 0.05, top - square * 0.1), text)
        for idx, rank_value in enumerate(RANK_VALUES):
            text = rank_value
            y = top + (idx + 0.6) * square
            painter.drawText(QPointF(left - square * 0.2, y), text)

    def mousePressEvent(self, event) -> None:  # type: ignore[override]
        if event.button() != Qt.LeftButton:
            return
        margin = 30
        available_width = self.width() - margin * 2
        available_height = self.height() - margin * 2
        square = min(available_width / BOARD_FILES, available_height / BOARD_RANKS)
        total_width = square * BOARD_FILES
        total_height = square * BOARD_RANKS
        left = (self.width() - total_width) / 2
        top = (self.height() - total_height) / 2

        x = event.position().x()
        y = event.position().y()
        if not (left <= x <= left + total_width and top <= y <= top + total_height):
            return
        col = int((x - left) // square)
        row = int((y - top) // square)
        col = min(max(col, 0), BOARD_FILES - 1)
        row = min(max(row, 0), BOARD_RANKS - 1)
        coord = indices_to_coord(row, col)
        self.square_clicked.emit(coord)


class HandWidget(QWidget):
    """持ち駒表示および選択のためのウィジェット。"""

    piece_selected = Signal(str)

    def __init__(self, color: str, selectable: bool, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._color = color
        self._selectable = selectable
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        self._buttons: dict[str, QPushButton] = {}
        for kind in PIECE_ORDER:
            label = KANJI_MAP[kind]
            button = QPushButton(f"{label} x0")
            button.setEnabled(selectable)
            if selectable:
                button.clicked.connect(lambda _=False, k=kind: self.piece_selected.emit(k))
            layout.addWidget(button)
            self._buttons[kind] = button
        layout.addStretch(1)

    def update_counts(self, counts: Counter[str]) -> None:
        for kind, button in self._buttons.items():
            count = counts.get(kind, 0)
            label = KANJI_MAP[kind]
            button.setText(f"{label} x{count}")
            if self._selectable:
                button.setEnabled(count > 0)


class MainWindow(QMainWindow):
    HUMAN_COLOR = "b"
    ENGINE_COLOR = "w"

    def __init__(self, audio_manager: AudioManager) -> None:
        super().__init__()
        self.audio_manager = audio_manager
        self.board_state = BoardState()
        self.move_history: list[str] = []
        self.selected_square: Optional[str] = None
        self.selected_drop_kind: Optional[str] = None
        self.awaiting_engine_move = False
        self.pending_user_move: Optional[str] = None
        self.usi_ready = False
        self.legal_moves: list[str] = []
        self.waiting_legal_moves = False
        self.in_check = False

        self.engine_client = EngineClient(EngineConfig(executable=self._default_engine_path()))
        self.engine_client.line_received.connect(self._handle_engine_line)
        self.engine_client.error_occurred.connect(self._handle_engine_error)
        self.engine_client.process_exited.connect(self._handle_engine_exit)

        self._build_ui()
        self._start_engine()

    def _build_ui(self) -> None:
        self.setWindowTitle("Ginko 5x5 Shogi")
        self.resize(1100, 720)

        central = QWidget(self)
        root = QHBoxLayout(central)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(18)

        left_panel = QVBoxLayout()
        left_panel.setSpacing(8)

        self.gote_hand = HandWidget(self.ENGINE_COLOR, selectable=False)
        left_panel.addWidget(QLabel("後手持ち駒"))
        left_panel.addWidget(self.gote_hand)

        self.board_widget = BoardWidget(self.board_state)
        self.board_widget.square_clicked.connect(self._handle_board_click)
        left_panel.addWidget(self.board_widget, stretch=1)

        self.sente_hand = HandWidget(self.HUMAN_COLOR, selectable=True)
        self.sente_hand.piece_selected.connect(self._handle_drop_selection)
        left_panel.addWidget(QLabel("先手持ち駒"))
        left_panel.addWidget(self.sente_hand)

        root.addLayout(left_panel, stretch=3)

        right_panel = QVBoxLayout()
        right_panel.setSpacing(8)

        self.new_game_button = QPushButton("新規対局")
        self.new_game_button.clicked.connect(self._handle_new_game)
        self.resign_button = QPushButton("投了")
        self.resign_button.clicked.connect(self._handle_resign)

        right_panel.addWidget(self.new_game_button)
        right_panel.addWidget(self.resign_button)
        self.check_indicator = QLabel()
        self.check_indicator.setAlignment(Qt.AlignCenter)
        self.check_indicator.setStyleSheet("font-weight: bold;")
        right_panel.addWidget(self.check_indicator)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        self.info_view = QTextEdit()
        self.info_view.setReadOnly(True)
        right_panel.addWidget(QLabel("ログ"))
        right_panel.addWidget(self.log_view, stretch=1)
        right_panel.addWidget(QLabel("思考情報"))
        right_panel.addWidget(self.info_view, stretch=1)

        root.addLayout(right_panel, stretch=2)
        self.setCentralWidget(central)
        self.statusBar().showMessage("エンジン初期化中…")

        self._update_check_indicator()
        self._refresh_views()

    def _default_engine_path(self) -> Path:
        project_root = Path(__file__).resolve().parents[3]
        debug_path = project_root / "engine" / "target" / "debug" / "engine"
        if debug_path.exists():
            return debug_path
        release_path = project_root / "engine" / "target" / "release" / "engine"
        return release_path

    def _start_engine(self) -> None:
        try:
            self.engine_client.start()
        except FileNotFoundError as exc:
            QMessageBox.critical(self, "エラー", f"エンジンが見つかりません: {exc}")
            self.new_game_button.setEnabled(False)
            self.resign_button.setEnabled(False)
            self.board_widget.setEnabled(False)
            return

        self.engine_client.send_line("usi")

    def closeEvent(self, event) -> None:  # type: ignore[override]
        self.engine_client.stop()
        super().closeEvent(event)

    def _handle_new_game(self) -> None:
        self.board_state.reset()
        self.move_history.clear()
        self.selected_square = None
        self.selected_drop_kind = None
        self.awaiting_engine_move = False
        self.pending_user_move = None
        self.legal_moves.clear()
        self.waiting_legal_moves = False
        self.in_check = False
        self._refresh_views()
        self._update_check_indicator()
        if self.usi_ready:
            self.engine_client.send_line("usinewgame")
            self._sync_engine_position()
            self.statusBar().showMessage("先手番です")
            self._request_legal_moves()
        self.info_view.clear()

    def _handle_resign(self) -> None:
        QMessageBox.information(self, "投了", "先手が投了しました。")
        self.move_history.append("resign")
        self.awaiting_engine_move = False
        self.statusBar().showMessage("対局終了")

    def _handle_drop_selection(self, kind: str) -> None:
        if self.awaiting_engine_move:
            return
        self.selected_drop_kind = kind
        self.selected_square = None
        self.board_widget.set_selection(None, drop_mode=True)
        self.statusBar().showMessage(f"{KANJI_MAP[kind]} を打つ場所を選択")
        self._update_highlight_targets()

    def _handle_board_click(self, coord: str) -> None:
        if self.awaiting_engine_move:
            return

        piece = self.board_state.piece_at(coord)
        if self.selected_drop_kind:
            if piece is not None:
                self._append_log("その升には打てません")
                return
            move = f"{self.selected_drop_kind}*{coord}"
            if not self._apply_human_move(move):
                return
            self.selected_drop_kind = None
            self.board_widget.set_selection(None, drop_mode=False)
            self._update_highlight_targets()
            return

        if self.selected_square is None:
            if piece and piece.color == self.HUMAN_COLOR:
                self.selected_square = coord
                self.board_widget.set_selection(coord, drop_mode=False)
                self.statusBar().showMessage(f"{coord} から移動する先を選択")
                self._update_highlight_targets()
            return

        if coord == self.selected_square:
            self.selected_square = None
            self.board_widget.set_selection(None, drop_mode=False)
            self.statusBar().showMessage("選択を解除しました")
            self._update_highlight_targets()
            return

        move = self._build_move_string(self.selected_square, coord)
        if move is None:
            return
        self.selected_square = None
        self.board_widget.set_selection(None, drop_mode=False)
        self._update_highlight_targets()
        self._apply_human_move(move)

    def _build_move_string(self, from_sq: str, to_sq: str) -> Optional[str]:
        piece = self.board_state.piece_at(from_sq)
        if piece is None or piece.color != self.HUMAN_COLOR:
            self._append_log("自駒を選択してください")
            return None
        target = self.board_state.piece_at(to_sq)
        if target and target.color == self.HUMAN_COLOR:
            self._append_log("自駒の上には移動できません")
            return None

        base = BoardState._base_kind(piece.kind)
        promote = False
        if base == "P" and self._is_promotion_rank(to_sq, self.HUMAN_COLOR):
            promote = True
        elif base in PROMOTABLE and (
            self._is_promotion_rank(from_sq, self.HUMAN_COLOR)
            or self._is_promotion_rank(to_sq, self.HUMAN_COLOR)
        ):
            promote = (
                QMessageBox.question(
                    self,
                    "成り",
                    "成りますか？",
                    QMessageBox.Yes | QMessageBox.No,
                )
                == QMessageBox.Yes
            )

        move = f"{from_sq}{to_sq}"
        if promote:
            move += "+"
        return move

    def _update_highlight_targets(self) -> None:
        targets: set[str] = set()
        if self.selected_drop_kind:
            prefix = f"{self.selected_drop_kind.upper()}*"
            for move in self.legal_moves:
                if move.startswith(prefix):
                    dest = move.split("*", 1)[1][:2]
                    targets.add(dest)
        elif self.selected_square:
            prefix = self.selected_square
            for move in self.legal_moves:
                if move.startswith(prefix):
                    targets.add(move[2:4])
        self.board_widget.set_highlight_targets(sorted(targets))

    def _apply_human_move(self, move: str) -> bool:
        try:
            self.board_state.apply_move(move)
        except ValueError as exc:
            self._append_log(f"無効な手です: {exc}")
            return False

        self.move_history.append(move)
        self.pending_user_move = move
        self.awaiting_engine_move = True
        self.audio_manager.play_move_sound()
        self._append_log(f"先手: {move}")
        self._refresh_views()

        self._sync_engine_position()
        self.engine_client.send_line("go depth 3")
        self.statusBar().showMessage("後手の思考中…")
        return True

    def _refresh_views(self) -> None:
        self.board_widget.set_board_state(self.board_state)
        self.gote_hand.update_counts(self.board_state.hand_counts(self.ENGINE_COLOR))
        self.sente_hand.update_counts(self.board_state.hand_counts(self.HUMAN_COLOR))
        self._update_highlight_targets()
        turn_text = "先手番" if self.board_state.side_to_move == self.HUMAN_COLOR else "後手番"
        if not self.awaiting_engine_move:
            self.statusBar().showMessage(f"{turn_text}です")

    def _handle_engine_line(self, line: str) -> None:
        if line.startswith("id"):
            return
        if line == "usiok":
            self.engine_client.send_line("isready")
            return
        if line == "readyok":
            self.usi_ready = True
            self.engine_client.send_line("usinewgame")
            self._sync_engine_position()
            self.statusBar().showMessage("先手番です")
            self.info_view.clear()
            self._request_legal_moves()
            return
        if line.startswith("info string position error"):
            self._append_info(line)
            self._append_log(line)
            self.waiting_legal_moves = False
            if self.pending_user_move and self.move_history and self.move_history[-1] == self.pending_user_move:
                self.move_history.pop()
                self.board_state.load_history(self.move_history)
                self.awaiting_engine_move = False
                self.pending_user_move = None
                self._sync_engine_position()
                self._refresh_views()
                self.statusBar().showMessage("手を指し直してください")
                self._request_legal_moves()
            return
        if line.startswith("info "):
            if line.startswith("info string legalmoves error"):
                self.waiting_legal_moves = False
            self._append_info(line)
            return
        if line.startswith("legalmoves"):
            self._handle_legalmoves_response(line)
            return
        if line.startswith("checkstate"):
            self._handle_checkstate(line)
            return
        if line.startswith("bestmove"):
            move = line.split(" ", 1)[1].strip()
            self._handle_bestmove(move)
            return

        self._append_log(f"<< {line}")

    def _handle_bestmove(self, move: str) -> None:
        if not self.awaiting_engine_move:
            # Ignore stray responses, e.g. when the previous position command failed.
            self._append_log(f"bestmoveを無視しました: {move}")
            return

        self.awaiting_engine_move = False
        self.pending_user_move = None
        if move == "resign":
            self._append_log("後手が投了しました")
            self.statusBar().showMessage("後手が投了しました")
            return
        try:
            self.board_state.apply_move(move)
        except ValueError as exc:
            self._append_log(f"エンジン手適用エラー: {exc}")
            return
        self.move_history.append(move)
        self._append_log(f"後手: {move}")
        self.audio_manager.play_move_sound()
        self._refresh_views()
        self._request_legal_moves()

    def _handle_engine_error(self, text: str) -> None:
        self._append_log(f"[ERR] {text}")

    def _handle_engine_exit(self, code: int) -> None:
        self._append_log(f"エンジンが終了しました (code={code})")
        self._append_info(f"info string engine exited code={code}")
        self.board_widget.setEnabled(False)
        self.sente_hand.setEnabled(False)
        self.awaiting_engine_move = False
        self.pending_user_move = None
        self.waiting_legal_moves = False
        self.statusBar().showMessage("エンジン停止")

    def _append_log(self, message: str) -> None:
        self.log_view.append(message)

    def _append_info(self, message: str) -> None:
        self.info_view.append(message)

    def _sync_engine_position(self) -> None:
        if not self.usi_ready:
            return
        if self.move_history:
            moves_text = " ".join(self.move_history)
            self.engine_client.send_line(f"position startpos moves {moves_text}")
        else:
            self.engine_client.send_line("position startpos")

    def _handle_legalmoves_response(self, line: str) -> None:
        parts = line.split()
        moves = parts[1:] if len(parts) > 1 else []
        self.legal_moves = moves
        self.waiting_legal_moves = False
        self._update_highlight_targets()

    def _handle_checkstate(self, line: str) -> None:
        parts = line.split(maxsplit=1)
        value = parts[1].strip().lower() if len(parts) > 1 else ""
        self.in_check = value in {"1", "true", "yes"}
        self._update_check_indicator()

    def _request_legal_moves(self) -> None:
        if not self.usi_ready or self.awaiting_engine_move or self.waiting_legal_moves:
            return
        self.engine_client.send_line("legalmoves")
        self.waiting_legal_moves = True

    def _update_check_indicator(self) -> None:
        if not hasattr(self, "check_indicator"):
            return
        if self.in_check:
            self.check_indicator.setText("王手: 受けています")
            self.check_indicator.setStyleSheet("font-weight: bold; color: #c62828;")
        else:
            self.check_indicator.setText("王手: かかっていません")
            self.check_indicator.setStyleSheet("font-weight: bold; color: #2e7d32;")

    @staticmethod
    def _is_promotion_rank(coord: str, color: str) -> bool:
        rank = coord[1]
        if color == "b":
            return rank == "a"
        return rank == "e"


def main() -> int:
    app = QApplication(sys.argv)
    audio_manager = AudioManager()
    try:
        audio_manager.initialize()
    except Exception as exc:  # pylint: disable=broad-except
        sys.stderr.write(f"[warn] audio init failed: {exc}\n")

    window = MainWindow(audio_manager)
    window.show()
    exit_code = app.exec()
    audio_manager.shutdown()
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
