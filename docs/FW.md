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
HIGH    control: DI image -> latest weight snapshot -> FSM -> interlocks -> DO image
MEDIUM  weighing: RS485/TLB485 -> latest validated WeightSnapshot
LOW     web: HTTP/WebSocket + static HTML/CSS/JS
LOW     storage: config + bounded event/cycle buffer
```

Only one control context commits physical outputs. Control never waits for Wi-Fi, logging or a Modbus reply.

## Weighing transport rule

The production controller receives continuous weight digitally from TLB485 over isolated RS485. It does not read the raw load-cell bridge and does not use a DI as the weight channel.

The weighing task polls independently and publishes:

```text
WeightSnapshot {
  net_kg
  sample_time_us
  stable
  quality
  sequence
}
```

The 10 ms control loop consumes only the newest validated snapshot. A Modbus transaction may be slower than one controller tick without blocking the FSM.

Communication profiles:

```text
bring-up: 9600 bit/s, 50 ms poll
high-rate target after G4 verification: 115200 bit/s, 20 ms poll (~50 Hz)
10 ms poll: test-only after measured timing/error margin
```

The TLB and ESP serial settings must match. See [`WEIGHING.md`](WEIGHING.md).

## Filling target strategy

v0.1 deliberately keeps target adaptation operator-controlled. Recipes may differ only by target weight, for example 50.0 / 50.1 / 50.2 / 50.3 kg, while the other tuned filling parameters stay unchanged. External check-scale results guide a fast recipe switch.

Calibration is separate from recipe offset. No PID or automatic AI target correction is required for v0.1.

## Milestones

| Milestone | Deliverable | Exit gate |
|---|---|---|
| M0 Skeleton | ESP-IDF project, C++ model/types, versioned boot | G0 build |
| M1 Safe platform | board adapter, safe boot/reset/watchdog output image | G1 safe-output proof |
| M2 Process image | frozen 8DI image, desired 8DO image, single output owner | G2 dummy I/O |
| M3 FSM | canonical SP01 sequence, monotonic timeouts, fault codes | G3 dry cycle |
| M4 Weighing | nonblocking TLB485 adapter, age/quality/stability, measured transport profile | G4 TLB bench |
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
  TLB485 + load cell online through board RS485 terminal
  digital kg/status verified
  update frequency, latency, jitter and errors measured
  stale/disconnect/reconnect behavior verified
  test while representative inductive loads switch
  evaluate 115200 / 20 ms operating profile

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
  real SP01 DI + digital weight
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
Status       state, weight, active target, faults
I/O          DI1..DI8, DO1..DO8
Settings     target recipe + controller parameters
Timing       state/I-O/weight timeline
Calibration  zero / 20 kg check / 50 kg span / verify
Diagnostics  TLB, RS485 age/errors, Wi-Fi, reset reason, firmware
```

The Linux preview may show recipe/settings UX before write authority is implemented on ESP. Runtime writes must use validated service/config APIs; browser code never writes raw GPIO or raw Modbus registers.

Serve static HTML/CSS/JS from ESP32. Browser does rendering. No Pi is required for SP01 v0.1.

## Branches

```text
main            current integrated baseline
fw-sp01-v0.1    embedded firmware development, kept in sync when requested
py-sim          simulator/reference
debate          history/red-team
```
