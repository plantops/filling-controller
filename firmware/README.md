# SP01 Firmware v0.1

Target: Waveshare Industrial ESP32-S3 8DI/8DO controller, ESP-IDF v5.5.5, C++17, FreeRTOS.

Current maturity: **hardware-ready RC for bench testing**. Linux/shared-core CI and ESP32-S3 compilation pass; real 24 V I/O and TLB485 transport still require physical gates.

```text
HIGH    control    DI image -> latest WeightSnapshot -> shared C++ FSM -> interlocks -> DO image
MEDIUM  weighing   isolated RS485/TLB485 -> latest validated WeightSnapshot
LOW     web        small ESP-hosted HTML/JSON HMI
```

## Current implementation

- shared C++ controller core built/tested on Linux amd64 and ESP32-S3;
- MANUAL and AUTO operation modes;
- speed-adaptive discharge using reference sensors A/B;
- 8 DI on GPIO4..11;
- 8 DO through TCA9554 at I2C `0x20`, SDA GPIO42 / SCL GPIO41;
- safe DO latch written before enabling TCA9554 outputs;
- onboard isolated RS485, firmware mapping GPIO17 TX / GPIO18 RX / GPIO21 RTS;
- TLB485 Modbus polling with weight quality/staleness handling;
- guarded TLB zero/span service calls;
- bilingual ESP-hosted browser HMI;
- Linux host HMI preview for UI/parameter review;
- Wi-Fi supervisory only; control does not depend on Wi-Fi.

## Weighing design

Production boundary:

```text
load cell bridge
   -> TLB485
   -> board isolated RS485 terminal
   -> asynchronous TLB task
   -> latest validated WeightSnapshot
   -> 10 ms controller loop
```

Do **not** route continuous weight through a DI. A DI is binary only, all eight SP01 DIs are already assigned, and it cannot replace the digital kg stream. Do not connect the raw load-cell bridge directly to ESP32 for production.

The weighing task polls independently; the controller never blocks waiting for Modbus.

Bring-up profile:

```text
9600 bit/s
address 1
50 ms poll
```

Target high-rate profile after G4 transport measurements pass:

```text
115200 bit/s
20 ms poll   ~= 50 updates/s
```

A 10 ms poll is test-only after measured TLB response time, RS485 error rate and controller timing show adequate margin.

Canonical details: [`../docs/WEIGHING.md`](../docs/WEIGHING.md).

## Target recipe strategy

v0.1 keeps the proven operator compensation method simple. Recipes may differ only by target weight while the other tuned filling parameters remain unchanged:

```text
50.0 kg
50.1 kg
50.2 kg
50.3 kg
...
```

Finished bags are checked on an external scale and the operator can switch target recipe quickly. This is distinct from calibration. No PID or automatic AI target correction is required for v0.1.

## Linux amd64 — shared controller

Requirements: CMake, C++17 compiler and Python 3 for the HMI preview server.

```bash
cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host
```

The host binary runs the same shared C++ controller core used by ESP firmware.

## Linux amd64 — HMI preview

```bash
./firmware/host/serve-hmi.sh 8080
```

Open:

```text
http://<linux-node-ip>:8080/
```

The host HMI preview is deliberately **mock/read-only**:

- no GPIO;
- no Modbus;
- no actuator authority;
- animated representative cycle only;
- current parameter defaults and planned recipe UX shown for review.

The real ESP HMI currently provides live Status, I/O, Calibration and Diagnostics. Runtime Settings/recipe write support is not yet enabled; production parameters are currently configured through ESP-IDF `menuconfig`.

## Current parameters

Configured under `menuconfig -> SP01 Filling Controller`:

