# Hardware Prototype v0.1 — SP01 Wireless Node

## Status

**Frozen bench/shadow prototype architecture. Not approved for live packer actuation.**

v0.1 intentionally builds **one spout only: SP01**. Do not purchase or build eight nodes yet.

The first physical connection is to a **dummy 24 V DI/DO bench harness**, not to the real packer. Real SP01 wiring is connected later through explicit shadow/live gates.

Current planning assumption: **24 VDC is available on the rotating packer**. Its quality, grounding, available current and transient behavior still require measurement before real-machine connection.

The rotary packer has no practical stationary Ethernet path at the spout. Therefore the prototype uses:

```text
stationary side: 1 dedicated Wi-Fi AP/router + engineering laptop
rotating side:   1 autonomous ESP32-S3 SP01 node + LAUMAS TLB + local I/O
```

Wi-Fi is supervisory only. It is never required to finish, abort, or safely stop the current filling cycle.

At 2,000 bags/h with 8 spouts:

```text
3600 s/h × 8 / 2000 = 14.4 s per spout revolution/cycle
```

## 1. v0.1 topology

```text
                         STATIONARY SIDE

                 Laptop / engineering browser
                           │
                         LAN preferred
                           │
                           ▼
                 ┌───────────────────┐
                 │ DEDICATED AP/     │
                 │ ROUTER            │
                 │ PACKER01 network  │
                 └─────────┬─────────┘
                           │ 2.4 GHz Wi-Fi
                           │ supervisory only
                           )))

                         ROTATING SIDE
                           (((
                           │
                 ┌─────────▼─────────┐
                 │ ESP32-S3 SP01     │
                 │ ESP-IDF/FreeRTOS  │
                 │ 8 isolated DI     │
                 │ 8 protected DO    │
                 │ isolated RS485    │
                 │ Wi-Fi STA         │
                 │ embedded web HMI  │
                 └──────┬───────┬────┘
                        │       │
                   RS485│       │local field I/O
                        │       │
                        ▼       ▼
                  ┌──────────┐  dummy harness first
                  │ LAUMAS   │  real machine later
                  │ TLB485   │
                  └────┬─────┘
                       │
                    LOAD CELL
```

There is **no Ethernet connection across the rotating/stationary boundary** in v0.1.

Do not route Ethernet through ordinary carbon brushes or spare slip-ring contacts. If a wired rotating uplink is ever required, use an Ethernet-rated rotary joint/slip ring as a separate future design decision.

The ESP board Ethernet port, if present, is bench/service-only in v0.1.

## 2. Control boundary

SP01 must remain autonomous when the AP/router, laptop, browser, or Wi-Fi link disappears.

Local deterministic path:

```text
machine/dummy DI
   ↓
ESP32 process image / FSM
   ↓
local DO

load cell
   ↓
TLB485
   ↓ RS485
ESP32
```

Supervisory path:

```text
ESP32
   ↓ Wi-Fi
AP/router
   ↓
laptop/HMI/historian
```

The supervisory path may observe and issue only validated non-time-critical service/configuration commands. It is not part of filling cutoff, interlocks, actuator timing, or safety.

## 3. Physical node ownership

One controller owns one spout only.

For v0.1:

```text
SP01 = one ESP32-S3 + one TLB + one local I/O set
```

Future replication, only after SP01 acceptance:

```text
SP01  ESP + TLB
SP02  ESP + TLB
...
SP08  ESP + TLB
```

No spout is a master for another spout. SP01 must never become a machine-wide dependency merely because it is the first prototype.

## 4. SP01 control sequence

Preserve the proven legacy mechanical sequence:

```text
WAIT PERMISSIVE
→ WAIT FILL POSITION
→ BAG ACQUIRE
→ BAG VERIFY
→ TARE / READY
→ COARSE FILL
→ FINE FILL
→ CUTOFF
→ SETTLE / CHECK
→ WAIT PUSH POSITION
→ PUSH
→ COMPLETE
```

No blocking `delay()` calls are allowed in the control FSM. Every state has entry action, tick logic, exit condition, timeout, and fault reaction.

## 5. Digital input map

| New channel | Semantic signal | Legacy |
|---|---|---|
| DI01 | `hopper.feeder_running` | INPUT_0 |
| DI02 | `downstream.conveyor_ready` | INPUT_1 |
| DI03 | `machine.motor_running` | INPUT_2 |
| DI04 | `process.initiative` | INPUT_3 |
| DI05 | `cycle.fill_position` | INPUT_4 — field device still to be physically confirmed |
| DI06 | `bag.present` | INPUT_5 |
| DI07 | `position.push` | INPUT_6 |
| DI08 | `spare` | reserved |

