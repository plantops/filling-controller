# Project Evolution

This file records the design decisions that materially changed the project. It exists so the repository shows **why** the current structure looks different from the original Python bootstrap.

## 2026-09 — Phase 0: digital-twin bootstrap

The project started as a language-neutral filling-controller contract with a Python reference runtime. The early goal was to prove semantic I/O, state-machine behavior, simulated filling physics, soft sensors and an observable web HMI before committing to hardware.

Key decisions:

- machine behavior is the permanent product, not Python;
- plant truth, sensor measurement and controller estimate are separate domains;
- simulation, replay, shadow and real adapters should share the same semantic contract;
- predictive endpoint/cutoff control is preferred over conventional PID for the existing coarse/fine/off pneumatic mechanism.

The Python implementation is preserved on branch `py-sim`.

## Phase 1: legacy machine normalization

Physical interpretation converged on one controller **per spout**, not one controller for all eight spouts.

At approximately 2,000 bags/h:

```text
machine bag interval ≈ 1.8 s
one spout revolution/cycle ≈ 14.4 s
```

The legacy output numbering reaches `OUTPUT_9`, but `OUTPUT_4` is unused. The eight active functions are remapped contiguously to the new local DO channels while legacy numbering remains migration metadata only.

## Phase 2: industrial weighing boundary

The production concept moved away from HX711-class direct MCU weighing.

Current boundary:

```text
load cell -> industrial weighing transmitter -> digital fieldbus -> controller
```

LAUMAS TLB485 is the v0.1 prototype choice. TLB-specific registers are confined to one adapter. The controller owns sequencing, timing, fault policy and prediction; the weighing unit owns excitation, A/D, calibration/filtering and weight/status reporting.

Calibration became a first-class product workflow with zero, 20 kg verification and 50 kg span/verification.

## Phase 3: rotating-machine constraint

The controller sits on the rotating packer, so a normal stationary Ethernet cable is not practical. Ordinary carbon brushes/slip-ring contacts are not accepted as an Ethernet path.

v0.1 therefore uses:

```text
stationary: dedicated AP/router
rotating:   autonomous SP01 ESP32-S3 + local TLB + local I/O
link:       supervisory Wi-Fi only
```

Wi-Fi is deliberately outside the deterministic control path.

## Phase 4: SP01 hardware freeze

Prototype scope was reduced to one evidence-producing node:

```text
SP01 only
8 local DI
8 local DO
local RS485 -> TLB485
Wi-Fi STA -> dedicated stationary AP/router
dummy physical I/O first
real machine connection later
```

Rotating 24 VDC is assumed available for planning, but voltage quality, current capacity, grounding and transients still require measurement before field connection.

Do not buy or build all eight nodes until SP01 passes the gates.

## Phase 5: red-team separation of simulator and controller

The Wi-Fi review correctly found that the existing Python/FastAPI runtime must not be treated as production-controller evidence. It shares an asyncio/network runtime, uses simulation-oriented timing and contains prototype write paths unsuitable for live control.

The architectural response is separation, not an attempt to turn the simulator into embedded firmware:

```text
branch py-sim
  Python digital twin / replay / algorithm lab / conformance oracle

branch fw-sp01-v0.1
  ESP-IDF + C++ + FreeRTOS production-controller implementation

main
  current contract, hardware, BOM, calibration, reviews and plans
```

Both implementations are expected to converge through shared conformance scenarios rather than source-language similarity.

## Current evidence gates

```text
P1 dummy DI/DO bench
P2 TLB + load-cell calibration/latency bench
P3 integrated dry cycle + Wi-Fi stress
P4 rotating RF survey + real-input shadow
R1 evidence-based bench/shadow red-team review
P5 controlled real-output commissioning
R2 evidence-based live-SP01 red-team review
P6 full SP01 pilot
P7 only then decide x8 replication
```

The next useful design information comes from measurements: safe boot/reset outputs, coil current/inrush, TLB latency/filter behavior, calibration repeatability, controller jitter, RF quality around 360° rotation and shadow comparison against the proven legacy controller.
