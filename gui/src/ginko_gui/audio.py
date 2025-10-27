"""pygame.mixerを用いたノンブロッキング音声再生管理。"""

from __future__ import annotations

from pathlib import Path
from typing import Optional

import pygame.mixer


class AudioManager:
    """GUI全体で共有する音声再生コンポーネント。"""

    def __init__(self) -> None:
        self._initialized = False
        self._move_sound: Optional[pygame.mixer.Sound] = None
        self._move_sound_path: Optional[Path] = None

    def initialize(self) -> None:
        """mixerを初期化する。複数回呼び出しても安全。"""

        if self._initialized:
            return
        pygame.mixer.init()
        self._initialized = True
        if self._move_sound_path is not None:
            self.set_move_sound(self._move_sound_path)

    def shutdown(self) -> None:
        if not self._initialized:
            return
        pygame.mixer.quit()
        self._initialized = False
        self._move_sound = None

    def set_move_sound(self, path: Optional[Path]) -> None:
        """指し手再生に用いる音声ファイルを設定する。"""

        self._move_sound_path = path
        if not self._initialized:
            return
        if path and path.exists():
            self._move_sound = pygame.mixer.Sound(path.as_posix())
        else:
            self._move_sound = None

    def play_move_sound(self) -> None:
        """bestmove受信時などに呼び出される再生メソッド。"""

        if not self._initialized:
            return
        if self._move_sound is not None:
            self._move_sound.play()


__all__ = ["AudioManager"]
