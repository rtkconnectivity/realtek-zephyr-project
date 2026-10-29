"""
Claude Code Monitor UI
Three status panels with animated lights + Yes/No buttons + confirm-mode gate.

Status colors:
  Idle    → gray
  Working → blue, slow pulse   (tool executing)
  Waiting → yellow, fast pulse (PermissionRequest OR gate pending)
  Success → green              (Stop — auto-resets after 10 s)

State files: ~/.claude/monitor/state_*.json  (written by state_writer.py)
Gate files:  ~/.claude/monitor/gate_*.json   (written here + by gate_hook.py)
"""
import json
import math
import subprocess
import sys
import time
import tkinter as tk
from enum import Enum
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

MONITOR_DIR     = Path.home() / ".claude" / "monitor"
POLL_INTERVAL_MS = 300
SUCCESS_RESET_S  = 10
STALE_ACTIVE_S   = 300


# ---------------------------------------------------------------------------
# Status model
# ---------------------------------------------------------------------------

class Status(Enum):
    IDLE    = "idle"
    WORKING = "working"
    WAITING = "waiting"
    SUCCESS = "success"


_STR_TO_STATUS: dict[str, Status] = {s.value: s for s in Status}

PALETTE: dict = {
    "bg":           "#F5F7FA",
    "card":         "#FFFFFF",
    "border":       "#DDE1E7",
    "border_hover": "#1890FF",
    "text":         "#262626",
    "muted":        "#8C8FA3",
    "pending":      "#FA8C16",
    Status.IDLE:    "#C1C7D0",
    Status.WORKING: "#1890FF",
    Status.WAITING: "#FAAD14",
    Status.SUCCESS: "#52C41A",
}

STATUS_TEXT: dict[Status, str] = {
    Status.IDLE:    "空  闲",
    Status.WORKING: "执 行 中",
    Status.WAITING: "等待确认",
    Status.SUCCESS: "任务完成",
}

GLOW: dict[Status, list[str]] = {
    Status.IDLE:    ["#E8EAF0", "#EEF0F5", "#F2F4F8"],
    Status.WORKING: ["#BAD7F9", "#D6EAFF", "#EAF4FF"],
    Status.WAITING: ["#FFE58F", "#FFF1B8", "#FFFBE6"],
    Status.SUCCESS: ["#B7EB8F", "#D9F7BE", "#F6FFED"],
}

_PULSE_SPEED: dict[Status, float] = {
    Status.WORKING: 0.06,
    Status.WAITING: 0.14,
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _mix(hex1: str, hex2: str, t: float) -> str:
    t = max(0.0, min(1.0, t))
    r1, g1, b1 = int(hex1[1:3], 16), int(hex1[3:5], 16), int(hex1[5:7], 16)
    r2, g2, b2 = int(hex2[1:3], 16), int(hex2[3:5], 16), int(hex2[5:7], 16)
    return "#{:02x}{:02x}{:02x}".format(
        round(r1 * t + r2 * (1 - t)),
        round(g1 * t + g2 * (1 - t)),
        round(b1 * t + b2 * (1 - t)),
    )


def _gate_path(key: str) -> Path:
    return MONITOR_DIR / f"gate_{key}.json"


def _read_gate(key: str) -> dict | None:
    try:
        return json.loads(_gate_path(key).read_text(encoding="utf-8"))
    except Exception:
        return None


def _atomic_write(path: Path, data: dict) -> None:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)


def _summarize_pending(pending: dict | None) -> str:
    if not pending:
        return ""
    tool = pending.get("tool", "?")
    inp  = pending.get("input", {})
    if isinstance(inp, dict):
        # Show first key=value pair as a hint
        for k, v in inp.items():
            snippet = str(v)[:60].replace("\n", "↵")
            return f"{tool}  {k}={snippet}"
    return tool


# ---------------------------------------------------------------------------
# Single window panel
# ---------------------------------------------------------------------------

