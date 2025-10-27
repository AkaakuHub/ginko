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
        self._voice_sounds: dict[str, pygame.mixer.Sound] = {}
        self._voice_paths: dict[str, Path] = {}

    def initialize(self) -> None:
        """mixerを初期化する。複数回呼び出しても安全。"""

        if self._initialized:
            return
        pygame.mixer.init()
        self._initialized = True
        if self._move_sound_path is not None:
            self.set_move_sound(self._move_sound_path)
        if self._voice_paths:
            for event, path in list(self._voice_paths.items()):
                self.set_voice_sound(event, path)

    def shutdown(self) -> None:
        if not self._initialized:
            return
        pygame.mixer.quit()
        self._initialized = False
        self._move_sound = None
        self._voice_sounds.clear()

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

    def set_voice_sound(self, event: str, path: Optional[Path]) -> None:
        """イベントに紐づくボイス音声を登録する。"""

        if path is None:
            self._voice_paths.pop(event, None)
            self._voice_sounds.pop(event, None)
            return

        self._voice_paths[event] = path
        if not self._initialized:
            return
        if path.exists():
            self._voice_sounds[event] = pygame.mixer.Sound(path.as_posix())
        else:
            self._voice_sounds.pop(event, None)

    def play_voice(self, event: str) -> None:
        """登録済みイベントのボイスを再生する。"""

        if not self._initialized:
            return
        sound = self._voice_sounds.get(event)
        if sound is not None:
            sound.play()


__all__ = ["AudioManager"]
