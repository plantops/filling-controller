# SP01 Progress

Updated: 2026-09-09

Current version marker: **`0.1.0-rc1`**. The `v0.1.0-rc1` tag remains the frozen RC baseline; `main` now includes post-RC design/document updates.

Current maturity: **READY FOR BENCH**. Software gates are green; the main remaining technical measurement is the real TLB485 -> isolated RS485 -> ESP weight path.

## Summary

| Area | Status | Notes |
|---|---|---|
| Shared C++17 controller core | PASS | Same controller logic used by Linux host and ESP32 firmware |
| Linux amd64 build/tests | PASS | Build, conformance tests and host smoke pass in CI before current docs/config update |
| Linux HMI preview | READY | Mock/read-only; parameter + recipe UX review |
| MANUAL mode | PASS host | Fill-only; machine stopped; bag push hard-blocked |
| AUTO mode | PASS host | Continuous fill/discharge sequence |
| Discharge timing concept | LOW RISK | Fixed point at fixed 14.4 s/rev; current A/B refs add current-revolution speed measurement |
| Dual discharge refs A/B | PASS host | Speed-adaptive timing already host-tested |
| ESP32-S3 build | PASS baseline | ESP-IDF v5.5.5; rerun CI after config/docs sync |
| Waveshare 8DI/8DO adapter | READY FOR BENCH | Physical electrical behavior pending |
| Safe DO initialization | READY FOR BENCH | ALL-OFF boot/reset still requires physical proof |
| TLB485 Modbus layer | **TOP BENCH PRIORITY** | Code compiles; real digital weight transport not yet measured |
| Weight control strategy | LOW RISK | Proven operator recipe approach; no automatic learning required |
| Target recipe bank | DESIGN LOCKED | 50.0 / 50.1 / 50.2 / ... with fast operator selection |
| Calibration service | READY FOR BENCH | ZERO/span workflow implemented; physical 0/20/50 kg pending |
| ESP-hosted HMI | READY FOR BENCH | Live Status, I/O, Calibration, Diagnostics |
| Runtime Settings/recipe write | PENDING | Preview UX only; current config via `menuconfig` |
| SP01 shadow/live authority | NOT STARTED | No real machine actuation approved yet |

## Locked weighing decision

```text
load cell bridge
    -> LAUMAS TLB485
    -> isolated RS485 terminal on ESP board
    -> asynchronous digital WeightSnapshot
    -> controller
```

- continuous weight does **not** go through DI;
- ESP32 production does not read raw mV/V load-cell signal;
- all eight DIs remain machine signals;
- control task never blocks waiting for Modbus;
- bring-up profile remains 9600 bit/s / 50 ms poll;
- target after clean G4 evidence is 115200 bit/s / 20 ms poll (~50 Hz);
- 10 ms polling is test-only after measured margin.

Canonical design: [`docs/WEIGHING.md`](docs/WEIGHING.md).

## Target adaptation

Production compensation follows the proven operator method:

```text
Recipe 50.0 kg
Recipe 50.1 kg
Recipe 50.2 kg
Recipe 50.3 kg
...
```

Other tuned filling parameters remain unchanged unless evidence requires a change. External check-scale results guide fast recipe switching. Calibration remains unchanged. No PID/AI auto-target adaptation is required in v0.1.

## Discharge

At nominal 14.4 s/rev, discharge geometry is fixed after a few tuning runs. If rotor speed changes, command delay scales with revolution timing.

Current v0.1 keeps A/B refs because they provide local speed measurement in the same revolution. One sensor plus measured revolution period remains a valid future simplification if field evidence shows it is sufficient.

## Current gate status

```text
G0  Linux + ESP32 build/conformance             PASS baseline
G1  ESP boot; ALL DO safe OFF                   PENDING
G2  8 dummy 24 V DI / 8 dummy DO loads          PENDING
G3  MANUAL dry fill cycle                        PENDING
G4  TLB485 digital weight over isolated RS485    PENDING / TOP PRIORITY
G5  ZERO / 20 kg / 50 kg calibration             PENDING
G6  AUTO dry cycle + discharge timing             PENDING
G7  Wi-Fi loss / reboot / stale / comm faults    PENDING
G8  SP01 shadow                                  PENDING
G9  controlled live SP01 pilot                   PENDING
```

## G4 measurement plan

Do not spend G4 proving a new weighing algorithm. Prove the transport from the known weighing chain into the controller.

Record:

```text
0 / 20 / 40 / 49 / 50 kg digital values
update rate
sample age / end-to-end latency
jitter
Modbus error count
stale detection
cable disconnect response
reconnect response
behavior while representative 24 V inductive loads switch
```

Start at 9600/50 ms. If clean, test 115200/20 ms. Test 10 ms only if useful and supported by measured response/error margins.

## Immediate next run

### Linux amd64

```bash
git switch main
git pull --ff-only

cmake -S firmware/host -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
./build/host/sp01_host

./firmware/host/serve-hmi.sh 8080
```

Review target-recipe/parameter UX at `http://<linux-node-ip>:8080/`.

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

## Release rule

`v0.1.0-rc1` remains a **bench release candidate**, not a production release. Do not move/rewrite that tag for post-RC documentation changes.

Promotion to a production `v0.1.0` requires at minimum:

```text
G1..G7 PASS with evidence
stable digital TLB/load-cell transport
verified calibration
verified MANUAL behavior
verified AUTO discharge position across operating speed range
safe restart/fault behavior
SP01 shadow review before actuator authority
```
