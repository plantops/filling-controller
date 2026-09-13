# SP01 G2 -> G9 Gate Execution Plan

Branch: `diag/sp01-g2-g9`
Baseline: `c8bbfa8a0a92f0d899497aa4d8b2e41647b9811c`
Updated: 2026-09-12

## Operating rule

Advance a gate only from evidence appropriate to that gate. CI/simulation may close software gates; they never substitute for physical I/O, TLB, thermal, shadow or live-machine evidence.

Machine actuators remain disconnected until G8 shadow is complete and G9 live connection is explicitly authorized locally.

## Current gate state

| Gate | Scope | Status | Exit evidence |
|---|---|---|---|
| G0 | Build | PASS | host tests + ESP32-S3 build |
| G1 | Safe board bring-up | PASS | boot, 16 MB flash, TCA9554 safe, DI idle, W5500 SPI/link/DHCP, stable capture |
| G2 | Physical dummy I/O + Ethernet | PASS WITH OPERATOR WAIVER | 1 h soak PASS; DI1..8 PASS; partial physical output-stage switching evidence retained; exhaustive DO1..8 mapping and separate G2 reset-safe capture explicitly waived by project owner/operator |
| G2T | Thermal/serviceability | BLOCKED-HW | measured installation temp, elevated-temp soak, reset/fault safe outputs, spare swap/config recovery |
| G3 | Dry FSM/process logic | PASS | deterministic C++ AUTO/MANUAL + fault matrix + GOOD/~355 and REJECT/~210 software routes |
| G4 | TLB485 dynamic weighing | BLOCKED-HW | kg/status, update rate, latency/jitter, filter/noise envelope, stale/disconnect/reconnect, switching-noise trial |
| G5 | Calibration | BLOCKED-HW | zero, 20 kg check, 50 kg span, zero/20/50 verification |
| G6 | Network-loss independence | PENDING | local controller/weighing behavior unchanged when browser/network disappears and returns |
| G7 | Bench design review | PENDING | core V1..V11 reconciled with executable + measured evidence; affected V12/V13 extensions reviewed; red-team R1 no unresolved critical finding |
| G8 | Machine shadow | BLOCKED-HW | real DI + TLB, new DO isolated, legacy-vs-SP01 timeline for GOOD and REJECT, red-team R2 |
| G9 | One-spout live pilot | BLOCKED-HW | controlled DO connection, rollback/spare ready, local acceptance of GOOD and REJECT paths |

G2 baseline evidence: `evidence/SP01/2026-09-10/G2/RESULT.md`.
G2 reduced-rigor acceptance: `evidence/SP01/2026-09-12/G2/OPERATOR_WAIVER.md`.
G3 evidence: `evidence/SP01/2026-09-10/G3/SOFTWARE_RESULT.md`.

The G2 waiver is a documented scope decision. It does not claim measurements that were not completed. Any unexplained output behavior during later commissioning reopens the relevant hardware check.

## Canonical view ownership

Primary view pack: `SP01_CANONICAL_VIEWS.md`.

```text
CORE: V1..V11
EXT:  V12 weighing quality/calibration; V13 eight-spout topology

G0   V6
G1   V7 + V10
G2   V1 + V7
G2T  V1 + V7 + V10
G3   V2 + V3 + V4 + V6 + V8
G4   V1 + V10 + V12
G5   V10 + V12
G6   V5 + V10 + V11
G7   core V1..V11 + affected extensions reconciled
G8   V1 + V7 + V8 + V11 + V12 + V13
G9   V1 + V8 + V11
```

No diagram or HMI view is gate proof unless backed by executable or measured evidence owned by that gate.

## Frozen broken-bag contract

```text
while COARSE_FILL or FINE_FILL:
    qualified persistent measured-weight loss from running high-water mark
    -> latch disposition REJECT
    -> DO4..DO8 OFF in same control decision

AUTO:
    -> REJECT_WAIT
    -> one DO3 bag.push at semantic ~210 deg reject window
    -> no later ~355 deg push

MANUAL:
    -> COMPLETE
    -> no automatic DO3 push

healthy AUTO bag:
    normal fill/cutoff/settle
    -> disposition GOOD
    -> normal A/B discharge timing
    -> one DO3 bag.push near ~355 deg
```

210° is a reject/eject position, not a broken-bag sensor. Broken-bag detection comes from the TLB485 weight trajectory while filling.

Detector threshold/persistence and real 210/355 timing/lead are not frozen by G3. They belong to G4/G8.

## G2 — physical dummy I/O — PASS WITH OPERATOR WAIVER

Machine wiring remains disconnected.

Completed evidence:

```text
1 h heartbeat/network soak          PASS
DI1..DI8 dry-contact truth          PASS
physical output stage switching     OBSERVED PARTIALLY
```

Project owner/operator accepted the available G2 evidence for progression and explicitly waived:

```text
exhaustive one-by-one DO1..DO8 loopback proof
separate reset-safe timing capture dedicated only to G2
```

This waiver is recorded in `evidence/SP01/2026-09-12/G2/OPERATOR_WAIVER.md` and is not equivalent to a fabricated per-channel hardware PASS.

Production actuator wiring remains disconnected. G8/G9 still require actual machine-channel mapping, safe behavior, and commanded-output correctness before live authority.

## G2T — thermal/serviceability

