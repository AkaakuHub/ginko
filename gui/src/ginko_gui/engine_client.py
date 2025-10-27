"""USIエンジンとの非同期通信をQtイベントループに統合するクライアント。"""

from __future__ import annotations

import subprocess
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from PySide6.QtCore import QObject, Signal


@dataclass(slots=True)
class EngineConfig:
    executable: Path
    arguments: list[str] = field(default_factory=list)

    def command(self) -> list[str]:
        return [self.executable.as_posix(), *self.arguments]


class EngineClient(QObject):
    """USIエンジンを子プロセスとして管理し、標準入出力を行う。"""

    line_received = Signal(str)
    error_occurred = Signal(str)
    process_exited = Signal(int)

    def __init__(self, config: EngineConfig, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._config = config
        self._process: Optional[subprocess.Popen[str]] = None
        self._stdout_thread: Optional[threading.Thread] = None
        self._stderr_thread: Optional[threading.Thread] = None
        self._monitor_thread: Optional[threading.Thread] = None
        self._write_lock = threading.Lock()

    def start(self) -> None:
        if self._process is not None:
            return

        if not self._config.executable.exists():
            raise FileNotFoundError(f"Engine not found: {self._config.executable}")

        command = self._config.command()
        self._process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        self._stdout_thread = threading.Thread(target=self._read_stdout_loop, daemon=True)
        self._stdout_thread.start()

        self._stderr_thread = threading.Thread(target=self._read_stderr_loop, daemon=True)
        self._stderr_thread.start()

        self._monitor_thread = threading.Thread(target=self._wait_for_exit, daemon=True)
        self._monitor_thread.start()

    def _read_stdout_loop(self) -> None:
        assert self._process is not None and self._process.stdout is not None
        for line in self._process.stdout:
            if not line:
                break
            self.line_received.emit(line.rstrip())

    def _read_stderr_loop(self) -> None:
        assert self._process is not None and self._process.stderr is not None
        for line in self._process.stderr:
            if not line:
                break
            self.error_occurred.emit(line.rstrip())

    def _wait_for_exit(self) -> None:
        assert self._process is not None
        self._process.wait()
        code = self._process.returncode or 0
        self.process_exited.emit(code)

    def send_line(self, line: str) -> None:
        if self._process is None or self._process.stdin is None:
            raise RuntimeError("Engine process is not running")
        with self._write_lock:
            self._process.stdin.write(line + "\n")
            self._process.stdin.flush()

    def stop(self) -> None:
        if self._process is None:
            return
        try:
            self.send_line("quit")
        except RuntimeError:
            pass
        self._process.terminate()
        self._process.wait(timeout=5)
        self._process = None


__all__ = ["EngineConfig", "EngineClient"]
