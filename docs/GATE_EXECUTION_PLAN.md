# SP01 G2 -> G9 execution plan

Branch: `diag/sp01-g2-g9`
Baseline: `c8bbfa8a0a92f0d899497aa4d8b2e41647b9811c`
Date started: 2026-09-10

## Operating rule

Advance a gate only from recorded evidence. CI success is not a substitute for physical evidence. Machine actuators stay disconnected until G8 shadow is complete and G9 live connection is explicitly authorized locally.

Status values: `PASS`, `ACTIVE`, `BLOCKED-HW`, `PENDING`, `FAIL`.

| Gate | Scope | Current status | Exit evidence |
|---|---|---|---|
| G0 | Build | PASS | clean host tests + ESP32-S3 build |
| G1 | Safe board bring-up | PASS (self-test scope) | boot, TCA9554 safe latch, DI idle, W5500 SPI/link/DHCP, no reset in capture |
| G2 | Physical dummy I/O | ACTIVE | 1 h clean soak, DI1..8 dry-contact truth table, DO1..8 dummy-load OFF/ON/reset-OFF, sustained Ethernet path |
| G2T | Thermal/serviceability | BLOCKED-HW | measured installation temp, elevated-temp soak, reset/fault safe-output evidence, spare-swap/config recovery drill |
| G3 | Dry FSM cycle | ACTIVE-SW | deterministic AUTO/MANUAL full cycles + every timeout/fault path + GOOD/~355° vs REJECT/~210° disposition paths on virtual/dummy I/O |
| G4 | TLB485 bench | BLOCKED-HW | digital kg/status, measured poll latency/jitter/error rate, stale/disconnect/reconnect, switching-noise trial |
| G5 | Calibration | BLOCKED-HW | zero, 20 kg check, 50 kg span, zero/20/50 verification |
| G6 | Network loss | PENDING | local FSM behavior unchanged when browser/network disappears during each state |
| G7 | Bench review | PENDING | consolidated measured evidence + red-team R1, no unresolved critical finding |
| G8 | Shadow | BLOCKED-HW | real DI + TLB weight, new DO physically isolated, timeline comparison with legacy including broken-bag reject ~210° and healthy-bag eject ~355°, red-team R2 |
| G9 | Live pilot | BLOCKED-HW | controlled DO connection, rollback ready, known-good spare ready, both disposition paths accepted on SP01 pilot |

## Execution order

### Track A — software/CI, run immediately

1. Freeze `c8bbfa8` diagnostic baseline and preserve G1 evidence.
2. Expand host tests from happy-path coverage to the complete G3 fault/timeout matrix.
3. Add the two bag-disposition routes: healthy/good -> normal push near 355°; broken/reject -> early push near 210°. Do not implement a fake 210° sensor; first define a testable broken-bag detection event and a position-reference abstraction.
4. Make virtual-bench state exchange race-safe before using it as gate evidence.
5. Add deterministic network-loss test hooks so G6 is testable without raw GPIO writes.
6. Add structured evidence output for state transitions, reset reason, TLB diagnostics, cycle timing, bag disposition and reject/normal push timing for G7/G8 comparison.
7. Keep `CONFIG_SP01_VIRTUAL_IO` branch-only; production defaults remain OFF.
8. CI on every change: host tests + ESP32-S3 build. No merge to `main` until the relevant physical gate is signed.

### Track B — physical bench, local technician required

Run only with machine wiring disconnected:

1. G2 precondition: `g1_usb_probe.py --seconds 3600 --log soak.txt`; require continuous heartbeat, flat heap trend, no reset/link flap/lease loss.
2. G2a: DI1..8, one dry contact at a time, record OPEN/CLOSED/OPEN.
3. G2b: one 24 V dummy lamp/test load, DO1..8 one-hot pulse, record OFF/ON/reset-OFF and no cross-channel actuation.
4. G2 network: sustained HMI/API traffic while heartbeat continues; record packet/application failures and recovery.
5. G2T: measure real mounting temperature, then controlled elevated-temperature test where suitable equipment is available; repeat boot/reset/fault and representative I/O/network load.
6. Spare drill: swap to a pre-flashed controller, restore approved config, verify TLB calibration remains unchanged.
7. G4/G5 start only when TLB485 + load cell + known test masses are present.

