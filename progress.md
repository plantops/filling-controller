# SP01 Progress

Updated: 2026-09-09

Current version: **`0.1.0-rc1`**

Current maturity: **READY FOR BENCH**. Software gates are green; real 24 V I/O, weighing hardware and machine behavior still require physical verification.

## Summary

| Area | Status | Notes |
|---|---|---|
| Shared C++17 controller core | PASS | Same controller logic used by Linux host and ESP32 firmware |
| Linux amd64 build/tests | PASS | Build, conformance tests and host smoke pass in CI |
| Linux HMI preview | READY | Mock/read-only; no GPIO, Modbus or actuator authority |
| MANUAL mode | PASS host | Fill-only; machine stopped; bag push hard-blocked |
| AUTO mode | PASS host | Continuous fill/discharge sequence |
| Dual discharge refs A/B | PASS host | Speed-adaptive timing tested at different simulated rotor speeds |
| ESP32-S3 build | PASS | ESP-IDF v5.5.5 |
| Waveshare 8DI/8DO adapter | READY FOR BENCH | Compiles; physical channel/electrical behavior not yet verified |
| Safe DO initialization | READY FOR BENCH | Implemented; ALL-OFF boot/reset must be verified physically |
| TLB485 Modbus layer | READY FOR BENCH | Compiles; real TLB485 link not yet verified |
| Calibration service | READY FOR BENCH | ZERO/span workflow implemented; physical 0/20/50 kg verification pending |
| ESP-hosted HMI | READY FOR BENCH | Live Status, I/O, Calibration and Diagnostics |
| Runtime Settings edit/save | PENDING | Current production parameters are configured through `menuconfig` |
| SP01 shadow/live authority | NOT STARTED | No real machine actuation approved yet |

## Operation modes

### MANUAL

Machine stopped. `DI04 process.initiative` is the fill ON/OFF switch.

```text
OFF -> idle
ON  -> bag acquire -> tare -> coarse -> fine -> cutoff -> settle -> COMPLETE
```

MANUAL does not use the discharge sequence and `bag.push` is hard-blocked.

### AUTO

Machine rotating. Continuous cycle:

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

The A/B references are used to normalize discharge timing against actual rotor speed. `discharge_lead` is a commissioning trim, not a fixed delay for one nominal machine speed.

## Current gate status

```text
G0  Linux + ESP32 build/conformance          PASS
G1  ESP boot; ALL DO safe OFF                PENDING
G2  8 dummy 24 V DI / 8 dummy DO loads       PENDING
G3  MANUAL dry fill cycle                     PENDING
G4  TLB485 + load cell communication          PENDING
G5  ZERO / 20 kg / 50 kg calibration          PENDING
G6  AUTO dry cycle + discharge A/B            PENDING
G7  Wi-Fi loss / reboot / stale / comm faults PENDING
G8  SP01 shadow                               PENDING
G9  controlled live SP01 pilot                PENDING
```

## Immediate next run

### Linux amd64 node

```bash
git switch main
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host

./firmware/host/serve-hmi.sh 8080
```

Review the HMI at:

```text
http://<linux-node-ip>:8080/
```

### ESP32-S3 first flash

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash monitor
```

First flash rules:

```text
machine actuators disconnected
TLB calibration writes disabled
verify no reset loop
verify ALL DO stay OFF through boot/reset
verify control task remains alive
verify missing TLB fails safely
verify Wi-Fi/HMI has no effect on control behavior
```

## Parameter review

Current defaults are documented in [`firmware/README.md`](firmware/README.md) and shown in the Linux HMI preview. Key values include target 50 kg, coarse-to-fine 40 kg, controller period 10 ms, TLB polling 50 ms, and normalized discharge countdown/lead parameters.

Do not treat the current discharge geometry/timing defaults as commissioned machine values. Sensor A angle, sensor B angle, optimum discharge angle and command-to-release actuator delay must be measured on the real machine.

## Release rule

`0.1.0-rc1` is a **bench release candidate**, not a production release.

Promotion to `v0.1.0` requires, at minimum:

```text
G1..G7 PASS with evidence
stable TLB/load-cell behavior
verified calibration
verified MANUAL behavior
verified AUTO discharge position across machine speed range
safe restart/fault behavior
SP01 shadow review before actuator authority
```