| Parameter | Current bring-up default | Direction after bench evidence |
|---|---:|---|
| Control period | 10 ms | keep |
| Target weight | 50.000 kg | recipe bank 50.0/50.1/50.2/... |
| Coarse -> Fine threshold | 40.000 kg | keep/tune from proven machine behavior |
| Cutoff margin | 0 g | tune only from evidence |
| Maximum weight age | 500 ms | review after measured RS485 profile |
| Bag acquire timeout | 2000 ms | measured |
| Coarse fill timeout | 12000 ms | measured |
| Fine fill timeout | 5000 ms | measured |
| Minimum settle time | 200 ms | measured |
| Wait discharge timeout | 6000 ms | measured |
| Push duration | 500 ms | measured |
| Discharge countdown | 1000 normalized counts | commission |
| Discharge lead trim | 0 counts | commission |
| DI invert mask | `0x00` | as-built |
| DO invert mask | `0x00` | as-built |
| TLB485 baud | 9600 | target 115200 after G4 |
| TLB485 Modbus address | 1 | keep unless as-built differs |
| TLB485 poll period | 50 ms | target 20 ms after G4 |

## Discharge timing

At fixed revolution time (nominal SP01 cycle 14.4 s), the discharge point is fixed after sensor geometry and actuator lead are tuned. Speed variation is a simple angular-time scaling problem.

Current v0.1 retains A/B references to measure speed inside the current revolution. This is measurement margin, not fundamental complexity; a future one-sensor version can derive revolution period from consecutive pulses if field evidence supports it.

## Operation modes

### MANUAL

Machine is stopped. `DI04 process.initiative` is the fill ON/OFF switch.

```text
OFF -> idle
ON  -> bag acquire -> tare -> coarse -> fine -> cutoff -> settle -> COMPLETE
```

Bag push is hard-blocked in MANUAL. Fill-position, conveyor-ready and discharge A/B are not used to complete the manual fill-only cycle.

### AUTO

Machine rotates and runs continuous cycles:

```text
permissive
-> fill position
-> bag acquire
-> tare / coarse / fine / cutoff / settle
-> discharge ref A
-> discharge ref B
-> speed-normalized countdown
-> bag push
-> next cycle
```

## ESP32-S3 build

Install ESP-IDF v5.5.5 and activate its environment, then:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

In `menuconfig -> SP01 Filling Controller` set at least Wi-Fi SSID/password and service token as needed. Keep calibration writes disabled for the first hardware flash.

The TLB and ESP serial settings must match. Start conservative, then move to the measured high-rate profile only after G4.

## Flash ESP32-S3

Connect the board with a USB data cable.

Linux port discovery:

```bash
ls /dev/ttyACM*
ls /dev/ttyUSB*
```

Typical ports are `/dev/ttyACM0` or `/dev/ttyUSB0`.

If serial permission is denied:

```bash
sudo usermod -aG dialout $USER
```

Log out/in, then:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

On Windows, for example:

```powershell
idf.py -p COM6 flash monitor
```

## HMI on real ESP32

After the ESP joins the configured Wi-Fi network, obtain its IP from the AP/router or serial log and open `http://<esp32-ip>/`.

Current live HMI sections:

```text
Status / Trạng thái
I/O
Calibration / Hiệu chuẩn
Diagnostics / Chẩn đoán
```

Planned Settings adds fast target-recipe selection plus validated parameter editing. Browser code never gets raw GPIO or raw Modbus write authority.

Calibration web writes require machine stopped, fill switch OFF, controller idle/safe, fresh stable weight, calibration writes enabled and a valid service token. DI7 and DI8 are discharge reference sensors A/B and are **not** calibration/service switches.

## First physical hardware gates

Keep machine actuators disconnected for the first run.

```text
G1  controller boots; no reset loop; ALL DO remain safe OFF
G2  exercise 8 dummy 24 V DI and 8 dummy DO loads
G3  verify MANUAL dry fill sequence with representative dummy loads
G4  TLB485 + load cell over board RS485; measure update rate/latency/jitter/errors/reconnect
G5  calibrate: ZERO -> CHECK 20 kg -> SPAN 50 kg -> VERIFY 0/20/50
G6  AUTO dry cycle + discharge timing
G7  Wi-Fi loss / reboot / stale weight / comm fault injection
G8  SP01 shadow with physical machine outputs isolated
G9  controlled live SP01 pilot after review
```

Safety/E-stop remains outside this firmware. The filling motor DO is a command to an external contactor/VFD input, never motor power.
