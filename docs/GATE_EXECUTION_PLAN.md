# SP01 G2 -> G9 Gate Execution Plan

Branch: `diag/sp01-g2-g9`
Baseline: `c8bbfa8a0a92f0d899497aa4d8b2e41647b9811c`
Date started: 2026-09-10

## Operating rule

Advance a gate only from recorded evidence. CI success is not physical evidence. Machine actuators remain disconnected until G8 shadow is complete and G9 live connection is explicitly authorized locally.

Status values: `PASS`, `ACTIVE`, `ACTIVE-SW`, `BLOCKED-HW`, `PENDING`, `FAIL`.

## Gate status

| Gate | Scope | Status | Exit evidence |
|---|---|---|---|
| G0 | Build | PASS | clean host tests + ESP32-S3 build |
| G1 | Safe board bring-up | PASS (self-test scope) | boot, TCA9554 safe latch, DI idle, W5500 SPI/link/DHCP, no reset in capture |
| G2 | Physical dummy I/O | ACTIVE | 1 h clean soak, DI1..8 dry-contact truth table, DO1..8 dummy-load OFF/ON/reset-OFF, sustained Ethernet path |
| G2T | Thermal/serviceability | BLOCKED-HW | measured installation temp, elevated-temp soak, reset/fault safe-output evidence, spare-swap/config recovery drill |
| G3 | Dry FSM / process logic | ACTIVE-SW | deterministic AUTO/MANUAL cycles, fault matrix, broken-bag detection, immediate fill shutdown, GOOD/~355° and REJECT/~210° paths |
| G4 | TLB485 bench | BLOCKED-HW | digital kg/status, poll latency/jitter/error rate, filter/noise envelope, stale/disconnect/reconnect, switching-noise trial |
| G5 | Calibration | BLOCKED-HW | zero, 20 kg check, 50 kg span, zero/20/50 verification |
| G6 | Network loss | PENDING | local control result unchanged when browser/network disappears during every relevant state |
| G7 | Bench design review | PENDING | canonical view pack + measured evidence + red-team R1, no unresolved critical finding |
| G8 | Shadow on machine | BLOCKED-HW | real DI + TLB weight, new DO isolated, timeline comparison with legacy for GOOD and REJECT cycles, red-team R2 |
| G9 | Live SP01 pilot | BLOCKED-HW | controlled DO connection, rollback and spare ready, both normal and reject paths accepted locally |

## Canonical views used by the gates

The normative index is `ENGINEERING_VIEW_INDEX.md`.

```text
G2   V1 + V7
G2T  V1 + V7 + V10 + V13
G3   V1 + V2 + V3 + V4 + V5 + V6 + V8 + V12
G4   V1 + V10 + V12
G5   V10 + V11 + V12
G6   V1 + V6 + V11
G7   V1..V13 consolidated review
G8   V1 + V2 + V7 + V8 + V11 + V12 + V13
G9   V1 + V7 + V8 + V11 + V13
```

No diagram or HMI view may be used as gate proof unless it is backed by executable or measured evidence appropriate to that gate.

## Frozen broken-bag process contract

```text
Detection:
  while COARSE_FILL or FINE_FILL is active,
  measured weight falls persistently because loss > incoming fill.

On accepted detection:
  latch disposition = REJECT for this spout_id + cycle_id
  immediately command OFF:
    DO4 dosing.valve_a
    DO5 dosing.valve_b
    DO6 dosing.valve_c
    DO7 filling.motor
    DO8 spout.aeration

Then:
  REJECT -> one bag.push near ~210°, suppress later ~355° push
  GOOD   -> no push near ~210°, one normal bag.push near ~355°
```

Detector thresholds and angular timing remain commissioning values. A one-sample negative derivative is not sufficient evidence.

## G2 — physical dummy I/O

Machine wiring disconnected.

Required:

```text
1 h heartbeat/network soak
DI1..8 OPEN -> CLOSED -> OPEN one channel at a time
DO1..8 dummy-load one-hot pulse
reset -> all physical DO safe
sustained HMI/API Ethernet traffic while heartbeat continues
```

Evidence goes to `evidence/SP01/<date>/G2/RESULT.md`.

## G2T — thermal and serviceability

Required physical evidence:

```text
actual controller/TLB mounting temperature
elevated-temperature representative I/O + network + RS485 load
reset/brownout/fault behavior
safe-output verification
one spare-controller replacement drill
approved config recovery
TLB calibration unchanged by ESP replacement
```

Do not invent a temperature qualification beyond measured hardware evidence.

## G3 — dry FSM and process logic

G3 is the main software gate before TLB/machine authority.

Required deterministic matrix:

```text
AUTO healthy full cycle
MANUAL healthy full cycle; no automatic bag.push
manual OFF during filling
mode change during active cycle
auto permissive loss
bag acquire timeout
bag lost after acquisition
WeightStale at prefill/coarse/fine/settle
WeightFault
coarse timeout
fine timeout
discharge-reference timeout
invalid discharge timing
forced I/O fault/reset -> safe output image
fault clear rejected/accepted under defined conditions
```

