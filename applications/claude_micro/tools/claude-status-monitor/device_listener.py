#!/usr/bin/env python3
"""
Phase 3: USB device listener daemon.

Two threads:
  HID thread  — reads button (VID=0x2fe3, PID=0x0100); press → gate allow
  Audio thread — captures UAC mic; VAD → Whisper → voice_result.json + clipboard

Run once alongside monitor_ui.py:
  python device_listener.py
"""
import json
import os
import tempfile
import threading
import time
import wave
from pathlib import Path

import hid
import numpy as np
import pyautogui
import pyperclip
import sounddevice as sd
from faster_whisper import WhisperModel

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

MONITOR_DIR = Path.home() / ".claude" / "monitor"

HID_VID = 0x2FE3
HID_PID = 0x0100
HID_REPORT_BUTTON  = 0x01
HID_REPORT_ID_LED  = 0x02

# host status string → LED state byte (0=idle,1=working,2=waiting,3=success)
_STATUS_TO_LED: dict[str, int] = {
    "working": 1,
    "waiting": 2,
    "success": 3,
}

SAMPLE_RATE      = 16000
CHANNELS         = 1
DTYPE            = "int16"
BLOCK_SIZE       = 512        # frames per callback

SPEECH_RMS       = 800        # int16 RMS threshold to detect speech
SILENCE_END_S    = 1.5        # seconds of silence to end a segment
MIN_SEGMENT_S    = 0.4        # discard segments shorter than this
MAX_SEGMENT_S    = 30.0       # force-flush after this duration

WHISPER_MODEL    = "base"
WHISPER_LANGUAGE = None       # None = auto-detect (supports zh + en)


# ---------------------------------------------------------------------------
# Shared Whisper model (lazy, loaded once)
# ---------------------------------------------------------------------------

_model: WhisperModel | None = None
_model_lock = threading.Lock()


def _get_model() -> WhisperModel:
    global _model
    with _model_lock:
        if _model is None:
            print("[Whisper] Loading model…")
            _model = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
            print("[Whisper] Ready")
        return _model


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _atomic_write(path: Path, data: dict) -> None:
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)


def _copy_to_clipboard(text: str) -> None:
    try:
        pyperclip.copy(text)
        print(f"[Clipboard] Copied: {text[:60]}")
    except Exception as e:
        print(f"[Clipboard] Error: {e}")


def _find_gate_to_allow(panel_idx: int = -1) -> bool:
    """Write decision=allow to the gate for the given panel index (0-based).

    panel_idx=-1 targets the first pending gate regardless of panel.
    Panel order is determined by state file created_at timestamps.
    """
    target_key: str | None = None
    if panel_idx >= 0:
        sessions: list[tuple[float, str]] = []
        for f in MONITOR_DIR.glob("state_*.json"):
            try:
                data = json.loads(f.read_text(encoding="utf-8"))
                sessions.append((data.get("created_at", 0.0), f.stem[len("state_"):]))
            except Exception:
                pass
        sessions.sort(key=lambda t: t[0])
        if panel_idx < len(sessions):
            target_key = sessions[panel_idx][1]

    for f in MONITOR_DIR.glob("gate_*.json"):
        try:
            gate = json.loads(f.read_text(encoding="utf-8"))
            if not (gate.get("enabled") and gate.get("pending") and gate.get("decision") is None):
                continue
            if target_key is not None and f.stem[len("gate_"):] != target_key:
                continue
            gate["decision"] = "allow"
            tmp = f.with_suffix(".tmp")
            tmp.write_text(json.dumps(gate, ensure_ascii=False, indent=2), encoding="utf-8")
            tmp.replace(f)
            print(f"[HID] Gate allowed: {f.name} (panel {panel_idx})")
            return True
        except Exception:
            pass
    print(f"[HID] Button pressed (panel {panel_idx}) — no pending gate found")
    return False


# ---------------------------------------------------------------------------
# Shared HID device handle (set by _hid_thread, read by _led_sync_thread)
# ---------------------------------------------------------------------------

_hid_dev: hid.device | None = None
_hid_dev_lock = threading.Lock()

# Event fired by _hid_thread each time MCU connects; _audio_thread waits on it
# to re-scan for the USB audio device after MCU enumerates.
_hid_connected_event = threading.Event()


def _send_led_report(states: list[int]) -> bool:
    """Send Report ID 0x02 with win1/win2/win3 LED states. Returns True on success."""
    with _hid_dev_lock:
        dev = _hid_dev
    if dev is None:
        return False
    try:
        dev.write([HID_REPORT_ID_LED, states[0], states[1], states[2]])
        return True
    except Exception as e:
        print(f"[LED] HID write failed: {e}")
        return False


# ---------------------------------------------------------------------------
# HID listener thread
# ---------------------------------------------------------------------------