### Track C — machine-adjacent, no output authority

1. G8 only after G2/G2T/G3/G4/G5/G6/G7 pass.
2. Connect real DI and TLB485 weight.
3. Keep new physical DO isolated.
4. Capture timestamped legacy and SP01 timelines for identical cycles.
5. Map the actual broken-bag detection source separately from the 210° reject position.
6. Compare healthy bags through normal ~355° push and broken/reject cases through early ~210° push; freeze the actual angular/timing windows and actuator lead from measured data.
7. Verify that a rejected cycle suppresses the later normal ~355° push and that a good cycle never commands the ~210° reject push.
8. Red-team R2 reviews wiring assumptions, fail-safe behavior, timing, disposition latching and rollback.

### Track D — controlled live pilot

1. Approve one spout only.
2. Confirm legacy rollback path and tested spare controller are physically available.
3. Connect controlled outputs channel-by-channel under local permit/LOTO/startup procedure.
4. Run low-risk manual/dry checks first, then controlled production cycles.
5. Validate healthy-bag normal eject near ~355° and, only under an approved safe test method, the broken-bag/reject path near ~210°.
6. Stop on unexplained output, wrong disposition, duplicate push, unstable weight transport, reset/brownout, timing divergence, or failed rollback drill.
7. G9 passes only after the site owner accepts the pilot from recorded evidence.

## G3 required software matrix

The host/virtual suite must prove safe outputs for at least:

- AUTO normal full cycle for a GOOD bag: no reject push near 210°, normal push near ~355°;
- AUTO REJECT cycle: broken-bag event is latched to the current cycle, early push near ~210°, later normal ~355° push suppressed;
- reject event associated with the wrong/previous cycle is not allowed to eject another spout/cycle;
- missing/invalid 210° timing reference follows a defined bounded fault/hold behavior before production authority;
- detection arriving too late for the 210° window follows a defined bounded behavior before production authority;
- MANUAL normal full cycle with no automatic bag push;
- manual OFF during filling;
- mode change during active cycle;
- auto permissive loss during active cycle;
- bag acquire timeout;
- bag lost after acquisition;
- stale weight at tare/coarse/fine/settle;
- weight transmitter fault;
- coarse timeout;
- fine timeout;
- discharge-reference timeout;
- invalid B-before-A discharge timing;
- reset/forced I/O fault -> all outputs OFF;
- fault clear rejected while process initiative is ON, accepted only with safe condition.

The software test hook may inject a semantic `broken_bag_detected` event. It must not pretend that this event is physically located at 210°; G8 maps the real detector/interface.

## Evidence layout

Commit gate evidence under:

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

Each gate result should contain `RESULT.md` with firmware SHA, board ID, test setup, measured results, anomalies, operator, and PASS/FAIL rationale. Photos/logs may be referenced rather than embedded when repository size would become excessive.

For G8/G9 bag-disposition evidence also record:

```text
spout_id
cycle_id
disposition GOOD/REJECT
broken-bag detection source + timestamp
reference/timing source
scheduled eject window
legacy push timestamp
SP01 desired/actual push timestamp
whether a second/incorrect push occurred
```

## Current boundary

Software can proceed through G3 preparation and G6/G7 instrumentation now. Physical completion of G2, G2T, G4, G5, G8 and G9 cannot be claimed from CI or simulation; those gates wait for local bench/plant evidence.

The early broken-bag reject requirement means G3 cannot be considered complete until the two-disposition software behavior is implemented and tested.