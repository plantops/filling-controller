# SP01 Firmware v0.1

Target: Waveshare industrial ESP32-S3 8DI/8DO board, ESP-IDF v5.5.5, C++17, FreeRTOS.

```text
HIGH    control    DI image -> shared C++ FSM -> interlocks -> DO image
MEDIUM  weighing   RS485/TLB485 -> latest validated WeightSnapshot
LOW     web        small ESP-hosted HTML/JSON HMI
```

Current code includes:

- shared C++ controller core, also built/tested on Linux amd64;
- 8 DI on GPIO4..11;
- 8 DO through TCA9554 at I2C `0x20`, SDA GPIO42 / SCL GPIO41;
- safe DO latch written before enabling TCA9554 outputs;
- RS485 on GPIO17 TX / GPIO18 RX / GPIO21 RTS;
- TLB485 Modbus read path: status + gross/net weight + stability;
- bounded stale-weight and communication fault handling;
- calibration service for command-100 zero and command-101 sample span;
- small bilingual browser HMI;
- physical service interlock on DI8 for calibration writes;
- Wi-Fi supervisory only.

## Linux amd64

```bash
cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host
```

## ESP32-S3 build / flash

Install ESP-IDF v5.5.5, then:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

In `menuconfig -> SP01 Filling Controller` set the dedicated Wi-Fi SSID/password and a non-empty service token if using calibration HMI writes.

TLB defaults are `9600 8N1`, Modbus address `1`. Calibration writes are disabled by default; enable them in menuconfig after checking the exact installed TLB485 manual/revision.

## First physical run

```text
1. Controller board only, machine outputs disconnected.
2. Connect 24 V dummy switches to DI1..DI8.
3. Connect lamps/dummy loads to DO1..DO8.
4. Power board: all DO must remain OFF through boot/reset.
5. Exercise DI/DO sequence with TLB absent; filling must stop at weighing readiness.
6. Connect TLB485 + load cell.
7. Verify live weight/stability in HMI.
8. Enable DI8 service mode with machine permissive OFF for calibration.
9. SET ZERO -> CHECK 20 kg -> SET SPAN 50 kg -> verify 0/20/50.
```

No raw GPIO or raw Modbus web write endpoint exists. Safety/E-stop remains outside this firmware.
