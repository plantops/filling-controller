#!/usr/bin/env python3
"""Drive the SP01 virtual bench through repeatable dry-cycle checks.

This tool talks only to the diagnostic virtual-I/O HTTP API. It never writes
physical GPIO/DO channels. Use only with a firmware image that prominently
reports VIRTUAL I/O BUILD / VIRTUAL BENCH.

Examples:
    python tools/vbench_gate.py --url http://192.168.11.21 auto
    python tools/vbench_gate.py --url http://192.168.11.21 manual
    python tools/vbench_gate.py --url http://192.168.11.21 fault-permissive
    python tools/vbench_gate.py --url http://192.168.11.21 all --evidence g3.jsonl
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any, Iterable


class GateError(RuntimeError):
    pass


class VirtualBench:
    def __init__(self, base_url: str, evidence: Path | None = None) -> None:
        self.base = base_url.rstrip("/")
        self.evidence = evidence
        self.last_cycle = 0

    def _record(self, event: str, payload: dict[str, Any] | None = None) -> None:
        row: dict[str, Any] = {
            "ts": time.time(),
            "event": event,
        }
        if payload is not None:
            row["data"] = payload
        line = json.dumps(row, sort_keys=True)
        print(line)
        if self.evidence is not None:
            self.evidence.parent.mkdir(parents=True, exist_ok=True)
            with self.evidence.open("a", encoding="utf-8") as f:
                f.write(line + "\n")

    def _request(self, path: str, method: str = "GET") -> bytes:
        req = urllib.request.Request(self.base + path, method=method)
        try:
            with urllib.request.urlopen(req, timeout=3.0) as r:
                return r.read()
        except (urllib.error.URLError, TimeoutError) as exc:
            raise GateError(f"HTTP {method} {path} failed: {exc}") from exc

    def status(self, record: bool = False) -> dict[str, Any]:
        data = json.loads(self._request("/api/io").decode("utf-8"))
        if data.get("mode") != "virtual":
            raise GateError("target is not reporting mode=virtual; refuse to drive it")
        if record:
            self._record("status", data)
        return data

    def set_di(self, channel: int, value: bool) -> None:
        if channel < 1 or channel > 8:
            raise GateError(f"DI channel out of range: {channel}")
        q = urllib.parse.urlencode({"ch": channel - 1, "v": 1 if value else 0})
        self._request(f"/api/di?{q}", method="POST")
        self._record("set_di", {"channel": channel, "value": bool(value)})

    def command(self, action: str) -> None:
        if action not in {"reset", "clear"}:
            raise GateError(f"unsupported command: {action}")
        q = urllib.parse.urlencode({"a": action})
        self._request(f"/api/cmd?{q}", method="POST")
        self._record("command", {"action": action})

    def set_many(self, channels: Iterable[int], value: bool) -> None:
        for ch in channels:
            self.set_di(ch, value)

    def clean_start(self) -> dict[str, Any]:
        self.command("reset")
        self.set_many(range(1, 9), False)
        time.sleep(0.15)
        s = self.status(record=True)
        if s.get("fault") not in {"NONE", "None", "-"}:
            raise GateError(f"clean start fault={s.get('fault')}")
        if int(s.get("do", 0)) != 0:
            raise GateError(f"clean start virtual DO is not zero: {s.get('do')}")
        self.last_cycle = int(s.get("cycle", 0))
        return s

    def wait(self, *, states: set[str] | None = None,
             fault: str | None = None,
             cycle_gt: int | None = None,
             timeout: float = 10.0,
             poll: float = 0.05) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        last: dict[str, Any] = {}
        while time.monotonic() < deadline:
            last = self.status()
            st = str(last.get("state", ""))
            ft = str(last.get("fault", ""))
            cyc = int(last.get("cycle", 0))
            if fault is not None and ft == fault:
                self._record("wait_match", last)
                return last
            if states is not None and st in states:
                self._record("wait_match", last)
                return last
            if cycle_gt is not None and cyc > cycle_gt:
                self._record("wait_match", last)
                return last
            if ft not in {"NONE", "None", "-"}:
                raise GateError(f"unexpected fault while waiting: {ft}; status={last}")
            time.sleep(poll)
        raise GateError(f"timeout waiting for condition; last={last}")

    def enter_auto_coarse(self) -> dict[str, Any]:
        self.clean_start()
        # DI1 feeder, DI2 downstream, DI3 motor(AUTO), DI4 initiative.
        self.set_many((1, 2, 3, 4), True)
        self.wait(states={"WAIT_FILL_POSITION"}, timeout=2.0)
        # Fill-position must be observed low before the rising reference.
        self.set_di(5, False)
        time.sleep(0.10)
        self.set_di(5, True)
        self.wait(states={"BAG_ACQUIRE"}, timeout=2.0)
        self.set_di(6, True)
        return self.wait(states={"COARSE_FILL"}, timeout=2.0)

    def enter_auto_wait_discharge(self) -> dict[str, Any]:
        self.enter_auto_coarse()
        self.wait(states={"FINE_FILL"}, timeout=8.0)
        # CUTOFF can last only one controller tick, so accept later states.
        self.wait(states={"SETTLE", "WAIT_DISCHARGE"}, timeout=12.0)
        return self.wait(states={"WAIT_DISCHARGE"}, timeout=3.0)

    def auto_cycle(self) -> None:
        self._record("case_start", {"case": "auto_cycle"})
        self.enter_auto_wait_discharge()
        start_cycle = int(self.status().get("cycle", 0))

        # Generate A then B with a visible interval; the controller derives the
        # post-B push delay from this measured A->B interval.
        self.set_di(7, True)
        time.sleep(0.10)
        self.set_di(7, False)
        time.sleep(0.30)
        self.set_di(8, True)
        time.sleep(0.10)
        self.set_di(8, False)

        # PUSH may be brief, but cycle_id must increase on COMPLETE.
        s = self.wait(cycle_gt=start_cycle, timeout=3.0)
        if int(s.get("cycle", 0)) != start_cycle + 1:
            raise GateError(f"AUTO cycle increment invalid: {s}")
        if s.get("fault") not in {"NONE", "None"}:
            raise GateError(f"AUTO ended with fault: {s}")
        self._record("case_pass", {"case": "auto_cycle", "final": s})

    def manual_cycle(self) -> None:
        self._record("case_start", {"case": "manual_cycle"})
        self.clean_start()
        start_cycle = int(self.status().get("cycle", 0))
        # DI3 remains 0 -> MANUAL. DI1 feeder + DI4 manual initiative.
        self.set_di(1, True)
        self.set_di(4, True)
        self.wait(states={"BAG_ACQUIRE"}, timeout=2.0)
        self.set_di(6, True)
        self.wait(states={"COARSE_FILL"}, timeout=2.0)
        self.wait(states={"FINE_FILL"}, timeout=8.0)
        s = self.wait(states={"COMPLETE"}, timeout=12.0)
        if int(s.get("cycle", 0)) != start_cycle + 1:
            raise GateError(f"MANUAL cycle increment invalid: {s}")
        if int(s.get("do", 0)) != 0:
            raise GateError(f"MANUAL COMPLETE virtual DO not zero: {s}")
        self.set_di(4, False)
        self.wait(states={"WAIT_PERMISSIVE"}, timeout=2.0)
        self._record("case_pass", {"case": "manual_cycle", "final": s})

    def fault_permissive(self) -> None:
        self._record("case_start", {"case": "fault_permissive"})
        self.enter_auto_coarse()
        self.set_di(2, False)
        s = self.wait(fault="PERMISSIVE_LOST", timeout=2.0)
        if int(s.get("do", 0)) != 0:
            raise GateError(f"fault path left virtual DO on: {s}")
        # Clear is intentionally rejected while DI4 initiative is still ON.
        self.command("clear")
        time.sleep(0.10)
        still = self.status(record=True)
        if still.get("fault") != "PERMISSIVE_LOST":
            raise GateError(f"fault cleared while initiative ON: {still}")
        self.set_di(4, False)
        self.command("clear")
        self.wait(states={"WAIT_PERMISSIVE"}, timeout=2.0)
        self._record("case_pass", {"case": "fault_permissive", "final": s})

    def fault_b_before_a(self) -> None:
        self._record("case_start", {"case": "fault_b_before_a"})
        self.enter_auto_wait_discharge()
        self.set_di(8, True)
        s = self.wait(fault="DISCHARGE_TIMING_INVALID", timeout=2.0)
        if int(s.get("do", 0)) != 0:
            raise GateError(f"discharge timing fault left virtual DO on: {s}")
        self._record("case_pass", {"case": "fault_b_before_a", "final": s})

    def client_absence_precheck(self, seconds: float) -> None:
        """Precheck only: proves browser polling is unnecessary, not cable loss."""
        self._record("case_start", {"case": "g6_client_absence_precheck", "seconds": seconds})
        self.enter_auto_coarse()
        before = self.status(record=True)
        # Deliberately make no HTTP requests during the interval.
        time.sleep(seconds)
        after = self.status(record=True)
        if after.get("fault") not in {"NONE", "None"}:
            raise GateError(f"controller faulted while HMI client absent: {after}")
        if float(after.get("weight", 0.0)) <= float(before.get("weight", 0.0)):
            raise GateError(f"simulated process did not advance without browser polling: {after}")
        self._record("case_pass", {"case": "g6_client_absence_precheck", "before": before, "after": after})


def main() -> int:
    p = argparse.ArgumentParser(description="SP01 virtual-bench gate runner")
    p.add_argument("--url", required=True, help="e.g. http://192.168.11.21")
    p.add_argument("--evidence", type=Path, help="append JSONL evidence")
    p.add_argument("case", choices=["auto", "manual", "fault-permissive", "fault-b-before-a", "g6-client-absence", "all"])
    p.add_argument("--absence-seconds", type=float, default=3.0)
    args = p.parse_args()

    bench = VirtualBench(args.url, args.evidence)
    try:
        bench.status(record=True)  # fail fast and verify mode=virtual
        if args.case in {"auto", "all"}:
            bench.auto_cycle()
        if args.case in {"manual", "all"}:
            bench.manual_cycle()
        if args.case in {"fault-permissive", "all"}:
            bench.fault_permissive()
        if args.case in {"fault-b-before-a", "all"}:
            bench.fault_b_before_a()
        if args.case in {"g6-client-absence", "all"}:
            bench.client_absence_precheck(args.absence_seconds)
    except GateError as exc:
        bench._record("gate_fail", {"error": str(exc)})
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    bench._record("gate_pass", {"case": args.case})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