Bench stage uses eight labelled 24 V test switches/selectors. Real field DI are connected only after physical tracing and measurement. No legacy 24 V signal may connect directly to MCU GPIO.

## 6. Weighing channel and calibration

```text
load cell → LAUMAS TLB485 → isolated RS485 → ESP32-S3
```

Semantic input:

```text
W01 = weight.net
```

The TLB owns load-cell excitation, A/D conversion, calibration/filtering, and weight/status transport.

TLB-specific Modbus addresses belong only in the TLB adapter.

Required semantic interface:

```text
WeighingUnit
  read_weight()
  read_status()
  read_raw()            optional
  is_stable()
  tare()
  zero()
  begin_calibration()
  set_span(reference_kg)
  verify(reference_kg)
  save_calibration()
  cancel_calibration()
  health()
  diagnostics()
```

Unsupported operations must not be invented if the selected TLB model does not expose them.

**Calibration is a required firmware/HMI v0.1 function.** See `docs/WEIGHING_CALIBRATION.md`.

Current commissioning procedure:

```text
empty saddle → SET ZERO
20 kg known reference → CHECK
50 kg standard → SET SPAN
remove weight → VERIFY ZERO
20 kg → VERIFY
50 kg → VERIFY
SAVE + LOCK
```

If 20 kg is not itself a trusted calibration standard, use it as a linearity/mechanical verification point rather than forcing a multi-point curve.

The SP01 FSM must survive replacement of TLB by another weighing transmitter without changing state semantics.

## 7. Digital output map — corrected legacy mapping

The legacy addresses reach OUTPUT_9, but OUTPUT_4 is unused. There are exactly **8 active outputs**.

| New DO | Semantic actuator | Legacy address | Field device |
|---|---|---:|---|
| DO01 | `scanner.down` | OUTPUT_1 | scanner cylinder solenoid |
| DO02 | `bag_detect_air` | OUTPUT_2 | bag-detect air solenoid |
| DO03 | `bag.push` | OUTPUT_3 | bag eject/pusher cylinder |
| DO04 | `dosing.valve_a` | OUTPUT_5 | dosing valve A |
| DO05 | `dosing.valve_b` | OUTPUT_6 | dosing valve B |
| DO06 | `dosing.valve_c` | OUTPUT_7 | dosing valve C |
| DO07 | `filling.motor` | OUTPUT_8 | filling motor contactor/drive command |
| DO08 | `spout.aeration` | OUTPUT_9 | aeration solenoid |

Legacy OUTPUT_4 is not represented as a physical channel.

Firmware uses semantic names. Legacy output numbers exist only as migration/reference metadata.

## 8. Dosing truth table

Preserve the legacy pneumatic behavior:

| Mode | DO04 Valve A | DO05 Valve B | DO06 Valve C |
|---|---:|---:|---:|
| OFF | 0 | 0 | 0 |
| FINE ~30% | 1 | 0 | 1 |
| COARSE 100% | 1 | 1 | 1 |

The controller core reasons in `OFF`, `FINE`, `COARSE`; the output binding expands the mode to three solenoids.

## 9. Dummy physical I/O first

The controller must complete a full physical bench stage before any live packer output connection.

Dummy input harness:

```text
DI01..DI08 <- labelled 24 V switches/selectors
```

Dummy output harness:

```text
DO01..DO08 -> 24 V lamps/electronic dummy loads
```

After logic verification, selected channels are tested using representative inductive loads/spare coils so that inrush, suppression, switching behavior and output-stage temperature are measured.

Prefer a keyed/clearly distinguishable dummy harness versus real-machine harness. Software `BENCH` mode alone must not be the only barrier preventing accidental actuator energization.

See `docs/BOM_SP01_V01.md`.

## 10. Local output electrical rules

All eight local outputs are occupied in v0.1.

Before any field coil is connected, record:

```text
scanner coil current            ____ mA
bag-detect valve current        ____ mA
pusher valve current            ____ mA
dosing valve A current          ____ mA
dosing valve B current          ____ mA
dosing valve C current          ____ mA
filling-motor contactor current ____ mA
aeration valve current          ____ mA
```

Also record voltage, inrush where applicable, suppression, polarity, duty cycle, and cabinet temperature.

Use interposing MOSFET/SSR/relay drivers if any load exceeds the validated controller output capability.

`filling.motor` switches only a contactor/drive command. It must never switch motor power directly.

## 11. Local fail state