class WindowPanel:
    LIGHT_SIZE   = 90
    RING_OFFSETS = [6, 14, 20]
    CORE_INSET   = 25

    def __init__(self, parent: tk.Widget, index: int):
        self.index        = index
        self.status       = Status.IDLE
        self._pulse_phase = 0.0
        self._pulse_job   = None

        self.border_frame = tk.Frame(parent, bg=PALETTE["border"], padx=2, pady=2)
        self.border_frame.pack(side=tk.LEFT, padx=12, pady=8)

        card = tk.Frame(self.border_frame, bg=PALETTE["card"], padx=22, pady=14)
        card.pack()

        # title
        tk.Label(card, text=f"窗口 {index + 1}",
                 bg=PALETTE["card"], fg=PALETTE["text"],
                 font=("Segoe UI", 13, "bold")).pack()

        # project name
        self.project_var = tk.StringVar(value="—")
        tk.Label(card, textvariable=self.project_var,
                 bg=PALETTE["card"], fg=PALETTE["muted"],
                 font=("Segoe UI", 9)).pack()

        # light canvas
        S = self.LIGHT_SIZE
        self.canvas = tk.Canvas(card, width=S, height=S,
                                bg=PALETTE["card"], highlightthickness=0)
        self.canvas.pack(pady=10)

        self.rings = []
        for off in self.RING_OFFSETS:
            oid = self.canvas.create_oval(off, off, S - off, S - off,
                                          fill=PALETTE["card"], outline="")
            self.rings.append(oid)

        ci = self.CORE_INSET
        self.core = self.canvas.create_oval(ci, ci, S - ci, S - ci,
                                            fill=PALETTE[Status.IDLE], outline="")

        # status label
        self.status_var = tk.StringVar(value=STATUS_TEXT[Status.IDLE])
        self.status_lbl = tk.Label(card, textvariable=self.status_var,
                                   bg=PALETTE["card"], fg=PALETTE["muted"],
                                   font=("Consolas", 10))
        self.status_lbl.pack()

        # tool name (small)
        self.tool_var = tk.StringVar(value="")
        tk.Label(card, textvariable=self.tool_var,
                 bg=PALETTE["card"], fg=PALETTE["muted"],
                 font=("Consolas", 8)).pack()

        # pending info (orange, only when gate has pending)
        self.pending_var = tk.StringVar(value="")
        tk.Label(card, textvariable=self.pending_var,
                 bg=PALETTE["card"], fg=PALETTE["pending"],
                 font=("Segoe UI", 8), wraplength=160).pack()

        # confirm-mode toggle (capsule badge) — default ON
        self.gate_var = tk.BooleanVar(value=True)
        self.gate_badge = tk.Button(
            card,
            text="✓  确认模式",          # initial text matches default=True
            bg="#F6FFED", fg="#52C41A",
            font=("Segoe UI", 9),
            relief="flat", bd=0,
            padx=12, pady=4,
            cursor="hand2",
            activebackground="#D9F7BE",
            activeforeground="#52C41A",
            command=self._toggle_gate_badge,
        )
        self.gate_badge.pack(pady=(6, 0))

        # Yes / No buttons
        btn_row = tk.Frame(card, bg=PALETTE["card"])
        btn_row.pack(pady=(8, 2))

        self.yes_btn = self._btn(btn_row, "✓  Yes", "#1b6ca8", "#2386cc")
        self.yes_btn.pack(side=tk.LEFT, padx=5)

        self.no_btn = self._btn(btn_row, "✗  No", "#8b1a1a", "#b22222")
        self.no_btn.pack(side=tk.LEFT, padx=5)

        # Buttons start disabled; enabled only when gate has a pending decision
        self._set_buttons_enabled(False)

    # ------------------------------------------------------------------

    def _btn(self, parent, text, bg, active_bg):
        return tk.Button(parent, text=text, bg=bg, fg="white",
                         font=("Segoe UI", 10, "bold"),
                         width=7, relief="flat", cursor="hand2",
                         activebackground=active_bg, activeforeground="white",
                         borderwidth=0, disabledforeground="#C1C7D0")

    def _set_buttons_enabled(self, enabled: bool) -> None:
        state = tk.NORMAL if enabled else tk.DISABLED
        self.yes_btn.config(state=state)
        self.no_btn.config(state=state)

    def set_hover_highlight(self, on: bool) -> None:
        self.border_frame.config(bg=PALETTE["border_hover"] if on else PALETTE["border"])

    def _toggle_gate_badge(self) -> None:
        self.gate_var.set(not self.gate_var.get())
        self._refresh_gate_badge()

    def _refresh_gate_badge(self) -> None:
        if self.gate_var.get():
            self.gate_badge.config(
                text="✓  确认模式",
                bg="#F6FFED", fg="#52C41A",
                activebackground="#D9F7BE",
            )
        else:
            self.gate_badge.config(
                text="○  确认模式",
                bg="#F0F2F5", fg=PALETTE["muted"],
                activebackground="#E6E8EC",
            )

    # ------------------------------------------------------------------

    def set_status(self, status: Status, tool: str = "") -> None:
        if self._pulse_job:
            self.canvas.after_cancel(self._pulse_job)
            self._pulse_job = None

        self.status       = status
        self._pulse_phase = 0.0

        self.status_var.set(STATUS_TEXT[status])
        fg = PALETTE[status] if status != Status.IDLE else PALETTE["muted"]
        self.status_lbl.config(fg=fg)

        self.tool_var.set(tool if status in (Status.WORKING, Status.WAITING) else "")

        if status in (Status.WORKING, Status.WAITING):
            self._pulse()
        else:
            self._draw(status, alpha=1.0)

    def update_gate_pending(self, pending_text: str, buttons_enabled: bool) -> None:
        self.pending_var.set(pending_text)
        self._set_buttons_enabled(buttons_enabled)

    def _pulse(self) -> None:
        speed = _PULSE_SPEED.get(self.status, 0.10)
        self._pulse_phase += speed
        alpha = (math.sin(self._pulse_phase) + 1) / 2
        self._draw(self.status, alpha)
        self._pulse_job = self.canvas.after(50, self._pulse)

    def _draw(self, status: Status, alpha: float) -> None:
        self.canvas.itemconfig(self.core, fill=PALETTE[status])
        bg = PALETTE["card"]
        weights = [0.30, 0.55, 0.80]
        for ring_id, gcolor, w in zip(self.rings, GLOW[status], weights):
            self.canvas.itemconfig(ring_id, fill=_mix(gcolor, bg, alpha * w))


