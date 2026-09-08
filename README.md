# Filling Controller

Open controller for an 8-spout rotary cement bag packer.

**Current target: SP01 bench prototype.**

[Tiếng Việt](README_VI.md)

```text
STATIONARY
Laptop -> dedicated Wi-Fi AP/router
                     ))) supervisory only

ROTATING SP01
ESP32-S3 / ESP-IDF / C++ / FreeRTOS
  |- 8 DI / 8 DO
  |- RS485 -> LAUMAS TLB485 -> load cell
  `- local web HMI
```

Wi-Fi is not in the control loop. The controller, weighing link and I/O are local to SP01.

## Current branches

| Branch | Purpose |
|---|---|
| `main` | current specification and hardware documents |
| `fw-sp01-v0.1` | ESP32-S3 firmware |
| `py-sim` | Python digital twin / replay / conformance reference |
| `debate` | historical design discussion and red-team material |

## Current documents

- [`spec/SP01.md`](spec/SP01.md) — canonical SP01 sequence and I/O
- [`docs/HW.md`](docs/HW.md) — prototype wiring and architecture
- [`docs/BOM.md`](docs/BOM.md) — one-node procurement list
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — weighing calibration
- [`docs/FW.md`](docs/FW.md) — firmware plan
- [`hardware/README.md`](hardware/README.md) — vendor links, manuals and photo index

## Build order

```text
dummy 24 V DI/DO
-> TLB + load cell calibration
-> dry-cycle bench
-> Wi-Fi stress
-> SP01 shadow
-> review
-> controlled machine connection
```
