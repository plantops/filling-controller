# SP01 G3 software prequalification — 2026-09-10

**Verdict: SUPERSEDED / NOT SUFFICIENT FOR G3 EXIT.**

Branch: `diag/sp01-g2-g9`
Original test-wiring commit: `030807615d35408ff0cc3d50187acc459efa9847`
Original CI run: `34472688312` / workflow run #117

## Original software result

The original host/ESP32 software matrix passed the then-documented normal-cycle and fault tests: AUTO/MANUAL cycle behavior, timeout/fault handling, stale/faulted weight handling, safe reset, and discharge-reference checks.

## Why this evidence is now superseded

Subsequent installed-machine review froze additional canonical behavior that the original G3 suite does not implement or prove:

```text
while COARSE_FILL or FINE_FILL is active:
    sustained qualified net-weight loss -> broken bag / REJECT
    immediately remove dosing valves, filling motor and spout aeration
    latch REJECT for the current cycle
    eject/push the rejected bag near 210 deg
    suppress the later normal ~355 deg push

healthy GOOD bag:
    no 210 deg reject push
    normal push near ~355 deg
```

Therefore a green CI result from this file's original suite must not be cited as complete G3 exit evidence.

## Current software coverage status

Covered by the existing suite:

- normal AUTO full cycle;
- MANUAL full fill without automatic bag push;
- manual OFF during filling;
- mode change during active filling;
- bag/permissive/weight/timeout fault matrix;
- forced I/O fault and reset safe-output behavior;
- fault-clear interlock;
- existing normal-discharge A/B timing behavior.

Not yet covered by executable controller/tests:

- finite-window broken-bag weight-loss detector;
- immediate DO4..DO8 removal on qualified detection;
- per-cycle GOOD/REJECT disposition latch;
- 210 deg reject scheduling;
- rejection suppressing later ~355 deg normal push;
- GOOD path explicitly skipping the 210 deg reject action.

## Blocking contract

The detector semantics and immediate shutdown behavior are known. The remaining unresolved contract is the trustworthy position/timing reference that identifies the ~210 deg reject window on the installed machine.

Do not invent a new DI or reuse DI7/DI8. Do not derive 210 deg from the existing A/B normal-discharge references until field evidence establishes that relationship.

## G3 exit rule

G3 remains `ACTIVE-SW`. A replacement `RESULT.md` may claim G3 PASS only after the current canonical G3 matrix passes against the executable C++ controller and the required bench/dummy-I/O evidence in the gate plan is recorded.

CI is software evidence only and does not advance G2, G2T, G4, G5, G8 or G9.
