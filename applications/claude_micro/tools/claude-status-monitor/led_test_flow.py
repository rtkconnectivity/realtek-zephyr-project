#!/usr/bin/env python3
"""
One-shot LED state-flow tester.

Simulates a single task lifecycle on a chosen window (1–3):
  working → waiting → success → idle

The test state file is assigned a created_at that places it at the
requested window slot relative to existing real sessions:
  --win 1  → earliest  (before all real sessions)
  --win 2  → middle
  --win 3  → latest    (after all real sessions)

Usage:
  python led_test_flow.py                  # default: win 1, 3 s per step
  python led_test_flow.py --win 2          # target window 2
  python led_test_flow.py --win 3 --step 5 # window 3, 5 s per step
"""
import argparse
import json
import time
from pathlib import Path

MONITOR_DIR = Path.home() / ".claude" / "monitor"
STATE_FILE  = MONITOR_DIR / "state_test0001.json"
SESSION_KEY = "test0001"

_STEPS = [
    ("working", "Bash",  "执行中   win={w} → LED {w}=1 (蓝)"),
    ("waiting", "Write", "等待输入  win={w} → LED {w}=2 (黄)"),
    ("success", "",      "执行完成  win={w} → LED {w}=3 (绿)"),
    ("idle",    "",      "已结束   win={w} → LED {w}=0 (灰)"),
]


def _real_session_timestamps() -> list[float]:
    """Return sorted created_at values of non-test state files."""
    ts = []
    for f in MONITOR_DIR.glob("state_*.json"):
        if f == STATE_FILE:
            continue
        try:
            d = json.loads(f.read_text(encoding="utf-8"))
            ts.append(d.get("created_at", 0.0))
        except Exception:
            pass
    return sorted(ts)


def _pick_created_at(win: int) -> float:
    """
    Choose a created_at so that this test session sorts into slot `win`
    (1-based) when all state files are ordered by created_at.

    win=1 → insert before everything
    win=2 → insert between slot-0 and slot-1 real sessions (or just before latest)
    win=3 → insert after everything
    """
    real = _real_session_timestamps()   # sorted ascending

    if win == 1:
        # Before the earliest real session
        return (real[0] - 1.0) if real else (time.time() - 100.0)

    if win == 3:
        # After the latest real session
        return (real[-1] + 1.0) if real else time.time()

    # win == 2: between index 0 and index 1
    if len(real) >= 2:
        return (real[0] + real[1]) / 2.0
    if len(real) == 1:
        return real[0] - 0.5   # just before the only real session → slot 1, pushing it to 2
    return time.time() - 50.0


def _write(status: str, tool: str, created_at: float) -> None:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    tmp = STATE_FILE.with_suffix(".tmp")
    tmp.write_text(
        json.dumps({
            "session_key": SESSION_KEY,
            "project":     "led_test",
            "project_dir": str(MONITOR_DIR),
            "status":      status,
            "tool":        tool,
            "created_at":  created_at,
            "updated_at":  time.time(),
        }, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    tmp.replace(STATE_FILE)


def main() -> None:
    ap = argparse.ArgumentParser(description="LED state-flow tester")
    ap.add_argument("--win",  type=int, default=1, choices=[1, 2, 3],
                    help="target window slot (1-3, default 1)")
    ap.add_argument("--step", type=float, default=3.0,
                    help="seconds to hold each state (default 3)")
    args = ap.parse_args()

    created_at = _pick_created_at(args.win)
    total = len(_STEPS)

    print(f"[test] Window {args.win} | {total} steps x {args.step:.0f} s each")
    print()

    for i, (status, tool, desc_tpl) in enumerate(_STEPS):
        desc = desc_tpl.format(w=args.win)
        print(f"  [{i+1}/{total}] {desc}")
        _write(status, tool, created_at)

        if i < total - 1:
            for remaining in range(int(args.step), 0, -1):
                print(f"        {remaining}s ...", end="\r", flush=True)
                time.sleep(1)
            print()

    print()
    print(f"[test] Done. Window {args.win} is now idle.")
    print(f"       File: {STATE_FILE}")


if __name__ == "__main__":
    main()
