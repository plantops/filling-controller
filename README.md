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
  |- isolated RS485 -> LAUMAS TLB485 -> load cell
  `- local web HMI
```

Wi-Fi is not in the control loop. Controller, weighing and I/O stay local to SP01.

## Current status

Version marker: [`VERSION`](VERSION) = **`0.1.0-rc1`**. The existing `v0.1.0-rc1` tag remains the frozen RC baseline; `main` may contain later design/document updates.

Software is **READY FOR BENCH**:

- shared C++17 controller core builds/tests on Linux amd64;
- ESP32-S3 firmware builds with ESP-IDF v5.5.5;
- MANUAL fill-only and AUTO continuous modes implemented;
- discharge timing is low-complexity at fixed revolution time; current A/B references provide current-revolution speed measurement;
- Waveshare 8DI/8DO adapter, TLB485 layer, ESP web HMI and calibration service compile successfully;
- the main remaining technical measurement is reliable load-cell digital transport from TLB485 to the controller over RS485.

Physical gates are still pending. No real machine authority is implied by the RC.

See [`progress.md`](progress.md).

## Weighing design — locked boundary

```text
load cell bridge
   -> LAUMAS TLB485
   -> isolated RS485 terminal on ESP32 board
   -> asynchronous digital WeightSnapshot
   -> controller
```

Rules:

- no production raw-load-cell ADC on ESP32;
- continuous weight does **not** use a DI;
- all eight DIs stay assigned to machine signals;
- control never blocks on Modbus;
- conservative bring-up is 9600 bit/s / 50 ms polling;
- after G4 transport measurements, target high-rate profile is 115200 bit/s / 20 ms polling (~50 updates/s).

Canonical detail: [`docs/WEIGHING.md`](docs/WEIGHING.md).

## Target recipe strategy

Keep production compensation simple and operator-controlled. Other tuned filling parameters may stay unchanged while target recipes are cloned as:

```text
50.0 kg
50.1 kg
50.2 kg
50.3 kg
...
```

The operator checks bags on an external scale and switches target recipe quickly. Calibration remains separate from target compensation. No PID or automatic AI target correction is required for v0.1.

## Quick start — Linux amd64

```bash
git switch main
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host

./firmware/host/serve-hmi.sh 8080
```

Open `http://<linux-node-ip>:8080/` to review the host HMI preview and parameter/recipe UX. The host preview is mock/read-only and has no GPIO, Modbus or actuator authority.

## Quick start — ESP32-S3

Install ESP-IDF v5.5.5, then:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

Keep calibration writes disabled for the first flash. The TLB and ESP serial settings must match.

Detailed instructions: [`firmware/README.md`](firmware/README.md).

## Operating modes

- **MANUAL** — machine stopped; `DI04 process.initiative` is the fill ON/OFF switch; fill to target and stop; bag push is hard-blocked.
- **AUTO** — machine rotating; continuous fill/discharge cycle. Current v0.1 uses discharge refs A/B; at fixed 14.4 s/rev the discharge point is fixed after tuning, while speed variation is compensated from measured rotor timing.

## Current branches

| Branch | Purpose |
|---|---|
| `main` | current integrated design/docs/firmware baseline |
| `fw-sp01-v0.1` | SP01 firmware development branch; synced when requested |
| `py-sim` | Python digital twin / replay / conformance reference |
| `debate` | historical design discussion and red-team material |

## Current documents

- [`progress.md`](progress.md) — current gates, risk focus and next actions
- [`spec/SP01.md`](spec/SP01.md) — canonical SP01 sequence and I/O
- [`docs/WEIGHING.md`](docs/WEIGHING.md) — load-cell/TLB485/RS485 boundary and target recipes
- [`docs/HW.md`](docs/HW.md) — prototype wiring and architecture
- [`docs/BOM.md`](docs/BOM.md) — one-node procurement list
- [`docs/CALIBRATION.md`](docs/CALIBRATION.md) — weighing calibration
- [`docs/FW.md`](docs/FW.md) — firmware architecture, gates and milestones
- [`firmware/README.md`](firmware/README.md) — build, simulation, HMI and flashing
- [`hardware/README.md`](hardware/README.md) — vendor links, manuals and photo index

## Hardware gate order

```text
Linux amd64 simulation + HMI review
-> ESP32 boot / ALL DO safe OFF
-> dummy 24 V DI/DO
-> TLB485 digital weight over isolated RS485
-> calibration 0 / 20 / 50 kg
-> MANUAL/AUTO dry cycle
-> Wi-Fi loss / reboot / fault injection
-> SP01 shadow
-> controlled machine connection
```
