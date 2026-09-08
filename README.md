# Filling Controller

Open controller for an 8-spout rotary cement bag packer.

**Current target: SP01 hardware-ready bench prototype.**

[Tiếng Việt](README_VI.md)

```text
STATIONARY
Linux/laptop -> dedicated Wi-Fi AP/router
                         ))) supervisory only

ROTATING SP01
ESP32-S3 / ESP-IDF / C++ / FreeRTOS
  |- 8 DI / 8 DO
  |- RS485 -> LAUMAS TLB485 -> load cell
  `- local web HMI
```

Wi-Fi is not in the control loop. Controller, weighing and I/O stay local to SP01.

## Quick start — Linux amd64

```bash
git switch fw-sp01-v0.1
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host

./firmware/host/serve-hmi.sh 8080
```

Open `http://<linux-node-ip>:8080/` to review the host HMI preview and current parameter set. The host preview is mock/read-only and has no GPIO, Modbus or actuator authority.

## Quick start — ESP32-S3

Install ESP-IDF v5.5.5, then:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

Configure values under `menuconfig -> SP01 Filling Controller`. Keep calibration writes disabled for the first flash.

Detailed firmware, HMI, parameter and flashing instructions: [`firmware/README.md`](firmware/README.md).

## Operating modes

- **MANUAL** — machine stopped; `DI04 process.initiative` is the fill ON/OFF switch; fill to target and stop; bag push is hard-blocked.
- **AUTO** — machine rotating; continuous fill/discharge cycle; discharge timing is normalized from reference sensors A/B so push position follows real rotor speed.

## Current branches

| Branch | Purpose |
|---|---|
| `main` | current specification and integrated baseline |
| `fw-sp01-v0.1` | active SP01 firmware integration |
| `py-sim` | Python digital twin / replay / conformance reference |
| `debate` | historical design discussion and red-team material |

## Current documents

- [`spec/SP01.md`](spec/SP01.md) — canonical SP01 sequence and I/O
- [`docs/HW.md`](docs/HW.md) — prototype wiring and architecture
- [`docs/BOM.md`](docs/BOM.md) — one-node procurement list
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — weighing calibration
- [`docs/FW.md`](docs/FW.md) — firmware gates and milestones
- [`firmware/README.md`](firmware/README.md) — build, simulation, HMI and flashing
- [`hardware/README.md`](hardware/README.md) — vendor links, manuals and photo index

## Hardware gate order

```text
Linux amd64 simulation + HMI review
-> ESP32 boot / ALL DO safe OFF
-> dummy 24 V DI/DO
-> TLB485 + load cell calibration
-> MANUAL/AUTO dry cycle
-> Wi-Fi loss / reboot / fault injection
-> SP01 shadow
-> controlled machine connection
```
