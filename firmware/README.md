# SP01 Firmware v0.1

Target: Waveshare Industrial ESP32-S3 8DI/8DO controller, ESP-IDF v5.5.5, C++17, FreeRTOS.

Current maturity: **hardware-ready RC for bench testing**. Linux/shared-core CI and ESP32-S3 compilation pass; real 24 V I/O, TLB485 and machine behavior still require physical gates.

```text
HIGH    control    DI image -> shared C++ FSM -> interlocks -> DO image
MEDIUM  weighing   RS485/TLB485 -> latest validated WeightSnapshot
LOW     web        small ESP-hosted HTML/JSON HMI
```

## Current implementation

- shared C++ controller core built/tested on Linux amd64 and ESP32-S3;
- MANUAL and AUTO operation modes;
- speed-adaptive discharge using reference sensors A/B;
- 8 DI on GPIO4..11;
- 8 DO through TCA9554 at I2C `0x20`, SDA GPIO42 / SCL GPIO41;
- safe DO latch written before enabling TCA9554 outputs;
- RS485 on GPIO17 TX / GPIO18 RX / GPIO21 RTS;
- TLB485 Modbus polling with weight quality/staleness handling;
- guarded TLB zero/span service calls;
- bilingual ESP-hosted browser HMI;
- Linux host HMI preview for UI/parameter review;
- Wi-Fi supervisory only; control does not depend on Wi-Fi.

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
- current parameter defaults shown for UI review.

The real ESP HMI currently provides live Status, I/O, Calibration and Diagnostics. Runtime Settings write support is not yet enabled; production parameters are currently configured through ESP-IDF `menuconfig`.

## Current parameters

Configured under `menuconfig -> SP01 Filling Controller`:

| Parameter | Default |
|---|---:|
| Control period | 10 ms |
| Target weight | 50.000 kg |
| Coarse -> Fine threshold | 40.000 kg |
| Cutoff margin | 0 g |
| Maximum weight age | 500 ms |
| Bag acquire timeout | 2000 ms |
| Coarse fill timeout | 12000 ms |
| Fine fill timeout | 5000 ms |
| Minimum settle time | 200 ms |
| Wait discharge timeout | 6000 ms |
| Push duration | 500 ms |
| Discharge countdown | 1000 normalized counts |
| Discharge lead trim | 0 counts |
| DI invert mask | `0x00` |
| DO invert mask | `0x00` |
| TLB485 baud | 9600 |
| TLB485 Modbus address | 1 |
| TLB485 poll period | 50 ms |

`discharge_countdown` and `discharge_lead` must be commissioned from measured sensor geometry and actuator response. Do not field-tune them from guessed machine speed.

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

In `menuconfig -> SP01 Filling Controller` set at least:

```text
Dedicated Wi-Fi SSID
Dedicated Wi-Fi password
Service token
```

Initial TLB settings:

```text
TLB485 polling         enabled
Baud                   9600
Modbus address         1
Poll period            50 ms
Calibration writes     disabled
```

Keep calibration writes disabled for the first hardware flash. Enable them only after the exact installed TLB485 protocol/manual revision is verified.

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

Log out/in, then flash:

```bash
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

or in one command:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

On Windows use the detected COM port, for example:

```powershell
idf.py -p COM6 flash monitor
```

If automatic download mode is not entered, use the board BOOT/download procedure from the Waveshare hardware documentation, then retry the flash.

## HMI on real ESP32

After the ESP joins the configured Wi-Fi network, obtain its IP from the AP/router or serial log and open:

```text
http://<esp32-ip>/
```

Current live HMI sections:

```text
Status / Trạng thái
I/O
Calibration / Hiệu chuẩn
Diagnostics / Chẩn đoán
```

Calibration web writes require all runtime service interlocks to be satisfied, including machine stopped, fill switch OFF, controller idle/safe, fresh stable weight, calibration writes enabled and a valid service token. DI7 and DI8 are discharge reference sensors A/B and are **not** calibration/service switches.

## First physical hardware gates

Keep machine actuators disconnected for the first run.

```text
G1  controller boots; no reset loop; ALL DO remain safe OFF
G2  exercise 8 dummy 24 V DI and 8 dummy DO loads
G3  verify MANUAL dry fill sequence with representative dummy loads
G4  connect TLB485 + load cell; verify weight/stability/communication
G5  calibrate: ZERO -> CHECK 20 kg -> SPAN 50 kg -> VERIFY 0/20/50
G6  AUTO dry cycle + discharge A/B at multiple simulated rotor speeds
G7  Wi-Fi loss / reboot / stale weight / comm fault injection
G8  SP01 shadow with physical machine outputs isolated
G9  controlled live SP01 pilot after review
```

Safety/E-stop remains outside this firmware. The filling motor DO is a command to an external contactor/VFD input, never motor power.
