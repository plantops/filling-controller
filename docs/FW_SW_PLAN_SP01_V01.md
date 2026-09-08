# SP01 v0.1 Firmware / Software Plan

## Status

**Implementation plan for the one-spout bench prototype.**

The Python runtime remains the digital-twin/reference environment. Production controller behavior will be implemented separately on ESP32-S3 using ESP-IDF/C++/FreeRTOS and compared against shared conformance scenarios.

## 1. Product split

```text
PYTHON REFERENCE
  digital twin
  replay
  algorithm development
  scenario generation
  conformance oracle
  HMI prototyping

ESP32-S3 FIRMWARE
  real SP01 controller
  local DI/DO
  TLB/RS485
  monotonic timing
  watchdogs
  local event buffer
  Wi-Fi supervisory API/HMI
```

Do not turn the Python/FastAPI bootstrap into the production control runtime.

## 2. Target firmware stack

```text
ESP32-S3
ESP-IDF
C++
FreeRTOS
```

The controller core should remain portable C++ with hardware access confined to adapters/drivers where practical.

Proposed repository layout:

```text
firmware/esp32-s3/
  main/
  controller/
  io/
  weighing/
  network/
  storage/
  hmi/
  config/
  tests/

tests/conformance/
  normal_fill.yaml
  no_bag.yaml
  weight_stale.yaml
  no_flow.yaml
  position_timeout.yaml
  abort.yaml
  reset_safe_state.yaml
  ...
```

## 3. Deterministic control path

One owner writes the physical output image.

```text
READ/FREEZE INPUT IMAGE
        ↓
READ LATEST VALIDATED WEIGHT SNAPSHOT
        ↓
RUN SP01 FSM
        ↓
BUILD DESIRED OUTPUT IMAGE
        ↓
APPLY INTERLOCKS / SAFE-STATE RULES
        ↓
COMMIT DO01..DO08
```

No web, Wi-Fi, logging, storage or calibration UI task may write physical outputs directly.

## 4. Timing

Real firmware uses a monotonic clock, not synthetic `now += dt` timing.

Each state should track entry/deadline using monotonic microseconds. The runtime records:

```text
configured control period
actual control period
execution time
worst-case jitter
overrun count
watchdog/reset reason
```

Numeric timing limits are acceptance criteria to be measured and frozen during bench work rather than guessed in documentation.

## 5. TLB task and weight snapshot

TLB communication runs outside the deterministic FSM path. The control task never blocks waiting for Modbus.

Conceptual snapshot:

```text
WeightSnapshot
  value_kg
  sample_time_monotonic
  age
  stable
  status
  quality
  sequence
```

The FSM consumes only a fresh/valid snapshot. Stale or failed weighing data during filling causes a defined bounded fault/abort response.

Calibration and TLB register details remain inside the weighing adapter/service layer. See `docs/WEIGHING_CALIBRATION.md`.

## 6. v0.1 network policy

Wi-Fi is supervisory only.

Initial HMI/API policy should be deliberately narrow:

```text
READ:
  state
  I/O
  weight
  timing
  diagnostics
  faults
  local cycle history
  network health

WRITE allowed initially:
  controlled calibration workflow
  explicitly safe service operations required for commissioning

DEFER initially:
  raw GPIO write
  raw Modbus register write
  unrestricted I/O force
  live recipe changes during a cycle
  remote control-step endpoint
  unattended OTA during operation
```

Any later write capability must pass through authentication, state guards, interlocks and audit.

## 7. Calibration UI is part of firmware v0.1

Unlike most early HMI functions, calibration is a required write workflow from the first hardware integration because the weighing chain cannot be commissioned without it.

Required sequence:

```text
empty saddle → SET ZERO
20 kg known mass → CHECK
50 kg standard → SET SPAN
remove mass → VERIFY ZERO
20 kg → VERIFY
50 kg → VERIFY
SAVE + LOCK
```

The controller accepts calibration actions only in a safe service/calibration state with fresh stable weight and filling outputs disabled.

## 8. Local event buffering

The SP01 node keeps a bounded local buffer independent of Wi-Fi.

