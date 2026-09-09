# SP01 Progress

Updated: 2026-09-09

Current version marker: **`0.1.0-rc1`**. The `v0.1.0-rc1` tag remains the frozen RC baseline; `main` now includes post-RC design/document updates.

Current maturity: **READY FOR BENCH**.

## Project mission — now explicit

The original commercial controller is discontinued / difficult to source while the mechanical rotary packer remains a high-value usable asset. The project is therefore an **obsolescence-rescue / asset-life-extension** controller, not an exercise in matching the purchase price or lifetime claims of a proprietary controller.

Locked lifecycle principle:

> **DESIGN FOR REPLACEMENT, NOT IMMORTALITY.**

A low-cost ESP controller may be a consumable service module if it fails safe, is cheap to replace, can be swapped quickly, does not disturb the calibrated weighing chain, and all important configuration/firmware knowledge exists outside the failed board.

Current economic assumption: replacing a controller costing roughly **VND 1.5 million** even on the order of **six months** is acceptable relative to packer downtime or stranding the mechanical asset. This is not a mandatory preventive interval; actual field history will determine replacement policy.

Canonical lifecycle document: [`docs/SERVICEABILITY.md`](docs/SERVICEABILITY.md).

## Summary

| Area | Status | Notes |
|---|---|---|
| Shared C++17 controller core | PASS | Same controller logic used by Linux host and ESP32 firmware |
| Linux amd64 build/tests | PASS baseline | Build, conformance tests and host smoke passed before latest docs-only lifecycle updates |
| Linux HMI preview | READY | Mock/read-only; parameter + recipe UX review |
| MANUAL mode | PASS host | Fill-only; machine stopped; bag push hard-blocked |
| AUTO mode | PASS host | Continuous fill/discharge sequence |
| Discharge timing concept | VERY LOW RISK | Fixed point at fixed 14.4 s/rev; A/B refs add current-revolution speed measurement |
| Dual discharge refs A/B | PASS host | Speed-adaptive timing already host-tested |
| ESP32-S3 build | PASS baseline | ESP-IDF v5.5.5; no firmware source changed in the latest lifecycle-doc update |
| Waveshare 8DI/8DO adapter | READY FOR BENCH | Physical electrical behavior pending |
| Safe DO initialization | READY FOR BENCH | ALL-OFF boot/reset still requires physical proof |
| 70 °C environment | **G2T REQUIRED** | Treat as field condition; characterize safe behavior and practical lifetime, not assumed rating |
| Replaceable controller strategy | DESIGN LOCKED | One spout/node, known-good spare, connectorized recovery direction |
| Config export/import | PENDING | Critical for fast spare provisioning; current v0.1 still relies heavily on `menuconfig`/as-built records |
| TLB485 Modbus layer | **TOP BENCH PRIORITY** | Code compiles; real digital weight transport not yet measured |
| Weight control strategy | LOW RISK | Proven operator recipe approach; no automatic learning required |
| Target recipe bank | DESIGN LOCKED | 50.0 / 50.1 / 50.2 / ... with fast operator selection |
| Calibration service | READY FOR BENCH | ZERO/span workflow implemented; physical 0/20/50 kg pending |
| ESP replacement vs calibration | DESIGN LOCKED | Replacing ESP must not auto-write TLB zero/span |
| ESP-hosted HMI | READY FOR BENCH | Live Status, I/O, Calibration, Diagnostics |
| Runtime Settings/recipe write | PENDING | Preview UX only; current config via `menuconfig` |
| SP01 shadow/live authority | NOT STARTED | No real machine actuation approved yet |

## Risk map after latest design decisions

| Risk | Current view |
|---|---|
| FSM / sequence logic | Low |
| Filling target compensation | Very low |
| Discharge mechanics/timing | Very low |
| TLB485 -> ESP digital weight transport | Medium / top measurement priority |
| 24 V field electrical integration | Medium |
| 70 °C controller environment | Medium-high hardware/lifetime risk, mitigated by replaceability |
| Controller obsolescence / vendor dependence | Primary business risk being removed by project |
| Long-term ESP board life | No longer a primary success criterion if failure is safe and replacement is easy |

## Locked serviceability decisions

```text
1 spout = 1 independent controller
no SP01 master dependency
known-good pre-flashed spare for field use
controller failure contained to one spout
critical config/version stored outside the board
controller swap must not alter TLB calibration
field wiring should converge on plug/label/swap rather than rewiring
```

A hard MTTR target is not yet frozen. A real replacement drill will measure it. The design objective is a **minutes-scale controller exchange**, not an engineering/recommissioning event.

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
- 10 ms polling is test-only after measured margin;
- ESP replacement must start read-only and must not issue zero/span automatically.

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

## 70 °C environmental strategy

Possible ambient near the rotating controller may reach approximately **70 °C**.

The project does **not** require the low-cost ESP node to prove a 20-year lifetime at 70 °C. It requires:

```text
safe failure / reboot behavior
known output state
measured field temperature
measured practical lifetime
fast spare replacement
recoverable configuration
no hidden vendor dependency
```

The TLB485 is a separate service module. If its verified environment or measured field behavior is unsuitable, relocate it or use an appropriate transmitter rather than automatically accepting the same consumable policy as the ESP board.

## Current gate status

```text
G0   Linux + ESP32 build/conformance             PASS baseline
G1   ESP boot; ALL DO safe OFF                   PENDING
G2   8 dummy 24 V DI / 8 dummy DO loads          PENDING
G2T  thermal + serviceability characterization    PENDING / HIGH PRIORITY
G3   MANUAL dry fill cycle                        PENDING
G4   TLB485 digital weight over isolated RS485    PENDING / TOP PRIORITY
G5   ZERO / 20 kg / 50 kg calibration             PENDING
G6   AUTO dry cycle + discharge timing             PENDING
G7   Wi-Fi loss / reboot / stale / comm faults    PENDING
G8   SP01 shadow                                  PENDING
G9   controlled live SP01 pilot                   PENDING
```

## G2T measurement plan

```text
measure controller ambient/location temperature
measure TLB ambient/location temperature
run representative DI/DO load
run RS485 polling and Wi-Fi/HMI
exercise reboot / brownout / fault
verify outputs remain safe
record reset/error evidence
perform one spare-controller swap
restore approved configuration
verify TLB calibration unchanged
```

A controlled elevated-temperature soak up to the expected 70 °C ambient is desirable where suitable equipment is available.

The result of G2T should answer:

```text
Does it fail safely?
How hot does it really run?
What practical lifetime is observed?
Can it be replaced quickly enough that lifetime is economically irrelevant?
```

## G4 measurement plan

Do not spend G4 proving a new weighing algorithm. Prove transport from the known weighing chain into the controller.

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
behavior at representative elevated temperature
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

### Spare-controller drill — after G1/G2

```text
prepare second known-good board
record approved firmware/config
make machine/test harness safe
swap controller
verify safe boot
verify DI/DO identity
verify TLB read-only communication
verify no calibration change
record actual replacement time and pain points
```

## Release rule

`v0.1.0-rc1` remains a **bench release candidate**, not a production release. Do not move/rewrite that tag for post-RC documentation changes.

Promotion to production `v0.1.0` requires at minimum:

```text
G1..G7 PASS with evidence, including G2T
stable digital TLB/load-cell transport
verified calibration
verified MANUAL behavior
verified AUTO discharge position across operating speed range
safe restart/fault behavior
known-good spare + documented recovery path
critical config recoverable outside the controller
SP01 shadow review before actuator authority
```

Production success is defined primarily as **removing the discontinued controller as the reason the mechanical packer can no longer operate**.