def _hid_thread() -> None:
    global _hid_dev
    while True:
        try:
            dev = hid.device()
            dev.open(HID_VID, HID_PID)
            dev.set_nonblocking(False)
            mfr = dev.get_manufacturer_string()
            prd = dev.get_product_string()
            print(f"[HID] Connected: {mfr} / {prd}")

            with _hid_dev_lock:
                _hid_dev = dev

            # Notify audio thread that MCU is now enumerated (UAC may be ready)
            _hid_connected_event.set()

            while True:
                report = dev.read(64, timeout_ms=500)
                if not report:
                    continue
                if report[0] == HID_REPORT_BUTTON:
                    btn_byte = report[1] & 0x07
                    if btn_byte:
                        for bit in range(3):
                            if btn_byte & (1 << bit):
                                _find_gate_to_allow(bit)

        except OSError as e:
            print(f"[HID] Disconnected ({e}), retrying in 2 s…")
        except Exception as e:
            print(f"[HID] Error: {e}, retrying in 2 s…")
        finally:
            with _hid_dev_lock:
                _hid_dev = None
        time.sleep(2)


# ---------------------------------------------------------------------------
# Whisper worker (runs in its own thread per segment)
# ---------------------------------------------------------------------------

def _transcribe(audio: np.ndarray) -> None:
    fd, wav_path = tempfile.mkstemp(suffix=".wav")
    try:
        os.close(fd)
        with wave.open(wav_path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(audio.tobytes())

        model = _get_model()
        segments, _ = model.transcribe(
            wav_path,
            language=WHISPER_LANGUAGE,
            beam_size=5,
            vad_filter=True,         # built-in Silero VAD removes leading/trailing silence
        )
        text = " ".join(s.text.strip() for s in segments).strip()

        if text:
            print(f"[Whisper] {text}")
            _copy_to_clipboard(text)
            time.sleep(0.15)           # 等剪贴板写入稳定
            pyautogui.hotkey('ctrl', 'v')
            pyautogui.press('enter')
            _atomic_write(MONITOR_DIR / "voice_result.json", {
                "text":       text,
                "updated_at": time.time(),
            })
        else:
            print("[Whisper] (empty transcript)")
    except Exception as e:
        print(f"[Whisper] Error: {e}")
    finally:
        try:
            os.unlink(wav_path)
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Audio capture + VAD
# ---------------------------------------------------------------------------

def _find_usb_input_device() -> int | None:
    for i, d in enumerate(sd.query_devices()):
        if d["max_input_channels"] > 0 and "USB" in d["name"].upper():
            print(f"[Audio] USB input found: [{i}] {d['name']}")
            return i
    default = sd.default.device[0]
    print(f"[Audio] No USB input found, using default [{default}]")
    return default


def _audio_thread() -> None:
    # Wait for HID connect so UAC device has time to enumerate, then scan.
    # Re-scan on every reconnect so hot-plug works correctly.
    _hid_connected_event.wait()
    time.sleep(1.0)   # give UAC a moment to appear after HID connect
    _hid_connected_event.clear()
    device_idx = _find_usb_input_device()

    # VAD state (mutated only inside the sd callback — single callback thread)
    buf:           list[np.ndarray] = []
    recording:     bool             = False
    speech_start:  float            = 0.0
    silence_since: float | None     = None

    def callback(indata, frames, time_info, status):
        nonlocal recording, speech_start, silence_since, buf

        chunk = indata[:, 0].copy()
        rms   = float(np.sqrt(np.mean(chunk.astype(np.float32) ** 2)))

        if rms > SPEECH_RMS:
            # — active speech —
            if not recording:
                recording    = True
                speech_start = time.time()
                buf.clear()
                print("[Audio] Speech started")
            silence_since = None
            buf.append(chunk)

        else:
            # — silence —
            if recording:
                buf.append(chunk)
                now = time.time()

                if silence_since is None:
                    silence_since = now
                elif now - silence_since >= SILENCE_END_S:
                    _flush(now)

                # hard cap
                if recording and now - speech_start >= MAX_SEGMENT_S:
                    _flush(now)

    def _flush(now: float) -> None:
        nonlocal recording, silence_since, buf
        duration = now - speech_start
        recording    = False
        silence_since = None
        print(f"[Audio] Speech ended ({duration:.1f} s)")
        if duration >= MIN_SEGMENT_S and buf:
            audio = np.concatenate(buf)
            buf.clear()
            threading.Thread(target=_transcribe, args=(audio,), daemon=True).start()
        else:
            buf.clear()

    while True:
        try:
            print(f"[Audio] Opening device [{device_idx}] @ {SAMPLE_RATE} Hz…")
            with sd.InputStream(
                device=device_idx,
                channels=CHANNELS,
                samplerate=SAMPLE_RATE,
                dtype=DTYPE,
                blocksize=BLOCK_SIZE,
                callback=callback,
            ):
                while True:
                    time.sleep(1)
        except Exception as e:
            print(f"[Audio] Error: {e}, retrying in 3 s…")
            time.sleep(3)


# ---------------------------------------------------------------------------
# LED sync thread — polls state files, pushes LED states to MCU every 0.5 s
# ---------------------------------------------------------------------------

# Even when nothing changes, re-send the current state every this many seconds
# so the MCU always reflects reality after a reconnect or dropped report.
# Keep these in sync with monitor_ui.py so LED and UI agree on what is "stale".
_SUCCESS_RESET_S  = 10      # seconds before success auto-resets to idle
_STALE_ACTIVE_S   = 300     # seconds before working/waiting treated as idle
_LED_FORCE_SYNC_INTERVAL = 10.0


def _effective_status(raw_status: str, updated_at: float) -> str:
    """Mirror the staleness logic in monitor_ui.py._poll_states."""
    age = time.time() - updated_at
    if raw_status == "success" and age > _SUCCESS_RESET_S:
        return "idle"
    if raw_status in ("working", "waiting") and age > _STALE_ACTIVE_S:
        return "idle"
    return raw_status


def _read_gate_status(key: str) -> str | None:
    """Return 'waiting' if this session has an active pending gate, else None.

    gate_{key}.json structure (written by gate_hook.py):
      enabled  : bool
      pending  : dict (tool + input + requested_at) | null
      decision : "allow" | "block" | null
    A gate is "blocking" when enabled=true, pending is a non-null dict, decision=null.
    """
    try:
        gate = json.loads(
            (MONITOR_DIR / f"gate_{key}.json").read_text(encoding="utf-8")
        )
        if (gate.get("enabled")
                and isinstance(gate.get("pending"), dict)
                and gate.get("decision") is None):
            return "waiting"
    except Exception:
        pass
    return None


def _led_sync_thread() -> None:
    """Read state_*.json (+ gate_*.json) and send per-window LED states.

    Sends on two conditions:
      • state changed   (immediate)
      • _LED_FORCE_SYNC_INTERVAL seconds elapsed since last successful send
        (heartbeat — keeps MCU in sync after reconnect or dropped reports)

    Mirrors monitor_ui's logic exactly:
      • stale working/waiting (>300 s) → idle
      • stale success (>10 s) → idle
      • gate pending → override to waiting  (this is what monitor_ui shows in yellow)
    """
    prev_states: list[int] = [-1, -1, -1]
    last_sent_t: float = 0.0

    while True:
        try:
            sessions: list[tuple[float, str, str]] = []   # (created_at, status, key)
            for f in MONITOR_DIR.glob("state_*.json"):
                try:
                    data = json.loads(f.read_text(encoding="utf-8"))
                    key  = data.get("session_key", f.stem[len("state_"):])
                    raw  = data.get("status", "idle")
                    upd  = data.get("updated_at", 0.0)
                    eff  = _effective_status(raw, upd)
                    # Gate override: if the gate is pending, treat as waiting
                    # (same logic as monitor_ui._poll_states)
                    if eff not in ("idle",):
                        gate_eff = _read_gate_status(key)
                        if gate_eff:
                            eff = gate_eff
                    sessions.append((data.get("created_at", 0.0), eff, key))
                except Exception:
                    pass
            sessions.sort(key=lambda t: t[0])

            states: list[int] = [0, 0, 0]
            for i in range(min(3, len(sessions))):
                states[i] = _STATUS_TO_LED.get(sessions[i][1], 0)  # [1] = eff status

            now     = time.monotonic()
            changed = states != prev_states
            due     = now - last_sent_t >= _LED_FORCE_SYNC_INTERVAL

            if changed:
                # Debug: show what triggered the change
                dbg = ", ".join(
                    f"{s[2]}={s[1]}" for s in sessions[:3]
                ) if sessions else "none"
                print(f"[LED] State change detected: {dbg}")

            if changed or due:
                if _send_led_report(states):
                    tag = "changed" if changed else "periodic"
                    print(f"[LED] States sent ({tag}): "
                          f"win1={states[0]} win2={states[1]} win3={states[2]}")
                    prev_states = states[:]
                    last_sent_t = now
        except Exception as e:
            print(f"[LED] Sync error: {e}")

        time.sleep(0.5)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    print("[device_listener] Phase 3 starting…")
    MONITOR_DIR.mkdir(parents=True, exist_ok=True)

    # Pre-load Whisper model in background so first transcription is fast
    threading.Thread(target=_get_model, daemon=True).start()

    hid_t   = threading.Thread(target=_hid_thread,    name="hid",      daemon=True)
    audio_t = threading.Thread(target=_audio_thread,  name="audio",    daemon=True)
    led_t   = threading.Thread(target=_led_sync_thread, name="led_sync", daemon=True)
    hid_t.start()
    audio_t.start()
    led_t.start()

    print("[device_listener] Running. Ctrl-C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[device_listener] Stopped.")


if __name__ == "__main__":
    main()
