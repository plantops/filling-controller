# SP01 Engineering View Index

Canonical engineering views for design review, commissioning, HMI and long-term maintenance.

Primary canonical pack: [`SP01_CANONICAL_VIEWS.md`](SP01_CANONICAL_VIEWS.md).

Historical Yellow/Purple/Red-team material and the earlier `SP01_ENGINEERING_VIEWS.md` remain design/review context only. Installed-machine evidence, executable firmware and frozen interface contracts take precedence.

| View | Canonical purpose | Primary source |
|---|---|---|
| V1 | Runtime timeline and event evidence | `SP01_CANONICAL_VIEWS.md` |
| V2 | State/process matrix | `SP01_CANONICAL_VIEWS.md` |
| V3 | Interlock/process routing flow | `SP01_CANONICAL_VIEWS.md` |
| V4 | Interlock predicates; K-map non-canonical | `SP01_CANONICAL_VIEWS.md` |
| V5 | Logic dependency / authority graph | `SP01_CANONICAL_VIEWS.md` |
| V6 | Executable C++ engine / ground truth | controller source + `SP01_CANONICAL_VIEWS.md` |
| V7 | Physical I/O / terminals | `BOARD_TERMINALS.md` + `SP01_CANONICAL_VIEWS.md` |
| V8 | Exception / fault / controlled-reject matrix | `SP01_CANONICAL_VIEWS.md` + `BROKEN_BAG_REJECT.md` |
| V9 | Supervisory process projection | `SP01_CANONICAL_VIEWS.md` |
| V10 | Communication / telemetry contracts | `WEIGHING.md` + `SP01_CANONICAL_VIEWS.md` |
| V11 | Industrial digital-twin / web HMI | `SP01_CANONICAL_VIEWS.md` |
| V12 | Weighing quality, calibration, filtering and tare | `WEIGHING_SIGNAL_QUALITY.md` + `SP01_CANONICAL_VIEWS.md` |
| V13 | Eight-spout rotating/stationary topology | `EIGHT_SPOUT_SYSTEM_TOPOLOGY.md` + `SP01_CANONICAL_VIEWS.md` |

## Frozen process facts

```text
controller               ESP32-S3 / ESP-IDF / C++17 / FreeRTOS
one node                 one independent spout
weight                   LAUMAS TLB485 -> isolated RS485 -> WeightSnapshot
broken-bag detection     qualified net-weight decrease while COARSE/FINE filling is active
on detection             latch REJECT + remove DO4..DO8 in the same control decision
reject eject             DO3 bag.push near ~210 deg
healthy eject            DO3 bag.push near ~355 deg
rejected cycle           never gets the later ~355 deg push
210 deg                  reject/eject position, not a broken-bag sensor
cloud/central HMI        supervisory only; never owns local cutoff/interlock/eject timing
```

The executable branch now contains the software representation of this routing: `BagDisposition`, `State::RejectWait`, a bounded weight-loss detector and the semantic `PositionSnapshot.reject_window`. Production detector thresholds and the real 210-degree timing source remain disabled/unfrozen until G4/G8 evidence supplies them.

## Gate ownership by views

```text
G0   V6
G1   V7 + V10
G2   V1 + V7
G2T  V1 + V7 + V10
G3   V2 + V3 + V4 + V6 + V8
G4   V1 + V10 + V12
G5   V10 + V12
G6   V5 + V10 + V11
G7   all 13 views reconciled
G8   V1 + V7 + V8 + V11 + V12 + V13 shadow
G9   V1 + V8 + V11 controlled live pilot
```

## Precedence

```text
1. measured installed-machine electrical/mechanical/process evidence
2. executable C++ controller + board adapter
3. frozen I/O / weighing / reject / topology contracts
4. SP01_CANONICAL_VIEWS.md
5. generated HMI/diagram projections
6. historical team proposals
```

Numeric detector thresholds, angular windows and actuator-lead values are frozen only from gate evidence.