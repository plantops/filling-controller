# SP01 Firmware v0.1

Target:

```text
ESP32-S3
ESP-IDF
C++17
FreeRTOS
```

Python stays on `py-sim` as simulator/reference.

## Runtime split

```text
HIGH    control: DI image -> weight snapshot -> FSM -> interlocks -> DO image
MEDIUM  weighing: RS485/TLB485 -> latest validated WeightSnapshot
LOW     web: HTTP/WebSocket + static HTML/CSS/JS
LOW     storage: config + bounded event/cycle buffer
```

Only one control context commits physical outputs. Control never waits for Wi-Fi, logging or a Modbus reply.

## Milestones

| Milestone | Deliverable | Exit gate |
|---|---|---|
| M0 Skeleton | ESP-IDF project, C++ model/types, versioned boot | G0 build |
| M1 Safe platform | board adapter, safe boot/reset/watchdog output image | G1 safe-output proof |
| M2 Process image | frozen 8DI image, desired 8DO image, single output owner | G2 dummy I/O |
| M3 FSM | canonical SP01 sequence, monotonic timeouts, fault codes | G3 dry cycle |
| M4 Weighing | nonblocking TLB485 adapter, age/quality/stability | G4 TLB bench |
| M5 Calibration | zero, 20 kg check, 50 kg span, zero/20/50 verify | G5 calibration |
| M6 HMI | small local web UI, WebSocket telemetry, calibration service | G6 network-loss test |
| M7 Evidence | event ring, cycle record, reset/comm/timing diagnostics | G7 bench review |
| M8 Shadow | real DI + weight, physical DO isolated, legacy comparison | G8 shadow review |
| M9 Pilot | controlled field outputs and SP01 pilot | G9 live acceptance |

## Gates

```text
G0 BUILD
  clean ESP-IDF build for ESP32-S3

G1 SAFE OUTPUT
  dummy loads only
  boot/reset/watchdog cause no unintended output

G2 DUMMY I/O
  8 DI switches + 8 dummy DO
  channel map and single output ownership verified

G3 DRY CYCLE
  full FSM on dummy I/O
  state timeout/fault paths exercised

G4 TLB BENCH
  TLB485 + load cell online
  latency/filter/noise/stale-link behavior measured

G5 CALIBRATION
  zero -> 20 kg check -> 50 kg span
  zero/20/50 verification recorded

G6 NETWORK LOSS
  AP/browser removed during all states
  local control result unchanged

G7 BENCH REVIEW
  measured timing, reset, I/O, TLB, calibration and fault evidence
  red-team R1

G8 SHADOW
  real SP01 DI + weight
  new physical DO isolated
  compare new timeline with legacy
  red-team R2

G9 LIVE
  controlled output connection
  rollback available
  SP01 pilot accepted
```

Numeric pass/fail limits are frozen from measurements before a gate is signed off; do not invent them in advance.

## HMI v0.1

```text
Status       state, weight, target, faults
I/O          DI1..DI8, DO1..DO8
Timing       state/I-O/weight timeline
Calibration  zero / 20 kg check / 50 kg span / verify
Diagnostics  TLB, Wi-Fi, reset reason, firmware
```

Serve static HTML/CSS/JS from ESP32. Browser does rendering. No Pi is required for SP01 v0.1.

No raw GPIO or raw Modbus write endpoint.

## Branches

```text
main            current spec/docs
fw-sp01-v0.1    active embedded firmware
py-sim          simulator/reference
 debate          history/red-team
```
