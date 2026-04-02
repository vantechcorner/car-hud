#!/usr/bin/env python3
"""
Offline check: mirror bridge + HUD speed/RPM/temp parsing (ASCII only).

Usage (no car, no flash):
  python tools/obd_normalizer.py --samples
  python tools/obd_normalizer.py path/to/captured_lines.txt
  type captured.txt | python tools/obd_normalizer.py

Capture real adapter lines once (bridge Serial USB log), then replay here forever.
Note: PID 01 0D often returns 0 km/h when the vehicle is not moving — parse can be OK while UI shows 0.
"""

from __future__ import annotations

import argparse
import sys
from typing import Optional, Tuple


def hex_nibble(c: str) -> int:
    c = c.upper()
    if "0" <= c <= "9":
        return ord(c) - ord("0")
    if "A" <= c <= "F":
        return 10 + ord(c) - ord("A")
    raise ValueError(f"bad hex char {c!r}")


def hex_pair(hi: str, lo: str) -> int:
    return (hex_nibble(hi) << 4) | hex_nibble(lo)


def extract_hex_digits(s: str) -> str:
    return "".join(ch for ch in s.upper() if ch in "0123456789ABCDEF")


def bridge_normalize_line(line: str) -> Optional[str]:
    """Same rules as bridge processElmLine (compact -> spaced mode 01)."""
    line = line.strip().upper()
    if not line or line == "OK" or line.startswith("AT") or line.startswith("SEARCHING"):
        return None
    s = line.replace(" ", "")
    p = s.find("41")
    if p > 0:
        s = s[p:]
    if not s.startswith("41"):
        return None
    if len(s) == 6:
        pid, v1 = s[2:4], s[4:6]
        return f"41 {pid} {v1}"
    if len(s) == 8:
        pid, v1, v2 = s[2:4], s[4:6], s[6:8]
        return f"41 {pid} {v1} {v2}"
    return None


def hud_parse_obd_line(line: str) -> Optional[Tuple[str, str]]:
    """
    Mirror src/main.cpp parseObd for mode 01 single-byte PID and RPM/temp.
    Returns (label, value_str) or None.
    """
    if len(line) < 4:
        return None
    if "ERROR" in line.upper():
        return ("error", "can_error")
    digits = extract_hex_digits(line)
    if len(digits) < 6 or not digits.startswith("41"):
        return None
    pid = hex_pair(digits[2], digits[3])
    if pid == 0x0D:
        if len(digits) < 6:
            return None
        v = hex_pair(digits[4], digits[5])
        return ("speed_kmh", str(v))
    if pid == 0x0C:
        if len(digits) < 8:
            return None
        a = hex_pair(digits[4], digits[5])
        b = hex_pair(digits[6], digits[7])
        rpm = ((a << 8) | b) / 4.0
        return ("rpm", f"{rpm:.0f}")
    if pid == 0x05:
        if len(digits) < 6:
            return None
        a = hex_pair(digits[4], digits[5])
        return ("coolant_C", f"{a - 40:g}")
    return ("unknown_pid", f"0x{pid:02X}")


def run_samples() -> None:
    samples = [
        "41 0D 2A",
        "410D2A",
        '7E802410D2A',
        "41 0C 1A F8",
        "410C1AF8",
        "NO DATA",
        "UNABLE TO CONNECT",
    ]
    for raw in samples:
        n = bridge_normalize_line(raw)
        out = hud_parse_obd_line(n) if n else None
        print(f"IN : {raw!r}")
        print(f"BR : {n!r}")
        print(f"HUD:({out[0]}={out[1]})" if out else "HUD: (no parse)")
        print("---")


def main() -> int:
    ap = argparse.ArgumentParser(description="Offline OBD line normalizer + HUD parse preview")
    ap.add_argument("file", nargs="?", help="Text file, one ELM line per row (optional; else stdin)")
    ap.add_argument("--samples", action="store_true", help="Run built-in examples")
    args = ap.parse_args()
    if args.samples:
        run_samples()
        return 0

    def handle_line(raw: str) -> None:
        raw = raw.rstrip("\r\n")
        if not raw.strip():
            return
        n = bridge_normalize_line(raw)
        hud = hud_parse_obd_line(n) if n else None
        if n:
            print(f"OK  raw={raw!r} -> bridge={n!r} -> hud={hud}")
        else:
            print(f"SKIP raw={raw!r}")

    if args.file:
        with open(args.file, encoding="utf-8", errors="replace") as f:
            for line in f:
                handle_line(line)
        return 0
    for line in sys.stdin:
        handle_line(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
