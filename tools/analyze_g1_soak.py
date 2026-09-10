#!/usr/bin/env python3
"""Analyze a g1_usb_probe.py soak log without inventing pass limits.

The current documented G1 pre-G2 rule is strict and qualitative: heartbeat must
continue, heap must not trend down, no unexpected reset, no link flap, and no
loss of the DHCP address. This analyzer reports evidence and returns non-zero
when any of those explicit conditions is observed.
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from pathlib import Path

HB_RE = re.compile(r"\bHB\s+(\d+)\b.*?heap=(\d+).*?ip=([^\s]+)")
RESET_RE = re.compile(r"\breset=([A-Z0-9_]+)")


def main() -> int:
    p = argparse.ArgumentParser(description="Analyze SP01 one-hour/overnight G1 soak log")
    p.add_argument("log", type=Path)
    p.add_argument("--min-seconds", type=int, default=3600,
                   help="required observed heartbeat span; default 3600 s")
    args = p.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    hbs = [(int(n), int(heap), ip) for n, heap, ip in HB_RE.findall(text)]
    resets = RESET_RE.findall(text)
    link_down = text.count("ETH EVENT: link disconnected") + text.count("Ethernet link down")
    panic = text.lower().count("panic")
    brownout = text.lower().count("brownout")
    watchdog = text.lower().count("watchdog") + text.lower().count("task_wdt")

    failures: list[str] = []
    if len(hbs) < 2:
        failures.append("fewer than two heartbeats parsed")
    else:
        nums = [n for n, _, _ in hbs]
        gaps = [(a, b) for a, b in zip(nums, nums[1:]) if b != a + 1]
        if gaps:
            failures.append(f"heartbeat sequence gaps: {gaps[:8]}")
        observed_seconds = nums[-1] - nums[0]
        if observed_seconds < args.min_seconds:
            failures.append(f"observed heartbeat span {observed_seconds}s < required {args.min_seconds}s")

        heaps = [heap for _, heap, _ in hbs]
        # No arbitrary byte threshold: flag only a persistent monotonic direction
        # by comparing medians of the first and last deciles.
        window = max(3, len(heaps) // 10)
        first_med = statistics.median(heaps[:window])
        last_med = statistics.median(heaps[-window:])
        if last_med < first_med:
            failures.append(f"heap end median {last_med} < start median {first_med}; inspect for leak")

        bad_ip = [(n, ip) for n, _, ip in hbs if ip == "0.0.0.0"]
        # Ignore pre-DHCP zeros before the first non-zero lease. Once a lease has
        # appeared, any return to zero is a documented failure condition.
        first_lease = next((i for i, (_, _, ip) in enumerate(hbs) if ip != "0.0.0.0"), None)
        if first_lease is None:
            failures.append("no DHCP lease observed")
        else:
            post_lease_zeros = [(n, ip) for n, _, ip in hbs[first_lease:] if ip == "0.0.0.0"]
            if post_lease_zeros:
                failures.append(f"IP returned to 0.0.0.0 after DHCP: {post_lease_zeros[:8]}")

    unexpected_resets = [r for r in resets[1:] if r not in {"USB", "POWERON"}]
    if unexpected_resets:
        failures.append(f"unexpected reset reasons: {unexpected_resets}")
    if link_down:
        failures.append(f"Ethernet link-down events: {link_down}")
    if panic:
        failures.append(f"panic text occurrences: {panic}")
    if brownout:
        failures.append(f"brownout text occurrences: {brownout}")
    if watchdog:
        failures.append(f"watchdog text occurrences: {watchdog}")

    print(f"heartbeats={len(hbs)}")
    if hbs:
        print(f"hb_first={hbs[0][0]} hb_last={hbs[-1][0]}")
        heaps = [heap for _, heap, _ in hbs]
        print(f"heap_min={min(heaps)} heap_max={max(heaps)} heap_first={heaps[0]} heap_last={heaps[-1]}")
        leases = sorted({ip for _, _, ip in hbs if ip != "0.0.0.0"})
        print("leases=" + ",".join(leases))
    print(f"link_down={link_down} panic={panic} brownout={brownout} watchdog={watchdog}")

    if failures:
        print("VERDICT=NOT_PASS")
        for f in failures:
            print("FAIL: " + f)
        return 1

    print("VERDICT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
