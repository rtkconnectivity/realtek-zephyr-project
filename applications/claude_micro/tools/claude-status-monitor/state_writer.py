#!/usr/bin/env python3
"""
Claude Code hook -> monitor state file writer.

Usage (configured in ~/.claude/settings.json hooks):
  python D:/ai/bt2/tools/claude-status-monitor/state_writer.py <event>

Events:
  pre_tool   -- PreToolUse  (Claude is actively running a tool)
  permission -- PermissionRequest  (needs user confirmation)
  stop       -- Stop  (task complete)
"""
import hashlib
import json
import os
import sys
import time
from pathlib import Path

MONITOR_DIR = Path.home() / ".claude" / "monitor"

_EVENT_STATUS = {
    "pre_tool":   "working",
    "permission": "waiting",
    "stop":       "success",
}


def _session_info() -> tuple[str, str, str]:
    project_dir = os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd())
    key = hashlib.md5(project_dir.encode()).hexdigest()[:8]
    return key, Path(project_dir).name, project_dir


def _atomic_write(path: Path, data: dict) -> None:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)


def main() -> None:
    event = sys.argv[1] if len(sys.argv) > 1 else "unknown"

    payload: dict = {}
    try:
        raw = sys.stdin.read()
        if raw.strip():
            payload = json.loads(raw)
    except Exception:
        pass

    key, project_name, project_dir = _session_info()
    state_path = MONITOR_DIR / f"state_{key}.json"

    created_at = time.time()
    if state_path.exists():
        try:
            old = json.loads(state_path.read_text(encoding="utf-8"))
            created_at = old.get("created_at", created_at)
        except Exception:
            pass

    _atomic_write(state_path, {
        "session_key": key,
        "project":     project_name,
        "project_dir": project_dir,
        "status":      _EVENT_STATUS.get(event, "working"),
        "tool":        payload.get("tool_name", payload.get("tool", "")),
        "created_at":  created_at,
        "updated_at":  time.time(),
    })


if __name__ == "__main__":
    main()