At minimum record:

```text
state transitions
DI transitions
DO command transitions
weight/cutoff events
faults
calibration events
cycle summaries
network disconnect/reconnect diagnostics
```

Records use controller-generated timestamps/sequence numbers. Upload/synchronization runs asynchronously and may never block control.

## 9. Python / firmware conformance

Shared scenarios are the portability contract.

```text
scenario inputs + weight trace + timestamps
           │
           ├── Python reference
           └── C++ controller core / ESP HIL
                    │
                    ▼
compare semantic results
```

Compare at minimum:

```text
state sequence
state transition timing within defined tolerance
semantic dosing stage
DO command transitions
cutoff decision
fault code
cycle result
```

The simulator is allowed to use richer tooling internally; it does not need to imitate FreeRTOS architecture. It must match the defined machine behavior contract.

## 10. Test ladder

### F0 — host C++ unit/conformance tests

- pure controller core;
- no ESP hardware;
- deterministic scenario replay;
- fast CI execution.

### F1 — ESP software-in-loop

- actual ESP firmware;
- virtual DI/DO and virtual weigher;
- Wi-Fi/HMI active;
- long soak / fault injection.

### F2 — dummy physical I/O bench

- 24 V switch panel on DI01..DI08;
- lamps/dummy loads on DO01..DO08;
- representative inductive loads after basic logic passes;
- physical output states instrumented during boot/reset/watchdog.

### F3 — TLB/load-cell bench

- actual TLB485;
- actual/representative load cell;
- zero/span/verification workflow;
- 20 kg and 50 kg checks;
- latency, jitter, filtering, noise, stale-link tests.

### F4 — integrated wireless bench

- AP/router and browser active;
- reconnect storms;
- AP reboot/power loss during every FSM state;
- prove zero control dependency on Wi-Fi;
- local buffer/backfill tests.

### F5 — real SP01 shadow

- real SP01 input signals and weighing data where safely available;
- new DO electrically blocked from machine actuators;
- compare legacy vs proposed state/output timeline.

### F6 — controlled live actuator pilot

Only after explicit commissioning review. Connect one low-risk output first, then expand output authority step-by-step with immediate physical rollback.

## 11. Firmware milestones

```text
M0  repository structure + host-buildable controller core
M1  process image + FSM + virtual adapters
M2  ESP DI/DO driver + safe boot/reset output behavior
M3  TLB Modbus adapter + stale-weight handling
M4  calibration service + calibration web UI
M5  read-only live HMI / timing / diagnostics
M6  bounded local event store
M7  Wi-Fi stress + watchdog/brownout instrumentation
M8  dummy-I/O and TLB bench acceptance
M9  SP01 shadow package
```

## 12. Red-team gates

Do not ask for another broad theoretical review immediately. The next useful red-team review should occur when there is evidence to attack:

```text
Gate R1 — BENCH-PILOTABLE review
  actual BOM/as-built wiring
  ESP firmware skeleton exists
  safe-output behavior implemented
  TLB adapter implemented/stubbed against real interface
  calibration workflow implemented
  numeric bench acceptance criteria defined

Gate R2 — LIVE-SP01 review
  dummy-I/O results
  TLB calibration/latency measurements
  Wi-Fi stress/RF measurements
  shadow comparison against legacy
  measured coil loads/safe states
  rollback procedure
```

R1 should focus on whether the system is safe and coherent enough for dummy physical I/O/TLB bench work. R2 should focus on whether evidence supports controlled connection to real SP01 outputs.

## 13. Immediate next work

Proceed in parallel with:

```text
PROCUREMENT
  buy one SP01 set and dummy-I/O parts

FW
  create ESP-IDF/C++ controller skeleton
  implement safe process-image architecture
  implement virtual I/O first

SW
  preserve Python reference/digital twin
  define shared conformance scenarios

HMI
  keep early supervisory views mostly read-only
  implement weighing calibration as the required controlled write workflow
```

The next engineering target is not a live bag. It is a fully observable, calibratable, fault-injectable SP01 controller operating against dummy physical I/O and a real weighing chain.