On boot, reset, watchdog, brownout, firmware fault, abort, or loss of control authority, desired output image is:

```text
DO01 scanner          defined safe/de-energized state
DO02 bag-detect air   OFF
DO03 pusher           OFF/retracted
DO04 dosing A         OFF
DO05 dosing B         OFF
DO06 dosing C         OFF
DO07 filling motor    OFF
DO08 aeration         OFF
```

These are desired software states only until the actual valve/actuator mechanics are verified. A bistable valve or mechanically latched device invalidates the simple assumption `OFF = safe`.

## 12. RS485 topology and future expansion

v0.1 local bus:

```text
ESP32-S3 Modbus RTU master
        │
        └── TLB485 slave 1
```

Use isolated RS485, shielded twisted pair, correct termination, defined reference/ground strategy, bounded timeout/retry, stale-data detection, and explicit abort behavior.

Future auxiliary I/O may be added by RS485:

```text
ESP RS485-B or validated shared bus
   └── 8/16-channel remote DO/DI module
```

Preferred future rule:

```text
timing-critical dosing/motor/aeration outputs = local DO
auxiliary alarms/beacon/buzzer/service outputs = RS485 expansion
```

If expansion becomes material, prefer a separate UART/RS485 channel for I/O so auxiliary bus faults cannot delay TLB traffic.

## 13. Wi-Fi topology

v0.1 uses exactly one dedicated stationary AP/router and one client node:

```text
SSID: PACKER01-CONTROL

AP/router
   )))
   ))) ESP32-S3 SP01 in STA/client mode
```

Prototype rules:

- dedicated SSID for the packer prototype;
- 2.4 GHz first;
- laptop preferably wired to the AP/router during RF tests so only the rotating SP01 link is under RF test;
- fixed AP location near/above the packer where practical;
- do not depend on plant office/guest Wi-Fi;
- Wi-Fi power-save disabled while machine-powered if supported/configured;
- AP/router and ESP reconnect statistics recorded;
- RSSI, disconnect reason, packet loss/latency and reconnect count exposed in diagnostics;
- external antenna placement preferred when machine steel shields the radio;
- no Internet or cloud dependency for control or HMI.

A per-spout SoftAP/service hotspot may be added later, but it is not required for v0.1 and must not broadcast permanently by default.

## 14. Router/AP role

The AP/router is **not a controller** and not a master.

It provides only:

```text
Wi-Fi association
IP addressing/routing
browser access to SP01
optional uplink to an engineering/plant network later
```

If the AP/router reboots or loses power:

```text
SP01 local filling logic continues
TLB communication continues
local DI/DO continues
HMI becomes OFFLINE/STALE
records buffer locally
```

The next safe local state transition must not require router recovery.

## 15. HMI path

```text
Laptop/browser
   │
AP/router
   ))) Wi-Fi
   │
ESP32-S3 SP01 web server
```

See `docs/HMI.md`.

The browser must never directly address GPIO, raw Modbus registers, or physical coils.

Initial HMI is mostly read-only. Calibration is the required controlled write workflow in v0.1.

## 16. Power architecture

For planning, **24 VDC is assumed available on the rotating assembly**.

Do not purchase a new production PSU by default. First measure the actual source:

```text
nominal/min/max voltage
available current
voltage dip during valves/contactors
noise/transient behavior
0 V / PE / shield relationship
```

If acceptable:

```text
existing rotating 24 V
      │
      ├── fused CONTROL branch → ESP32 + TLB
      │
      └── ACTUATOR branch → existing safety chain → solenoids/contactor coils
```

If measurements show unacceptable noise/dips, add filtering or isolated DC/DC/local conditioning based on evidence.

E-stop and safety-rated functions remain independent of ESP firmware and Wi-Fi.

Prefer controller/TLB to remain powered when actuator power is removed so faults remain observable.

## 17. FreeRTOS execution model

Use a PLC-like process image:

```text
read physical inputs
→ freeze INPUT IMAGE
→ read/validate latest TLB weight snapshot
→ execute SP01 FSM
→ build DESIRED OUTPUT IMAGE
→ apply interlocks
→ commit LOCAL OUTPUT IMAGE
```

Suggested priority domains:

```text
HIGH:   input image / FSM / interlocks / output commit
MEDIUM: TLB RS485 / position acquisition
LOW:    Wi-Fi / HTTP / WebSocket / logging / configuration
```

Control uses monotonic physical time. Wi-Fi, web, storage or calibration UI may never own an actuator and may never block the high-priority control path.

See `docs/FW_SW_PLAN_SP01_V01.md`.

## 18. Procurement