# ---------------------------------------------------------------------------
# State reader
# ---------------------------------------------------------------------------

def _read_states() -> dict[str, dict]:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    out: dict[str, dict] = {}
    for f in MONITOR_DIR.glob("state_*.json"):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
            key  = data.get("session_key", f.stem)
            if time.time() - data.get("updated_at", 0) < 86400:
                out[key] = data
        except Exception:
            pass
    return out


# ---------------------------------------------------------------------------
# Main application window
# ---------------------------------------------------------------------------

class ClaudeMonitorUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Claude Code Monitor")
        self.root.configure(bg=PALETTE["bg"])
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        self._session_map: dict[str, int] = {}   # session_key -> panel index

        self._build_header()
        self._build_panels()
        self._build_debug_bar()
        self._build_voice_panel()

        self.root.after(POLL_INTERVAL_MS, self._poll_states)
        self.root.after(500, self._poll_voice_result)

        self._start_device_listener()

    # ------------------------------------------------------------------

    # ------------------------------------------------------------------

    def _start_device_listener(self) -> None:
        """Launch device_listener.py in the background if not already running."""
        import os
        script = Path(__file__).parent / "device_listener.py"
        if not script.exists():
            return

        # Check if a device_listener process is already running (Windows)
        try:
            result = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq python.exe", "/FO", "CSV", "/NH"],
                capture_output=True, text=True
            )
            # Look for device_listener in any python process's window title / cmdline
            # Simpler: use wmic to check command lines
            wmic = subprocess.run(
                ["wmic", "process", "where", "name='python.exe'",
                 "get", "CommandLine", "/FORMAT:CSV"],
                capture_output=True, text=True
            )
            if "device_listener" in wmic.stdout:
                print("[UI] device_listener.py already running, skipping launch")
                return
        except Exception:
            pass  # If check fails, just launch anyway (duplicate is harmless)

        try:
            subprocess.Popen(
                [sys.executable, str(script)],
                creationflags=subprocess.CREATE_NEW_CONSOLE,
            )
            print("[UI] device_listener.py launched")
        except Exception as e:
            print(f"[UI] Failed to launch device_listener.py: {e}")

    def _build_header(self) -> None:
        row = tk.Frame(self.root, bg=PALETTE["bg"])
        row.pack(fill=tk.X, padx=20, pady=(18, 4))
        tk.Label(row, text="Claude Code Monitor",
                 bg=PALETTE["bg"], fg=PALETTE["text"],
                 font=("Segoe UI", 17, "bold")).pack(side=tk.LEFT)

        self._conn_var = tk.StringVar(value="○ 等待连接")
        tk.Label(row, textvariable=self._conn_var,
                 bg=PALETTE["bg"], fg=PALETTE["muted"],
                 font=("Segoe UI", 9)).pack(side=tk.RIGHT, padx=4)

    def _build_panels(self) -> None:
        container = tk.Frame(self.root, bg=PALETTE["bg"])
        container.pack(padx=14, pady=8)

        self.panels: list[WindowPanel] = []
        for i in range(3):
            p = WindowPanel(container, i)
            p.yes_btn.config(command=lambda idx=i: self._on_yes(idx))
            p.no_btn.config( command=lambda idx=i: self._on_no(idx))
            p.gate_var.trace_add("write", lambda *_, idx=i: self._on_gate_toggle(idx))
            self.panels.append(p)

    def _build_debug_bar(self) -> None:
        bar = tk.Frame(self.root, bg=PALETTE["bg"])
        bar.pack(pady=(2, 16))

        tk.Label(bar, text="测试:", bg=PALETTE["bg"], fg=PALETTE["muted"],
                 font=("Segoe UI", 9)).pack(side=tk.LEFT, padx=(0, 6))

        cases = [
            ("W1 执行", lambda: self.panels[0].set_status(Status.WORKING, "Bash"), 0),
            ("W1 等待", lambda: self.panels[0].set_status(Status.WAITING, "Write"), 0),
            ("W1 完成", lambda: self.panels[0].set_status(Status.SUCCESS), 0),
            ("W2 执行", lambda: self.panels[1].set_status(Status.WORKING, "Read"), 1),
            ("全部重置", lambda: [p.set_status(Status.IDLE) for p in self.panels], None),
        ]
        for label, cmd, panel_idx in cases:
            btn = tk.Button(bar, text=label, bg="#F0F2F5", fg=PALETTE["muted"],
                            font=("Segoe UI", 9), relief="flat", cursor="hand2",
                            activebackground="#E6E8EC", activeforeground=PALETTE["text"],
                            command=cmd, padx=8)
            btn.pack(side=tk.LEFT, padx=3)
            if panel_idx is not None:
                btn.bind("<Enter>", lambda e, i=panel_idx: self.panels[i].set_hover_highlight(True))
                btn.bind("<Leave>", lambda e, i=panel_idx: self.panels[i].set_hover_highlight(False))

    # ------------------------------------------------------------------

    def _poll_states(self) -> None:
        try:
            states   = _read_states()
            now      = time.time()
            used_panels: set[int] = set()

            ordered = sorted(states.values(), key=lambda d: d.get("created_at", 0))

            for data in ordered:
                key  = data["session_key"]
                idx  = self._get_panel(key)
                if idx is None:
                    continue
                used_panels.add(idx)

                panel      = self.panels[idx]
                status_str = data.get("status", "idle")
                updated_at = data.get("updated_at", 0)
                age        = now - updated_at

                # Auto-transitions
                if status_str == "success" and age > SUCCESS_RESET_S:
                    status_str = "idle"
                elif status_str in ("working", "waiting") and age > STALE_ACTIVE_S:
                    status_str = "idle"

                new_status = _STR_TO_STATUS.get(status_str, Status.IDLE)
                tool       = data.get("tool", "") if new_status in (Status.WORKING, Status.WAITING) else ""
                project    = data.get("project", "—")

                panel.project_var.set(project)

                # Check gate for this session
                gate           = _read_gate(key)
                gate_enabled   = gate is not None and gate.get("enabled", False)
                gate_pending   = gate.get("pending") if gate else None

                if gate_enabled and gate_pending:
                    # Override to WAITING and show pending info
                    if panel.status != Status.WAITING:
                        panel.set_status(Status.WAITING, gate_pending.get("tool", ""))
                    panel.update_gate_pending(_summarize_pending(gate_pending), True)
                else:
                    # Normal state update
                    if panel.status != new_status or panel.tool_var.get() != tool:
                        panel.set_status(new_status, tool)
                    panel.update_gate_pending("", False)

            # Idle any panel whose session has gone stale / disappeared
            for key, idx in self._session_map.items():
                if idx not in used_panels:
                    panel = self.panels[idx]
                    if panel.status != Status.IDLE:
                        panel.set_status(Status.IDLE)
                        panel.update_gate_pending("", False)

            if states:
                self._conn_var.set(f"● {len(states)} 个会话")
            else:
                self._conn_var.set("○ 等待连接")

        except Exception:
            pass

        self.root.after(POLL_INTERVAL_MS, self._poll_states)

    def _get_panel(self, key: str) -> int | None:
        if key in self._session_map:
            return self._session_map[key]
        used = set(self._session_map.values())
        for i in range(len(self.panels)):
            if i not in used:
                self._session_map[key] = i
                # New session: write gate file with enabled=true so gate_hook
                # starts intercepting immediately, matching the default-ON badge.
                gate = _read_gate(key) or {}
                if not gate.get("enabled", False):
                    gate.setdefault("pending",  None)
                    gate.setdefault("decision", None)
                    gate["enabled"] = True
                    _atomic_write(_gate_path(key), gate)
                return i
        return None

    def _session_key_for_panel(self, idx: int) -> str | None:
        for key, i in self._session_map.items():
            if i == idx:
                return key
        return None

    # ------------------------------------------------------------------

    def _on_yes(self, idx: int) -> None:
        key = self._session_key_for_panel(idx)
        if key is None:
            return
        gate = _read_gate(key)
        if gate is None:
            return
        gate["decision"] = "allow"
        _atomic_write(_gate_path(key), gate)

    def _on_no(self, idx: int) -> None:
        key = self._session_key_for_panel(idx)
        if key is None:
            return
        gate = _read_gate(key)
        if gate is None:
            return
        gate["decision"] = "block"
        gate["reason"]   = "用户在 Monitor UI 中拒绝了此操作"
        _atomic_write(_gate_path(key), gate)

    def _on_gate_toggle(self, idx: int) -> None:
        key = self._session_key_for_panel(idx)
        if key is None:
            return
        enabled = self.panels[idx].gate_var.get()
        gate = _read_gate(key) or {}
        gate["enabled"]  = enabled
        gate["pending"]  = gate.get("pending") if enabled else None
        gate["decision"] = gate.get("decision") if enabled else None
        _atomic_write(_gate_path(key), gate)

    def _on_close(self) -> None:
        # Release any blocked gate hooks before closing
        for key in list(self._session_map.keys()):
            gate = _read_gate(key)
            if gate is not None and gate.get("enabled", False):
                gate["enabled"]  = False
                gate["decision"] = None
                try:
                    _atomic_write(_gate_path(key), gate)
                except Exception:
                    pass
        self.root.destroy()

    # ------------------------------------------------------------------

    _VOICE_PLACEHOLDER = "（等待语音输入）"

    def _build_voice_panel(self) -> None:
        sep = tk.Frame(self.root, bg=PALETTE["border"], height=1)
        sep.pack(fill=tk.X, padx=20, pady=(4, 0))

        panel = tk.Frame(self.root, bg=PALETTE["bg"])
        panel.pack(fill=tk.X, padx=20, pady=(8, 14))

        header = tk.Frame(panel, bg=PALETTE["bg"])
        header.pack(fill=tk.X)

        tk.Label(header, text="🎙  语音指令",
                 bg=PALETTE["bg"], fg=PALETTE["text"],
                 font=("Segoe UI", 11, "bold")).pack(side=tk.LEFT)

        self._voice_status_var = tk.StringVar(value="")
        tk.Label(header, textvariable=self._voice_status_var,
                 bg=PALETTE["bg"], fg=PALETTE["muted"],
                 font=("Segoe UI", 9)).pack(side=tk.RIGHT)

        card = tk.Frame(panel, bg=PALETTE["card"],
                        highlightbackground=PALETTE["border"],
                        highlightthickness=1)
        card.pack(fill=tk.X, pady=(6, 0))

        self._voice_text = tk.Text(
            card,
            bg=PALETTE["card"], fg=PALETTE["muted"],
            font=("Segoe UI", 11),
            relief="flat", bd=0,
            padx=14, pady=10,
            wrap=tk.WORD, height=3,
            insertbackground=PALETTE["text"],
        )
        self._voice_text.pack(fill=tk.X)
        self._voice_text.insert("1.0", self._VOICE_PLACEHOLDER)
        self._voice_text.bind("<FocusIn>", self._on_voice_focus)

        toolbar = tk.Frame(card, bg=PALETTE["card"])
        toolbar.pack(fill=tk.X, padx=10, pady=(0, 8))

        self._voice_copied_var = tk.StringVar(value="")
        tk.Label(toolbar, textvariable=self._voice_copied_var,
                 bg=PALETTE["card"], fg=PALETTE[Status.SUCCESS],
                 font=("Segoe UI", 8)).pack(side=tk.LEFT)

        tk.Button(toolbar, text="清除",
                  bg="#F0F2F5", fg=PALETTE["muted"],
                  font=("Segoe UI", 8), relief="flat", cursor="hand2",
                  activebackground="#E6E8EC", activeforeground=PALETTE["text"],
                  padx=8, pady=2,
                  command=self._clear_voice).pack(side=tk.RIGHT)

        self._last_voice_ts: float = 0.0

    def _on_voice_focus(self, _event) -> None:
        if self._voice_text.get("1.0", tk.END).strip() == self._VOICE_PLACEHOLDER:
            self._voice_text.delete("1.0", tk.END)
            self._voice_text.config(fg=PALETTE["text"])

    def _clear_voice(self) -> None:
        self._voice_text.delete("1.0", tk.END)
        self._voice_text.insert("1.0", self._VOICE_PLACEHOLDER)
        self._voice_text.config(fg=PALETTE["muted"])
        self._voice_copied_var.set("")
        self._voice_status_var.set("")


    def _poll_voice_result(self) -> None:
        try:
            path = MONITOR_DIR / "voice_result.json"
            data = json.loads(path.read_text(encoding="utf-8"))
            ts   = data.get("updated_at", 0.0)
            if ts != self._last_voice_ts:
                self._last_voice_ts = ts
                self._voice_text.delete("1.0", tk.END)
                self._voice_text.insert("1.0", data.get("text", ""))
                self._voice_text.config(fg=PALETTE["text"])
                self._voice_copied_var.set("✓ 已复制到剪贴板")
                self._voice_status_var.set(
                    time.strftime("%H:%M:%S", time.localtime(ts))
                )
        except Exception:
            pass
        self.root.after(500, self._poll_voice_result)

    def run(self) -> None:
        self.root.mainloop()


# ---------------------------------------------------------------------------

if __name__ == "__main__":
    ClaudeMonitorUI().run()
