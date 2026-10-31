# SP01 G2T — thermal/serviceability preparation

Date: 2026-09-11
Branch: `diag/sp01-g2-g9`
Status: **PREPARED / BLOCKED-HW — not PASS**

G2T is the next true commissioning gate after G2. G2 physical DO/reset-safe evidence is intentionally deferred because the operator is away from the board; that deferred evidence is not counted as PASS.

## Purpose

Prove that the SP01 controller remains serviceable and returns to a safe output state under representative installed conditions. This gate must use measured physical evidence; CI, simulation and PCB/HMI state do not substitute for it.

## Setup

- Machine actuator/solenoid/contactor wiring remains disconnected.
- Use the intended SP01 controller, power arrangement and enclosure/mounting arrangement as close as practical to installation.
- Record firmware SHA / build profile before the run.
- Record ambient and controller/enclosure temperature with the actual instrument used.
- Representative traffic should include the interfaces available at the time of test: control loop, Ethernet/HMI traffic, DI activity and RS485/TLB traffic when the TLB is connected.

## Evidence to collect

```text
identity
  board/spout ID
  firmware SHA
  build profile/config revision
  power source
  enclosure/mounting condition

temperature
  ambient at start/end
  controller/enclosure measurement point
  temperature at start/end and during steady operation
  measurement instrument

runtime
  start/end uptime
  reset reason
  free/minimum heap when available
  Ethernet link/IP behavior
  TLB poll/error/latency counters when connected
  anomalies/restarts/watchdog/brownout observations

safe-output behavior
  boot -> all outputs safe
  explicit reset/restart -> outputs return safe
  induced software/fault-safe path when practical -> outputs safe
  no unexplained DO transition

serviceability
  power-down / remove controller / install pre-flashed spare
  restore approved config
  confirm identity and communications
  verify ESP replacement does not alter TLB calibration
```

## Acceptance rule

No numeric temperature or lifetime limit is invented here. G2T can pass only after the installed operating range and observed behavior are measured and judged acceptable for the actual installation. Any reset, brownout, unexplained output transition, communication instability correlated with temperature, or inability to recover configuration remains an open finding.

## Current blocker

```text
operator not physically near SP01 board
G2 physical DO/reset-safe evidence deferred
installed temperature/serviceability evidence unavailable
```

Verdict: **BLOCKED-HW / PREP COMPLETE**.
