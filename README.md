# Filling Controller

Open, portable controller architecture for an 8-spout rotary cement bag packer.

The project began as a Python digital-twin/reference runtime and has now moved into a hardware-first SP01 prototype phase. The permanent product is the **machine behavior contract**: semantic I/O, state machine, weighing behavior, calibration, timing, safety boundaries, adapters and conformance scenarios. Runtime implementations are replaceable.

## Current status — v0.1 SP01

**Bench/shadow prototype only. Not approved for live packer actuation.**

The current build is intentionally one spout only:

```text
STATIONARY
engineering laptop
      |
dedicated AP/router
      ))) 2.4 GHz supervisory Wi-Fi only

ROTATING — SP01
ESP32-S3 + ESP-IDF/C++/FreeRTOS
  |-- 7 used + 1 spare isolated DI
  |-- 8 local protected DO
  |-- isolated RS485 -> LAUMAS TLB485 -> load cell
  |-- embedded web HMI
  `-- local deterministic FSM
```

There is no Ethernet data link across the rotating/stationary boundary. Wi-Fi is supervisory only; AP/router or browser loss must have zero effect on local cutoff, interlocks, timeout behavior or safe abort.

The first physical connection is **dummy 24 V I/O**, not the packer. Real SP01 wiring is connected only after bench and shadow gates pass.

## Branch map

| Branch | Role |
|---|---|
| `main` | current architecture, hardware specification, BOM, calibration contract and implementation plan |
| `py-sim` | preserved Python digital twin/reference runtime and prototype HMI |
| `fw-sp01-v0.1` | ESP32-S3 firmware implementation branch; created from the clean hardware/spec baseline |

Python is no longer the candidate production controller. It remains valuable as the simulator, replay environment and conformance oracle.

## Frozen SP01 hardware direction

- **Controller:** ESP32-S3 industrial 8DI/8DO carrier; current prototype candidate is Waveshare `ESP32-S3-POE-ETH-8DI-8DO`.
- **Weigher:** LAUMAS `TLB485`, local RS485 Modbus RTU.
- **Network:** one dedicated stationary 2.4 GHz AP/router; SP01 is a Wi-Fi STA/client.
- **Power:** rotating 24 VDC is assumed available; quality/current/grounding still require measurement before machine connection.
- **Local outputs:** all 8 DO are occupied; optional future RS485 expansion is auxiliary only.
- **Safety:** existing hardwired/E-stop architecture remains independent of ESP firmware and Wi-Fi.

See [hardware/README.md](hardware/README.md) and [docs/BOM_SP01_V01.md](docs/BOM_SP01_V01.md).

## Weighing and calibration

Calibration is a required firmware/HMI function, not a hidden register procedure.

Commissioning workflow:

```text
empty saddle -> SET ZERO
20.000 kg known reference -> CHECK/CAPTURE
50.000 kg standard -> SET SPAN
remove weight -> VERIFY ZERO
20 kg -> VERIFY
50 kg -> VERIFY
SAVE + AUDIT
```

The UI calls a `WeighingUnit`/TLB adapter. The browser never writes raw TLB registers. Calibration is allowed only in a validated stopped/service state with outputs disabled and stable weight.

See [docs/WEIGHING_CALIBRATION.md](docs/WEIGHING_CALIBRATION.md).

## Firmware / software split

```text
PYTHON (`py-sim`)
  digital twin
  replay
  scenario generation
  algorithm development
  HMI prototyping
  conformance oracle

ESP32-S3 (`fw-sp01-v0.1`)
  real SP01 controller
  deterministic process image / FSM
  local DI/DO
  TLB/RS485
  monotonic timing
  watchdogs
  local event buffer
  Wi-Fi supervisory HMI/API
```

The control task never waits for Wi-Fi, browser, storage upload or a fresh Modbus transaction. It consumes the latest validated local snapshots and enforces explicit stale-data handling.

See [docs/FW_SW_PLAN_SP01_V01.md](docs/FW_SW_PLAN_SP01_V01.md).

## Prototype gates

```text
P0  Python digital twin / specification
P1  ESP32 + dummy 24 V DI/DO
P2  TLB + load cell + calibration/latency bench
P3  full dry FSM + Wi-Fi/network stress
P4  rotating RF survey + real-input SHADOW SP01
R1  red-team BENCH/SHADOW review using measured evidence
P5  one low-risk real output at a time
R2  red-team LIVE-SP01 review
P6  full SP01 pilot
P7  decide x8 replication architecture
```

Do not procure or build SP02–SP08 until SP01 passes the relevant gates.

## Core documents

- [Hardware prototype v0.1](docs/PROTOTYPE_HW.md)
- [SP01 BOM / procurement](docs/BOM_SP01_V01.md)
- [Hardware links, manuals and photo index](hardware/README.md)
- [Weighing calibration](docs/WEIGHING_CALIBRATION.md)
- [Firmware/software plan](docs/FW_SW_PLAN_SP01_V01.md)
- [Embedded HMI](docs/HMI.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Machine specification](docs/SPECIFICATION.md)
- [Adapters](docs/ADAPTERS.md)
- [Simulation contract](docs/SIMULATION.md)
- [Project evolution](docs/EVOLUTION.md)
- [Prototype red-team checklist](docs/REDTEAM_PROTOTYPE_HMI.md)
- [SP01 Wi-Fi red-team checklist](docs/REDTEAM_WIFI_SP01.md)
- [SP01 Wi-Fi review response](docs/REDTEAM_WIFI_SP01_REVIEW.md)

## Hardware reference links

- Waveshare controller product: https://www.waveshare.com/esp32-s3-poe-eth-8di-8do.htm
- Waveshare documentation/wiki: https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO
- LAUMAS TLB485: https://www.laumas.com/en/product/tlb-485-digital-weight-transmitter-rs485/
- TP-Link Archer C64 support/manuals (prototype AP/router candidate): https://www.tp-link.com/vn/support/download/archer-c64/v1/

Vendor links are references, not production approval. Exact part revision, electrical ratings and manuals must be captured in the SP01 as-built record.

## Design rule

> **Unplug the AP/router while SP01 is filling. Local controller behavior must remain correct because the network was never part of the control loop.**