The detailed one-SP01 procurement and hold list is authoritative in `docs/BOM_SP01_V01.md`.

Core procurement now:

```text
1 × ESP32-S3 8DI/8DO + isolated RS485 + Wi-Fi controller carrier
1 × LAUMAS TLB485
1 × dedicated stationary AP/router
1 × external antenna + spare
1 × USB-RS485 adapter
1 × dummy 24 V DI/DO test harness set
1 × enclosure/rail/terminal/wiring set
```

Do not multiply by eight yet.

## 19. Prototype stages

### P0 — digital twin/reference

- Python semantic I/O and cycle model;
- shared conformance scenarios;
- no hardware authority.

### P1 — dummy physical I/O bench

- ESP board;
- 24 V switches on DI01..DI08;
- lamps/dummy loads on DO01..DO08;
- then representative inductive loads;
- AP/router + laptop HMI;
- **no machine wiring**.

### P2 — weighing bench

- TLB + load cell;
- calibration UI/service workflow;
- empty saddle zero;
- 20 kg verification;
- 50 kg standard span;
- zero/20/50 verification;
- filtering, latency/jitter, noise and stale-data tests.

### P3 — integrated wireless dry FSM

- ESP + TLB + dummy outputs;
- complete state machine;
- AP/router running continuously;
- HMI traffic and reconnect stress;
- AP power-cycle/disconnect during every FSM state;
- control timing measured with Wi-Fi enabled/disabled.

### P4 — rotating RF survey / shadow SP01

- mount node/antenna in intended rotating location;
- record RSSI, packet loss, disconnect/reconnect through full 360° rotation at realistic speed;
- connect real SP01 inputs/weight only where safely approved;
- proposed DO remain electrically blocked from actuators;
- compare against legacy sequence.

### P5 — controlled physical output pilot

- one low-risk output at a time;
- explicit authority and physical rollback;
- AP/router intentionally disconnected during test cases to prove network independence.

### P6 — full SP01 pilot

- only SP01 under new control;
- SP02..SP08 remain legacy;
- bag accuracy, throughput, faults, Wi-Fi quality and recovery recorded.

### P7 — architecture decision

Only after SP01 evidence decide whether future nodes use:

```text
8 individual Wi-Fi clients to one AP/router
OR
rotating Ethernet switch + one dedicated wireless bridge
OR
Ethernet-rated rotary joint
```

Do not choose the eight-spout network topology before SP01 RF evidence exists.

## 20. Mandatory v0.1 acceptance tests

Before SP01 live actuation:

1. safe local output states proven on boot/reset/watchdog/brownout;
2. dummy physical I/O bench completed before real output wiring;
3. every legacy SP01 field wire physically traced;
4. all eight coil/contactor loads measured;
5. TLB calibration workflow completed and verification recorded;
6. TLB end-to-end weight latency/filter delay measured;
7. loss/staleness of TLB data forces bounded safe abort;
8. AP/router power-off during every FSM state has **zero control effect**;
9. browser/laptop disconnect has **zero control effect**;
10. Wi-Fi reconnect storms do not violate the measured control timing budget;
11. RF quality measured through a complete rotation, not only with machine stationary;
12. calibration/configuration and event records survive temporary Wi-Fi loss;
13. safety chain remains independent of ESP/Wi-Fi;
14. rollback to legacy SP01 is documented and tested;
15. numeric pass/fail thresholds are frozen before a test is used as evidence for live approval.

## 21. Frozen v0.1 decision

```text
PROTOTYPE SCOPE      SP01 only
FIRST I/O TARGET     dummy physical 24 V I/O harness
CONTROLLER           ESP32-S3 / ESP-IDF / C++ / FreeRTOS
WEIGHER              LAUMAS TLB485
CALIBRATION          HMI-guided zero + 50 kg span + 20/50 kg verification
LOCAL INPUTS         7 used + 1 spare DI
LOCAL OUTPUTS        8 used DO
OUTPUT EXPANSION     optional RS485, auxiliary only initially
LOCAL FIELDBUS       isolated RS485
SUPERVISORY NETWORK  Wi-Fi
STATIONARY NETWORK   1 dedicated AP/router
HMI                  ESP-hosted web UI + laptop/browser
24 V SUPPLY          assumed available; verify before real machine
ETHERNET TO ROTOR    none
CARBON BRUSH DATA    none
SAFETY               existing independent hardwired/safety chain
```

The v0.1 architectural test remains simple:

> **Turn off the AP/router while SP01 is running. The local controller must remain deterministic and transition correctly because Wi-Fi was never part of the control loop.**