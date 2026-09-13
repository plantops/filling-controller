# SP01 Engineering View Index

Canonical engineering views for design review, commissioning, HMI and long-term maintenance.

Primary canonical pack: [`SP01_CANONICAL_VIEWS.md`](SP01_CANONICAL_VIEWS.md).

The project uses **eleven core views (V1..V11)** as the common control/commissioning language. V12 and V13 are retained as commissioning extensions for weighing quality/calibration and whole-machine topology.

Historical Yellow/Purple/Red-team material and the old `SP01_ENGINEERING_VIEWS.md` are review context only. Installed-machine evidence, executable firmware and frozen interface contracts take precedence.

| View | Canonical purpose | Primary source | Current review |
|---|---|---|---|
| V1 | Runtime timeline and event evidence | `SP01_CANONICAL_VIEWS.md` | complete schema; runtime event stream partial |
| V2 | State/process matrix | controller source + canonical pack | complete |
| V3 | Interlock/process routing flow | controller source + canonical pack | complete; real 210 adapter waits G8 |
| V4 | Interlock predicates / detector equations | controller source + canonical pack | complete; high-water/persistence detector |
| V5 | Logic dependency / authority graph | canonical pack | complete |
| V6 | Executable C++ engine / ground truth | controller source + canonical pack | complete |
| V7 | Physical I/O / terminals | `BOARD_TERMINALS.md` + canonical pack | map complete; DI PASS; DO evidence active |
| V8 | Exception / fault / controlled-reject matrix | `BROKEN_BAG_REJECT.md` + canonical pack | complete |
| V9 | Supervisory process projection | canonical pack | complete |
| V10 | Communication / telemetry contract | embedded HMI + collector + canonical pack | complete contract; telemetry partial |
| V11 | Industrial digital-twin / web HMI | `web/engineering-console/` + canonical pack | complete UI contract |
| V12 | Extension: weighing quality, calibration, filtering, tare | `WEIGHING_SIGNAL_QUALITY.md` + canonical pack | gate-driven |
| V13 | Extension: eight-spout rotating/stationary topology | `EIGHT_SPOUT_SYSTEM_TOPOLOGY.md` + canonical pack | gate-driven |

## Frozen process facts

```text
controller               ESP32-S3 / ESP-IDF / C++17 / FreeRTOS
one node                 one independent spout
weight                   LAUMAS TLB485 -> isolated RS485 -> WeightSnapshot
broken-bag detection     running high-water measured-weight loss + new-sample persistence
on detection             latch REJECT + remove DO4..DO8 in the same control decision
AUTO reject eject        DO3 bag.push near ~210 deg after semantic reject window
AUTO healthy eject       DO3 bag.push near ~355 deg after normal A/B timing
MANUAL reject            REJECT + fill stop + COMPLETE; no automatic eject
rejected AUTO cycle      never gets the later ~355 deg push
210 deg                  reject/eject position, not a broken-bag sensor
cloud/central HMI        supervisory only; never owns local cutoff/interlock/eject timing
```

Production detector thresholds and the real 210-degree timing source remain disabled/unfrozen until G4/G8 evidence supplies them.

## I/O evidence status

```text
G2 one-hour USB/Ethernet soak    PASS
G2 DI1..DI8 physical truth      PASS
G2 DO physical/loopback truth   ACTIVE
G2 reset/restart safe outputs   PENDING
```

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
G7   core V1..V11 reconciled + affected extensions
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
