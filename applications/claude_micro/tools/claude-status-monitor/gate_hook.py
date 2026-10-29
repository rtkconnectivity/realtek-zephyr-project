#!/usr/bin/env python3
"""
Claude Code PreToolUse gate hook.

Reads gate_{key}.json; if enabled, writes the pending tool info and polls
for a UI decision (allow / block) before Claude Code proceeds.

Timeout: auto-allow after GATE_TIMEOUT_S seconds.
Output: JSON {"decision": "block", "reason": "..."} on stdout to block,
        or exit 0 silently to allow.

Claude Code reads stdout from PreToolUse hooks — any non-JSON output
(or no output) means "allow".
"""
import hashlib
import json
import os
import sys
import time
from pathlib import Path

MONITOR_DIR    = Path.home() / ".claude" / "monitor"
GATE_TIMEOUT_S = 60      # auto-allow after this many seconds
POLL_INTERVAL  = 0.15    # seconds between gate file polls


def _session_key() -> str:
    project_dir = os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd())
    return hashlib.md5(project_dir.encode()).hexdigest()[:8]


def _gate_path(key: str) -> Path:
    return MONITOR_DIR / f"gate_{key}.json"


def _read_gate(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None


def _atomic_write(path: Path, data: dict) -> None:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)


def _truncate_input(data: dict, max_chars: int = 200) -> dict:
    """Return input dict with long string values truncated."""
    out = {}
    for k, v in data.items():
        if isinstance(v, str) and len(v) > max_chars:
            out[k] = v[:max_chars] + f"…(+{len(v) - max_chars})"
        else:
            out[k] = v
    return out


def main() -> None:
    # Read the hook payload from stdin (Claude Code sends JSON)
    payload: dict = {}
    try:
        raw = sys.stdin.read()
        if raw.strip():
            payload = json.loads(raw)
    except Exception:
        pass

    key       = _session_key()
    gate_path = _gate_path(key)

    gate = _read_gate(gate_path)
    if gate is None or not gate.get("enabled", False):
        # Gate not enabled — allow immediately
        return

    tool_name  = payload.get("tool_name", payload.get("tool", ""))
    input_data = payload.get("tool_input", payload.get("input", {}))

    # Write pending info into gate file
    pending = {
        "tool":         tool_name,
        "input":        _truncate_input(input_data) if isinstance(input_data, dict) else input_data,
        "requested_at": time.time(),
    }
    gate["pending"]  = pending
    gate["decision"] = None
    _atomic_write(gate_path, gate)

    # Poll for decision
    deadline = time.time() + GATE_TIMEOUT_S
    while time.time() < deadline:
        time.sleep(POLL_INTERVAL)
        gate = _read_gate(gate_path)
        if gate is None:
            break  # file removed — allow

        # UI toggled gate off while waiting → allow
        if not gate.get("enabled", False):
            break

        decision = gate.get("decision")
        if decision == "allow":
            # Clear pending so UI goes back to normal
            gate["pending"]  = None
            gate["decision"] = None
            _atomic_write(gate_path, gate)
            return  # exit 0 = allow

        if decision == "block":
            reason = gate.get("reason", f"用户在 UI 中拒绝了 {tool_name}")
            gate["pending"]  = None
            gate["decision"] = None
            _atomic_write(gate_path, gate)
            # Return block decision to Claude Code on stdout
            print(json.dumps({"decision": "block", "reason": reason},
                             ensure_ascii=False))
            return

    # Timeout or gate disabled — clear pending and allow
    gate = _read_gate(gate_path)
    if gate is not None:
        gate["pending"]  = None
        gate["decision"] = None
        _atomic_write(gate_path, gate)


if __name__ == "__main__":
    main()
