"""5x5将棋GUI本体。盤面描画とUSIエンジン連携を担当。"""

from __future__ import annotations

import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional, Tuple

from PySide6.QtCore import QPointF, QRectF, Qt, QSize, Signal, QTimer
from PySide6.QtGui import QColor, QFont, QPainter, QPen, QBrush, QLinearGradient, QPainterPath
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSlider,
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

    def repetition_key(self) -> tuple:
        board_items = tuple(
            sorted((coord, piece.color, piece.kind) for coord, piece in self.board.items())
        )
        hand_items = (
            tuple(sorted(self.hands["b"].items())),
            tuple(sorted(self.hands["w"].items())),
        )
        return board_items, hand_items, self.side_to_move

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
            self._draw_piece(painter, rect, piece)

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

    def _draw_piece(self, painter: QPainter, rect: QRectF, piece: Piece) -> None:
        painter.save()
        if piece.color == "w":
            center = rect.center()
            painter.translate(center)
            painter.rotate(180)
            painter.translate(-center)

        path = QPainterPath()
        width = rect.width()
        height = rect.height()
        top_margin = height * 0.08
        shoulder_offset = width * 0.18
        shoulder_height = height * 0.32
        flank_offset = width * 0.08
        bottom_margin = height * 0.06

        top_point = QPointF(rect.center().x(), rect.top() + top_margin)
        upper_left = QPointF(rect.left() + shoulder_offset, rect.top() + shoulder_height)
        lower_left = QPointF(rect.left() + flank_offset, rect.bottom() - bottom_margin)
        lower_right = QPointF(rect.right() - flank_offset, rect.bottom() - bottom_margin)
        upper_right = QPointF(rect.right() - shoulder_offset, rect.top() + shoulder_height)

        path.moveTo(top_point)
        path.lineTo(upper_right)
        path.lineTo(lower_right)
        path.lineTo(lower_left)
        path.lineTo(upper_left)
        path.closeSubpath()

        gradient = QLinearGradient(rect.topLeft(), rect.bottomLeft())
        gradient.setColorAt(0.0, QColor(250, 236, 210))
        gradient.setColorAt(0.45, QColor(234, 205, 150))
        gradient.setColorAt(1.0, QColor(198, 160, 110))

        outline_color = QColor(120, 90, 50)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setBrush(QBrush(gradient))
        painter.setPen(QPen(outline_color, max(1.0, width * 0.035)))
        painter.drawPath(path)

        inner_rect = rect.adjusted(width * 0.08, height * 0.05, -width * 0.08, -height * 0.05)
        label = KANJI_MAP.get(piece.kind, piece.kind)
        text_color = Qt.black if piece.color == "b" else QColor(120, 40, 40)
        text_font = QFont(self.font())
        text_font.setPointSizeF(height * 0.44)
        text_font.setBold(True)
        painter.setFont(text_font)
        painter.setPen(text_color)
        painter.drawText(inner_rect, Qt.AlignCenter, label)

        painter.restore()

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
        self.audio_assets_dir = self._default_audio_dir()
        self.audio_assets_dir.mkdir(parents=True, exist_ok=True)
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
        self.game_over = False
        self.position_counts: Counter = Counter()
        self.position_history: list[tuple] = []
        self.ai_vs_ai_mode = False
        self.engine_depth = 3
        self.engine_randomness = 200
        self.ai_turn_delay_ms = 1000
        self._pending_ai_start: Optional[str] = None

        self.engine_client = EngineClient(EngineConfig(executable=self._default_engine_path()))
        self.engine_client.line_received.connect(self._handle_engine_line)
        self.engine_client.error_occurred.connect(self._handle_engine_error)
        self.engine_client.process_exited.connect(self._handle_engine_exit)

        self._reset_position_history()
        self._build_ui()
        self._configure_audio_assets()
        self._start_engine()

    def _is_engine_controlled(self, color: str) -> bool:
        if color == self.ENGINE_COLOR:
            return True
        return self.ai_vs_ai_mode

    def _format_actor_label(self, color: str) -> str:
        base = "先手" if color == self.HUMAN_COLOR else "後手"
        if self._is_engine_controlled(color):
            return f"{base}AI"
        if color == self.HUMAN_COLOR:
            return f"{base}（あなた）"
        return base

    def _format_ai_delay_text(self) -> str:
        seconds = self.ai_turn_delay_ms / 1000
        return f"AIターン間隔: {seconds:.1f}秒"

    def _format_randomness_text(self) -> str:
        return f"AIランダム性: ±{self.engine_randomness}点"

    def _format_depth_text(self) -> str:
        return f"探索深さ: {self.engine_depth}手先"

    def _update_ai_delay_display(self) -> None:
        if hasattr(self, "ai_delay_label"):
            self.ai_delay_label.setText(self._format_ai_delay_text())
        if hasattr(self, "ai_delay_slider"):
            self.ai_delay_slider.setEnabled(self.ai_vs_ai_mode)

    def _handle_ai_delay_changed(self, value: int) -> None:
        self.ai_turn_delay_ms = max(0, int(value))
        self._update_ai_delay_display()

    def _handle_depth_changed(self, value: int) -> None:
        self.engine_depth = max(1, int(value))
        if hasattr(self, "depth_label"):
            self.depth_label.setText(self._format_depth_text())

    def _handle_randomness_changed(self, value: int) -> None:
        self.engine_randomness = max(0, int(value))
        if hasattr(self, "randomness_label"):
            self.randomness_label.setText(self._format_randomness_text())

    def _update_player_controls(self) -> None:
        human_turn_available = not self._is_engine_controlled(self.HUMAN_COLOR) and not self.game_over
        can_interact = human_turn_available and not self.awaiting_engine_move
        self.board_widget.setEnabled(can_interact)
        self.sente_hand.setEnabled(can_interact)
        self.cancel_drop_button.setEnabled(can_interact and self.selected_drop_kind is not None)
        self.resign_button.setEnabled(not self.ai_vs_ai_mode and not self.game_over)
        self._update_ai_delay_display()

    def _maybe_start_engine_turn(self) -> None:
        if (
            not self.usi_ready
            or self.game_over
            or self.awaiting_engine_move
            or self.waiting_legal_moves
        ):
            return

        side = self.board_state.side_to_move
        if not self._is_engine_controlled(side):
            self._update_player_controls()
            return

        if not self.legal_moves:
            # legalmovesレスポンスが空なら終局判定を試みる
            self._check_game_over_conditions()
            return

        if self._pending_ai_start is not None:
            return

        delay = self.ai_turn_delay_ms if self.ai_vs_ai_mode else 0
        if delay > 0:
            self._pending_ai_start = side
            seconds = delay / 1000
            actor = self._format_actor_label(side)
            self.statusBar().showMessage(f"{actor}の思考開始を{seconds:.1f}秒待機…")
            QTimer.singleShot(delay, lambda: self._handle_ai_delay_expired(side))
            return

        self._begin_engine_search(side)

    def _handle_ai_delay_expired(self, side: str) -> None:
        if self._pending_ai_start != side:
            return
        self._pending_ai_start = None
        self._begin_engine_search(side)

    def _begin_engine_search(self, side: str) -> None:
        if self._pending_ai_start is not None and self._pending_ai_start != side:
            return
        self._pending_ai_start = None
        if (
            not self.usi_ready
            or self.game_over
            or self.waiting_legal_moves
        ):
            return
        if self.board_state.side_to_move != side:
            return
        if not self._is_engine_controlled(side):
            self._update_player_controls()
            return
        if not self.legal_moves:
            self._check_game_over_conditions()
            return

        self._clear_drop_selection()
        self.selected_square = None
        self.board_widget.set_selection(None, drop_mode=False)
        self.awaiting_engine_move = True
        self.pending_user_move = None
        self._update_player_controls()
        self._sync_engine_position()
        self.engine_client.send_line(
            f"go depth {self.engine_depth} random {self.engine_randomness}"
        )
        actor = self._format_actor_label(side)
        self.statusBar().showMessage(f"{actor}の思考中…")

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

        self.cancel_drop_button = QPushButton("持ち駒選択をキャンセル")
        self.cancel_drop_button.setEnabled(False)
        self.cancel_drop_button.clicked.connect(self._handle_cancel_drop)
        left_panel.addWidget(self.cancel_drop_button)

        root.addLayout(left_panel, stretch=3)

        right_panel = QVBoxLayout()
        right_panel.setSpacing(8)

        self.new_game_button = QPushButton("新規対局")
        self.new_game_button.clicked.connect(self._handle_new_game)
        self.ai_mode_button = QPushButton("AI同士対局モード: OFF")
        self.ai_mode_button.clicked.connect(self._handle_toggle_ai_mode)
        self.resign_button = QPushButton("投了")
        self.resign_button.clicked.connect(self._handle_resign)

        right_panel.addWidget(self.new_game_button)
        right_panel.addWidget(self.ai_mode_button)
        right_panel.addWidget(self.resign_button)

        self.depth_label = QLabel(self._format_depth_text())
        self.depth_label.setAlignment(Qt.AlignLeft)
        self.depth_slider = QSlider(Qt.Horizontal)
        self.depth_slider.setRange(1, 8)
        self.depth_slider.setSingleStep(1)
        self.depth_slider.setPageStep(1)
        self.depth_slider.valueChanged.connect(self._handle_depth_changed)
        self.depth_slider.setValue(self.engine_depth)
        right_panel.addWidget(self.depth_label)
        right_panel.addWidget(self.depth_slider)

        self.randomness_label = QLabel(self._format_randomness_text())
        self.randomness_label.setAlignment(Qt.AlignLeft)
        self.randomness_slider = QSlider(Qt.Horizontal)
        self.randomness_slider.setRange(0, 2000)
        self.randomness_slider.setSingleStep(50)
        self.randomness_slider.setPageStep(100)
        self.randomness_slider.setTickInterval(200)
        self.randomness_slider.setTickPosition(QSlider.TicksBelow)
        self.randomness_slider.valueChanged.connect(self._handle_randomness_changed)
        self.randomness_slider.setValue(self.engine_randomness)
        right_panel.addWidget(self.randomness_label)
        right_panel.addWidget(self.randomness_slider)

        self.ai_delay_label = QLabel(self._format_ai_delay_text())
        self.ai_delay_label.setAlignment(Qt.AlignLeft)
        self.ai_delay_slider = QSlider(Qt.Horizontal)
        self.ai_delay_slider.setRange(0, 5000)
        self.ai_delay_slider.setSingleStep(100)
        self.ai_delay_slider.setPageStep(500)
        self.ai_delay_slider.setTickInterval(500)
        self.ai_delay_slider.setTickPosition(QSlider.TicksBelow)
        self.ai_delay_slider.valueChanged.connect(self._handle_ai_delay_changed)
        self.ai_delay_slider.setValue(self.ai_turn_delay_ms)
        right_panel.addWidget(self.ai_delay_label)
        right_panel.addWidget(self.ai_delay_slider)

        self.check_indicator = QLabel()
        self.check_indicator.setAlignment(Qt.AlignCenter)
        self.check_indicator.setStyleSheet("font-weight: bold;")
        right_panel.addWidget(self.check_indicator)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        self.info_view = QTextEdit()
        self.info_view.setReadOnly(True)
        log_header = QHBoxLayout()
        log_header.setContentsMargins(0, 0, 0, 0)
        log_header.setSpacing(6)
        log_label = QLabel("ログ")
        self.clear_log_button = QPushButton("ログをクリア")
        self.clear_log_button.clicked.connect(self._handle_clear_log)
        log_header.addWidget(log_label)
        log_header.addStretch(1)
        log_header.addWidget(self.clear_log_button)
        right_panel.addLayout(log_header)
        right_panel.addWidget(self.log_view, stretch=1)
        right_panel.addWidget(QLabel("思考情報"))
        right_panel.addWidget(self.info_view, stretch=1)

        root.addLayout(right_panel, stretch=2)
        self.setCentralWidget(central)
        self.statusBar().showMessage("エンジン初期化中…")

        self._update_check_indicator()
        self._refresh_views()
        self._update_player_controls()

    def _default_engine_path(self) -> Path:
        project_root = Path(__file__).resolve().parents[3]
        debug_path = project_root / "engine" / "target" / "debug" / "engine"
        if debug_path.exists():
            return debug_path
        release_path = project_root / "engine" / "target" / "release" / "engine"
        return release_path

    def _default_audio_dir(self) -> Path:
        project_root = Path(__file__).resolve().parents[3]
        return project_root / "gui" / "assets" / "audio"

    def _configure_audio_assets(self) -> None:
        greeting = self.audio_assets_dir / "greeting.mp3"
        if greeting.exists():
            self.audio_manager.set_voice_sound("game_start", greeting)

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

    def _handle_toggle_ai_mode(self) -> None:
        enable = not self.ai_vs_ai_mode
        if enable and self.move_history and not self.game_over:
            reply = QMessageBox.question(
                self,
                "AI同士対局モード",
                "進行中の対局を終了してAI同士の自動対局を開始しますか？",
                QMessageBox.Yes | QMessageBox.No,
            )
            if reply != QMessageBox.Yes:
                return

        self.ai_vs_ai_mode = enable
        self.ai_mode_button.setText("AI同士対局モード: ON" if enable else "AI同士対局モード: OFF")
        self._append_info(f"info string ai_vs_ai_mode={'on' if enable else 'off'}")
        self._pending_ai_start = None

        if enable:
            self._handle_new_game()
        else:
            self._update_player_controls()
            if self.usi_ready:
                self._request_legal_moves()
            self._refresh_views()

    def _handle_new_game(self) -> None:
        self.board_state.reset()
        self.move_history.clear()
        self.selected_square = None
        self._clear_drop_selection()
        self.awaiting_engine_move = False
        self.pending_user_move = None
        self.legal_moves.clear()
        self.waiting_legal_moves = False
        self.in_check = False
        self.game_over = False
        self._pending_ai_start = None
        self._reset_position_history()
        self._update_player_controls()
        self._update_check_indicator()
        if self.usi_ready:
            self.engine_client.send_line("usinewgame")
            self._sync_engine_position()
            self._request_legal_moves()
        self._refresh_views()
        self.info_view.clear()
        self.audio_manager.play_voice("game_start")

    def _handle_resign(self) -> None:
        QMessageBox.information(self, "投了", "先手が投了しました。")
        self.move_history.append("resign")
        self.awaiting_engine_move = False
        self.statusBar().showMessage("対局終了")

    def _handle_drop_selection(self, kind: str) -> None:
        if self.awaiting_engine_move or self.game_over or self._is_engine_controlled(self.HUMAN_COLOR):
            return
        self.selected_drop_kind = kind
        self.selected_square = None
        self.board_widget.set_selection(None, drop_mode=True)
        self.statusBar().showMessage(f"{KANJI_MAP[kind]} を打つ場所を選択")
        self._update_highlight_targets()
        self.cancel_drop_button.setEnabled(True)

    def _handle_board_click(self, coord: str) -> None:
        if (
            self.awaiting_engine_move
            or self.game_over
            or self._is_engine_controlled(self.HUMAN_COLOR)
        ):
            return

        piece = self.board_state.piece_at(coord)
        if self.selected_drop_kind:
            if piece is not None:
                self._append_log("その升には打てません")
                return
            move = f"{self.selected_drop_kind}*{coord}"
            if not self._apply_human_move(move):
                return
            self._clear_drop_selection()
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

    def _handle_cancel_drop(self) -> None:
        if (
            not self.selected_drop_kind
            or self._is_engine_controlled(self.HUMAN_COLOR)
        ):
            return
        self._clear_drop_selection()
        self.statusBar().showMessage("持ち駒の選択を解除しました")

    def _clear_drop_selection(self) -> None:
        had_drop = self.selected_drop_kind is not None
        self.selected_drop_kind = None
        if hasattr(self, "cancel_drop_button"):
            self.cancel_drop_button.setEnabled(False)
        if had_drop:
            self.board_widget.set_selection(None, drop_mode=False)
        self._update_highlight_targets()

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
        self._update_player_controls()
        self.audio_manager.play_move_sound()
        self._append_log(f"{self._format_actor_label(self.HUMAN_COLOR)}: {move}")
        self._refresh_views()
        self._record_position()
        if self.game_over:
            return True

        self._sync_engine_position()
        self.engine_client.send_line(
            f"go depth {self.engine_depth} random {self.engine_randomness}"
        )
        self.statusBar().showMessage(f"{self._format_actor_label(self.ENGINE_COLOR)}の思考中…")
        return True

    def _refresh_views(self) -> None:
        self.board_widget.set_board_state(self.board_state)
        self.gote_hand.update_counts(self.board_state.hand_counts(self.ENGINE_COLOR))
        self.sente_hand.update_counts(self.board_state.hand_counts(self.HUMAN_COLOR))
        self._update_highlight_targets()
        if not self.awaiting_engine_move:
            side = self.board_state.side_to_move
            self.statusBar().showMessage(f"{self._format_actor_label(side)}の番です")
        self._update_player_controls()

    def _handle_clear_log(self) -> None:
        self.log_view.clear()
        self.statusBar().showMessage("ログをクリアしました")

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
            self.info_view.clear()
            self._request_legal_moves()
            self._refresh_views()
            self.audio_manager.play_voice("game_start")
            return
        if line.startswith("info string position error"):
            self._append_info(line)
            self._append_log(line)
            self.waiting_legal_moves = False
            self._pending_ai_start = None
            if self.pending_user_move and self.move_history and self.move_history[-1] == self.pending_user_move:
                self.move_history.pop()
                self.board_state.load_history(self.move_history)
                self._rebuild_position_history()
                self.game_over = False
                self._clear_drop_selection()
                self.awaiting_engine_move = False
                self.pending_user_move = None
                self._sync_engine_position()
                self._refresh_views()
                self._update_player_controls()
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

        moving_color = self.board_state.side_to_move
        self._pending_ai_start = None
        self.awaiting_engine_move = False
        self.pending_user_move = None
        if move == "resign":
            label = self._format_actor_label(moving_color)
            self._append_log(f"{label}が投了しました")
            self.statusBar().showMessage(f"{label}が投了しました")
            self.game_over = True
            self._update_player_controls()
            return
        try:
            self.board_state.apply_move(move)
        except ValueError as exc:
            self._append_log(f"エンジン手適用エラー: {exc}")
            return
        self.move_history.append(move)
        self._append_log(f"{self._format_actor_label(moving_color)}: {move}")
        self.audio_manager.play_move_sound()
        self._refresh_views()
        self._record_position()
        if self.game_over:
            return
        self._request_legal_moves()
        self._check_game_over_conditions()
        self._maybe_start_engine_turn()

    def _handle_engine_error(self, text: str) -> None:
        self._append_log(f"[ERR] {text}")

    def _handle_engine_exit(self, code: int) -> None:
        self._append_log(f"エンジンが終了しました (code={code})")
        self._append_info(f"info string engine exited code={code}")
        self.awaiting_engine_move = False
        self.pending_user_move = None
        self.waiting_legal_moves = False
        self._pending_ai_start = None
        self._update_player_controls()
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
        self._check_game_over_conditions()
        self._maybe_start_engine_turn()

    def _handle_checkstate(self, line: str) -> None:
        parts = line.split(maxsplit=1)
        value = parts[1].strip().lower() if len(parts) > 1 else ""
        self.in_check = value in {"1", "true", "yes"}
        self._update_check_indicator()
        self._check_game_over_conditions()

    def _request_legal_moves(self) -> None:
        if (
            not self.usi_ready
            or self.awaiting_engine_move
            or self.waiting_legal_moves
            or self.game_over
        ):
            return
        self.legal_moves = []
        self._update_highlight_targets()
        self.engine_client.send_line("legalmoves")
        self.waiting_legal_moves = True

    def _reset_position_history(self) -> None:
        self.position_counts.clear()
        self.position_history.clear()
        key = self.board_state.repetition_key()
        self.position_counts[key] = 1
        self.position_history.append(key)

    def _record_position(self) -> None:
        if self.game_over:
            return
        key = self.board_state.repetition_key()
        self.position_history.append(key)
        self.position_counts[key] += 1
        if self.position_counts[key] >= 4:
            self._handle_repetition()

    def _rebuild_position_history(self) -> None:
        temp_state = BoardState()
        self.position_counts.clear()
        self.position_history.clear()
        key = temp_state.repetition_key()
        self.position_counts[key] = 1
        self.position_history.append(key)
        for move in self.move_history:
            try:
                temp_state.apply_move(move)
            except ValueError:
                break
            key = temp_state.repetition_key()
            self.position_history.append(key)
            self.position_counts[key] += 1

    def _check_game_over_conditions(self) -> None:
        if self.game_over or self.waiting_legal_moves:
            return
        if self.legal_moves:
            return

        side = self.board_state.side_to_move
        if self.in_check:
            self._handle_checkmate(side)
        else:
            self._handle_no_legal_moves(side)

    def _handle_checkmate(self, loser: str) -> None:
        winner = opponent(loser)
        if loser == self.HUMAN_COLOR and not self.ai_vs_ai_mode:
            self._finalize_game(
                "詰まされました",
                "詰み: あなたの負けです。",
                "詰み",
                "詰みです。あなたの負けです。",
            )
            return
        if loser == self.ENGINE_COLOR and not self.ai_vs_ai_mode:
            self._finalize_game(
                "詰ませました",
                "詰み: あなたの勝ちです。",
                "詰み",
                "おめでとうございます。あなたの勝ちです。",
            )
            return

        loser_label = self._format_actor_label(loser)
        winner_label = self._format_actor_label(winner)
        self._finalize_game(
            f"{winner_label}の勝ちです",
            f"詰み: {winner_label}の勝ちです。",
            "詰み",
            f"{winner_label}が{loser_label}を詰ませました。",
        )

    def _handle_no_legal_moves(self, loser: str) -> None:
        winner = opponent(loser)
        if loser == self.HUMAN_COLOR and not self.ai_vs_ai_mode:
            self._finalize_game(
                "合法手がありません",
                "合法手がないため、あなたの負けです。",
                "手詰まり",
                "合法手がないため、あなたの負けです。",
            )
            return
        if loser == self.ENGINE_COLOR and not self.ai_vs_ai_mode:
            self._finalize_game(
                "合法手がありません",
                "合法手がないため、あなたの勝ちです。",
                "手詰まり",
                "合法手がないため、あなたの勝ちです。",
            )
            return

        loser_label = self._format_actor_label(loser)
        winner_label = self._format_actor_label(winner)
        self._finalize_game(
            f"{winner_label}の勝ちです",
            f"合法手がないため、{winner_label}の勝ちです。",
            "手詰まり",
            f"{winner_label}が{loser_label}を手詰まりにしました。",
        )

    def _handle_repetition(self) -> None:
        loser = self.HUMAN_COLOR
        winner = opponent(loser)
        if not self.ai_vs_ai_mode:
            self._finalize_game(
                "千日手で負けました",
                "千日手: 先手の負けです。",
                "千日手",
                "千日手が成立したため、あなたの負けです。",
            )
            return

        loser_label = self._format_actor_label(loser)
        winner_label = self._format_actor_label(winner)
        self._finalize_game(
            f"千日手: {winner_label}の勝ち",
            f"千日手: {winner_label}の勝ちです。",
            "千日手",
            f"千日手が成立したため、{loser_label}の負けです。",
        )

    def _finalize_game(
        self,
        status_message: str,
        log_message: str,
        dialog_title: str,
        dialog_message: str,
    ) -> None:
        if self.game_over:
            return
        self.game_over = True
        self.awaiting_engine_move = False
        self.pending_user_move = None
        self.selected_square = None
        self._clear_drop_selection()
        self.waiting_legal_moves = False
        self._pending_ai_start = None
        self.board_widget.set_selection(None, drop_mode=False)
        self.board_widget.set_highlight_targets([])
        self.statusBar().showMessage(status_message)
        self._append_log(log_message)
        self._update_player_controls()
        QMessageBox.information(self, dialog_title, dialog_message)

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
