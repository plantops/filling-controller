# Filling Controller

Open controller for an 8-spout rotary cement bag packer.

**Current target: SP01 hardware-ready bench prototype for obsolescence rescue and asset-life extension.**

[Tiếng Việt](README_VI.md)

> **Have the physical ESP board in hand? Start here:** [`docs/ONBOARDING.md`](docs/ONBOARDING.md). Print [`docs/FIRST_BOARD_CHECKLIST.md`](docs/FIRST_BOARD_CHECKLIST.md) and keep [`docs/BOARD_TERMINALS.md`](docs/BOARD_TERMINALS.md) beside the board.

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

## First board — colleague onboarding

The beginner path is deliberately different from machine commissioning:

```text
board + antenna
-> USB-only power
-> GitHub Actions firmware artifact
-> first flash
-> verify safe boot / missing-TLB behavior
-> 24 V board-power test
-> DI dry-contact test
-> DO dummy-load test
-> only then continue to thermal/TLB/machine gates
```

The actual physical board has now been photographed with the cover on and open. The terminal map is therefore frozen for this board revision:

```text
DIGITAL OUTPUTS                 DIGITAL INPUTS                 POWER
COM GND 8 7 6 5 4 3 2 1        COM GND 8 7 6 5 4 3 2 1       7~36 V  +  -

RS485: A+ B- PE
CAN:   H L PE
```

The detailed guide now gives literal first-bench wiring:

```text
DI dry contact: INPUT COM <-> switch <-> DIx
DO dummy lamp:  OUTPUT COM=+24 V, OUTPUT GND=0 V, +24 V -> lamp -> DOx
```

Always identify a screw by its printed terminal name, not by remembered left/right orientation. See [`docs/BOARD_TERMINALS.md`](docs/BOARD_TERMINALS.md) and the visual [`docs/assets/BOARD_TERMINALS.svg`](docs/assets/BOARD_TERMINALS.svg).

## Why this project exists

The original commercial controller is discontinued / difficult to source while the mechanical packer remains a high-value usable asset. The project goal is therefore not to clone a premium controller's purchase price or promise a 20-year electronics lifetime. It is to remove controller obsolescence as the reason a mechanically valuable machine becomes stranded.

The lifecycle rule is:

> **DESIGN FOR REPLACEMENT, NOT IMMORTALITY.**

A low-cost ESP controller may be treated as a replaceable service module if it fails safe, can be swapped quickly, does not disturb weighing calibration, and all critical firmware/configuration knowledge is recoverable outside that one board.

Current economic assumption: replacing a controller costing roughly VND 1.5 million even on the order of six months is acceptable. This is an economic tolerance, not a fixed preventive-replacement interval.

Canonical lifecycle/service strategy: [`docs/SERVICEABILITY.md`](docs/SERVICEABILITY.md).

## Current status

Version marker: [`VERSION`](VERSION) = **`0.1.0-rc1`**. The existing `v0.1.0-rc1` tag remains the frozen RC baseline; `main` contains later design/document updates.

Software baseline remains **READY FOR BENCH**:

- shared C++17 controller core builds/tests on Linux amd64;
- ESP32-S3 firmware builds with ESP-IDF v5.5.5;
- MANUAL fill-only and AUTO continuous modes implemented;
- discharge timing is low-complexity at fixed revolution time; current A/B references provide current-revolution speed measurement;
- Waveshare 8DI/8DO adapter, TLB485 layer, ESP web HMI and calibration service compile successfully;
- physical board terminal identity is now frozen from actual hardware photos;
- the main technical measurements now are real 8DI/8DO electrical behavior, reliable TLB485 -> ESP digital weight transport, and environmental/serviceability behavior around a possible 70 °C machine ambient.

Physical gates are still pending. No real machine authority is implied by the RC.

See [`progress.md`](progress.md).

## Serviceability design — locked direction

```text
one spout = one independent controller
                 |
                 +-- same open firmware
                 +-- explicit per-spout config
                 +-- pre-flashed known-good spare
                 +-- connectorized / labelled harness
                 `-- no critical knowledge only in board flash
```

Rules:

- SP01 is not master for the other seven spouts;
- controller failure should be contained to one spout;
- replacement of the ESP controller should not require load-cell/TLB recalibration if the TLB remains unchanged;
- critical machine settings and version information must be recoverable outside the disposable node;
- board-swap recovery should become a minutes-scale service action, not a rewiring project; the actual MTTR will be measured rather than guessed.

## Environmental condition — up to 70 °C ambient

Possible machine ambient may reach approximately **70 °C**. The current whole-board prototype is not assumed qualified simply because individual parts may have high temperature ratings.

v0.1 therefore adds a dedicated **G2T thermal/serviceability characterization** after dummy I/O:

```text
measure real installation temperature
+ elevated-temperature controller operation
+ representative DO / RS485 / Wi-Fi load
+ reboot / brownout / fault tests
+ safe-output verification
+ spare-controller replacement drill
```

The design is allowed to accept economically reasonable ESP board lifetime and replace the controller as a consumable. The weighing transmitter is treated separately: if its verified environment is inadequate, relocate or replace that module rather than assuming the same disposable lifetime.

## Weighing design — locked boundary

```text
load cell bridge
   -> LAUMAS TLB485
   -> isolated RS485 terminal A+ / B- on ESP32 board
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

For a first physical board, use [`docs/ONBOARDING.md`](docs/ONBOARDING.md) rather than jumping directly to machine wiring.

Developer build path:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

Non-developer colleagues can download the GitHub Actions ESP artifact and flash it with `esptool`; the exact sequence is in the onboarding guide.

Keep calibration writes disabled for the first flash. The TLB and ESP serial settings must match when the TLB is later installed.

Detailed firmware instructions: [`firmware/README.md`](firmware/README.md).

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

- [`docs/ONBOARDING.md`](docs/ONBOARDING.md) — **start here with a physical board; detailed beginner wiring/power/flash/boot guide**
- [`docs/BOARD_TERMINALS.md`](docs/BOARD_TERMINALS.md) — literal actual-board terminal map and first DI/DO wiring
- [`docs/assets/BOARD_TERMINALS.svg`](docs/assets/BOARD_TERMINALS.svg) — one-page terminal/wiring visual
- [`docs/FIRST_BOARD_CHECKLIST.md`](docs/FIRST_BOARD_CHECKLIST.md) — printable first-board checklist
- [`progress.md`](progress.md) — current gates, risk focus and next actions
- [`spec/SP01.md`](spec/SP01.md) — canonical SP01 sequence and I/O
- [`docs/SERVICEABILITY.md`](docs/SERVICEABILITY.md) — replacement, spare, thermal and asset-life-extension strategy
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
-> beginner first-board onboarding
-> ESP32 boot / ALL DO safe OFF
-> dummy 24 V DI/DO
-> G2T thermal + serviceability characterization
-> TLB485 digital weight over isolated RS485
-> calibration 0 / 20 / 50 kg
-> MANUAL/AUTO dry cycle
-> Wi-Fi loss / reboot / fault injection
-> SP01 shadow
-> controlled machine connection
```