```text
measure actual controller/TLB mounting temperature
controlled elevated-temperature operation
representative I/O + network + RS485 traffic
boot/reset/brownout/fault behavior
safe-output verification
pre-flashed spare-controller replacement drill
restore approved config
verify ESP replacement does not alter TLB calibration
```

No temperature/lifetime claim beyond measured evidence.

## G3 — dry FSM/process logic — PASS

The C++ controller represents GOOD/REJECT dispositions and the broken-bag shutdown path.

Automated matrix includes:

```text
AUTO normal full cycle
MANUAL normal cycle; no automatic push
manual OFF during fill
mode change during active cycle
permissive loss
bag missing / bag lost
weight stale / weight fault
coarse/fine/discharge timeouts
invalid discharge timing
forced I/O fault and fault-clear rules

increasing weight -> no false reject
single negative spike -> no reject
persistent qualified high-water loss -> REJECT
REJECT -> DO4..DO8 OFF in same tick
AUTO REJECT -> semantic ~210 window -> one DO3 push
AUTO REJECT -> no later normal path
AUTO GOOD -> ignores reject window
AUTO GOOD -> normal A/B discharge -> DO3 push
MANUAL broken bag -> stop fill, COMPLETE, no automatic push
reject-window timeout -> bounded fault-safe result
```

`PositionSnapshot.reject_window` is a semantic software boundary only. G3 does not claim how the installed machine derives 210°.

## G4 — TLB485 dynamic weighing

Connect the actual TLB485/load-cell chain on bench and measure:

```text
actual digital kg/status
sample/update frequency
poll/response latency and jitter
Modbus errors
TLB filter/profile
stationary and dynamic/vibration noise
running-high-water loss noise envelope
stale detection
disconnect/reconnect recovery
representative switching-noise behavior
```

G4 supplies evidence for detector threshold/persistence; do not tune from simulation values.

## G5 — calibration

```text
empty saddle -> stable -> zero
20.000 kg check
50.000 kg -> span
remove -> verify zero
verify 20 kg
verify 50 kg
record TLB identity/profile/operator/result
```

Calibration is separate from recipe target compensation and any future bounded cycle tare.

## G6 — network-loss independence

Controller core has no browser/cloud dependency. Edge collector and Cloudflare-ready engineering console are supervisory.

Runtime proof still requires:

```text
remove browser/network access
local FSM continues
TLB acquisition continues
physical outputs remain locally owned
restore network
no cycle reset/restart or remote output injection
```

Collector API is GET/SSE only. No actuator route is present.

## G7 — canonical bench review

Review core `SP01_CANONICAL_VIEWS.md` V1..V11 plus affected V12/V13 extensions against:

```text
controller source and automated tests
G1/G2/G2T evidence
G4/G5 weighing evidence
G6 independence evidence
board terminal map
edge/collector read-only boundary
web/engineering-console measured-data-only behavior
```

Red-team R1 checks contradictions, unmeasured constants, output ownership and failure behavior.

## G8 — machine shadow; new DO physically isolated

Precondition: G2, G2T, G3, G4, G5, G6 and G7 PASS.

Connect real DI and real TLB weight; keep SP01 physical actuator outputs isolated. Record timestamp-aligned legacy and SP01 desired behavior.

GOOD:

```text
weight/fill/cutoff/settle
skip reject ~210
legacy normal push ~355
SP01 desired push ~355
```

REJECT:

```text
TLB trace
legacy broken-bag decision
SP01 detector decision
legacy immediate fill shutdown
SP01 desired DO4..DO8 shutdown
legacy reject push ~210
SP01 desired reject push ~210
absence of later ~355 push
```

G8 freezes from measurements:

```text
broken_bag_loss_trip_kg
broken_bag_persist_us
TLB filter assumptions
210° timing/reference + actuator lead
355° timing/reference + actuator lead
post-detection scanner/bag-detect behavior
late-detection behavior
```

Red-team R2 reviews this evidence before live authority.

## G9 — controlled one-spout live pilot

Preconditions:

```text
G8 accepted
approved as-built wiring/config
legacy rollback physically available
known-good spare ready
local permit/LOTO/startup authorization
```

Enable one SP01 only. Validate both AUTO routes from recorded evidence:

```text
GOOD: normal fill -> no ~210 push -> one ~355 push
REJECT: detector -> immediate DO4..DO8 OFF -> no fill restart -> one ~210 push -> no ~355 push
```

Stop on unexplained output, wrong disposition, missed/duplicate push, unstable weight, reset/brownout, timing divergence or unavailable rollback.

G9 passes only from local site acceptance evidence.

## Evidence layout

```text
evidence/SP01/<date>/G2/RESULT.md
evidence/SP01/<date>/G2T/RESULT.md
evidence/SP01/<date>/G3/...
evidence/SP01/<date>/G4/RESULT.md
evidence/SP01/<date>/G5/RESULT.md
evidence/SP01/<date>/G6/RESULT.md
evidence/SP01/<date>/G7/RESULT.md
evidence/SP01/<date>/G8/RESULT.md
evidence/SP01/<date>/G9/RESULT.md
```

Every physical result records firmware SHA, board/spout ID, setup, measurements/log references, anomalies, operator and PASS/FAIL rationale.

## Current blocking chain

```text
software: G3 PASS; core 11-view pack + V12/V13 extensions reconciled for current known evidence
physical: G2 PASS-WAIVER -> G2T -> G4 -> G5 -> G6 -> G7 -> G8 -> G9
                            ^
                            next true commissioning blocker
```