Broken-bag cases are mandatory:

```text
normal increasing weight                       -> GOOD
noisy but net increasing weight                -> GOOD
single negative spike                          -> no REJECT
sustained negative finite-window delta in fill -> REJECT
negative delta outside fill                    -> no broken-bag decision
REJECT accepted                                -> DO4..DO8 OFF in same control decision
REJECT latched                                 -> fill outputs never reopen in that cycle
REJECT                                         -> one push at simulated ~210°, none at ~355°
GOOD                                           -> no push at ~210°, one push at ~355°
wrong/previous cycle reject event              -> must not eject another spout/cycle
late reject decision                           -> deterministic bounded behavior
missing/invalid 210° reference                 -> deterministic bounded behavior
```

Software evidence must record:

```text
detector decision timestamp
output-image transition timestamp
disposition latch
210°/355° simulated timing result
number of push commands per cycle
```

G3 cannot PASS while the current executable controller still lacks the reject branch.

## G4 — TLB485 dynamic weighing

Required measurements:

```text
0 / 20 / 40 / 49 / 50 kg readings as applicable
actual update frequency
latency and jitter
TLB filtering profile
noise during stationary and representative vibration/switching
finite-window negative-delta noise envelope
Modbus error count
stale detection
disconnect/reconnect behavior
```

These measurements are used to freeze broken-bag detector parameters rather than guessing `loss_trip_kg`, window or debounce.

## G5 — calibration

Required:

```text
zero
20 kg check
50 kg span
verify zero / 20 / 50 kg
record TLB identity, profile, operator and result
```

Calibration is separate from recipe target compensation and from per-cycle tare.

## G6 — network-loss independence

During each relevant FSM phase, remove browser/network access and prove:

```text
control loop continues locally
weight acquisition continues locally
outputs follow local FSM only
network return does not reset/restart a cycle
```

HMI is supervisory only.

## G7 — canonical bench review

G7 is not another simulation test. It is the review checkpoint that all standard views agree with code and measured evidence.

Required review pack:

```text
V1 runtime timeline
V2 state matrix
V3 interlock flow
V4 predicates/equations
V5 dependency graph
V6 executable controller
V7 physical I/O
V8 exception/fault/reject matrix
V9 supervisory projection
V10 communication/data map
V11 digital-twin HMI
V12 weighing/calibration/filter/tare view
V13 eight-spout topology
```

Red-team R1 checks inconsistencies, undocumented assumptions and failure behavior.

## G8 — machine shadow, no new output authority

Precondition: G2, G2T, G3, G4, G5, G6 and G7 PASS.

Connect real DI and TLB485 weight, keep new physical DO isolated.

Capture identical legacy and SP01-shadow cycles. For broken-bag behavior record:

```text
spout_id + cycle_id
raw/filtered weight trace
detector decision timestamp
legacy immediate fill shutdown timestamp
SP01 desired DO4..DO8 shutdown timestamp
legacy reject push near ~210°
SP01 desired reject push near ~210°
absence of later ~355° push for REJECT
healthy bag normal ~355° push
```

G8 freezes from measurements:

```text
detection window
loss threshold
filter/persistence parameters
210° reject window and actuator lead
355° normal window and actuator lead
post-detection scanner/bag-detect behavior
late-detection behavior
```

Red-team R2 reviews the final shadow evidence before live authority.

## G9 — controlled live SP01 pilot

One spout only.

Preconditions:

```text
G8 accepted
legacy rollback path available
known-good spare available
approved as-built wiring/config
local commissioning/LOTO/startup authorization
```

Live acceptance must prove both routes:

```text
GOOD:
  normal fill
  no push ~210°
  one push ~355°

REJECT:
  accepted weight-loss detection
  immediate DO4..DO8 shutdown
  no fill restart in same cycle
  one push ~210°
  no second push ~355°
```

Stop the pilot on unexplained output, wrong disposition, duplicate/missed push, unstable weight transport, reset/brownout, timing divergence or failed rollback.

G9 passes only from recorded local acceptance evidence.

## Evidence layout

```text
evidence/SP01/<date>/G2/
evidence/SP01/<date>/G2T/
evidence/SP01/<date>/G3/
evidence/SP01/<date>/G4/
evidence/SP01/<date>/G5/
evidence/SP01/<date>/G6/
evidence/SP01/<date>/G7/
evidence/SP01/<date>/G8/
evidence/SP01/<date>/G9/
```

Every `RESULT.md` records firmware SHA, board ID, setup, measurements, anomalies, operator and PASS/FAIL rationale.

## Current boundary

```text
G0   PASS
G1   PASS
G2   ACTIVE — physical bench evidence pending
G2T  BLOCKED-HW
G3   ACTIVE-SW — reject branch/immediate fill shutdown not yet executable
G4   BLOCKED-HW
G5   BLOCKED-HW
G6   PENDING
G7   PENDING
G8   BLOCKED-HW
G9   BLOCKED-HW
```

The next engineering effort stays centered on canonical views and gate evidence. No new feature is promoted simply because it appears in a team proposal.