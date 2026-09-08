# Hardware Prototype v0.1 — SP01 Wireless Node

## Status

**Frozen bench/shadow prototype architecture. Not approved for live packer actuation.**

v0.1 intentionally builds **one spout only: SP01**. Do not purchase or build eight nodes yet.

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
                     Wi-Fi or LAN
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
                 │ FreeRTOS          │
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
                  ┌──────────┐  machine devices
                  │ LAUMAS   │
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
machine DI
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

The supervisory path may observe and issue validated non-time-critical commands, but is not part of filling cutoff, interlocks, or actuator timing.

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

All field DI are assumed to be industrial 24 V signals only after physical tracing and measurement. No legacy 24 V signal may connect directly to MCU GPIO.

## 6. Weighing channel

```text
load cell → LAUMAS TLB485 → isolated RS485 → ESP32-S3
```

Semantic input:

```text
W01 = weight.net
```

The TLB owns load-cell excitation, A/D conversion, calibration/filtering, and weight/status transport.

TLB-specific Modbus addresses belong only in the TLB adapter.

Required interface:

```text
WeighingUnit
  read_weight()
  read_status()
  tare()
  zero()
  health()
  diagnostics()
```

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

The controller core should reason in `OFF`, `FINE`, `COARSE`; the output binding expands the mode to three solenoids.

## 9. Local output electrical rules

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

## 10. Local fail state

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

## 11. RS485 topology and future expansion

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

## 12. Wi-Fi topology

v0.1 uses exactly one dedicated stationary AP/router and one client node:

```text
SSID: PACKER01-CONTROL

AP/router
   )))
   ))) ESP32-S3 SP01 in STA/client mode
```

Recommended prototype rules:

- dedicated SSID for the packer prototype;
- 2.4 GHz first;
- fixed AP location near/above the packer where practical;
- do not depend on plant office/guest Wi-Fi;
- Wi-Fi power-save disabled while machine-powered if supported/configured;
- AP/router and ESP reconnect statistics recorded;
- RSSI, disconnect reason, packet loss/latency and reconnect count exposed in diagnostics;
- external/remote antenna placement preferred if the controller enclosure or machine steel significantly shields the radio;
- no Internet or cloud dependency for control or HMI.

A per-spout SoftAP/service hotspot may be added later, but it is not required for v0.1 and must not broadcast permanently by default.

## 13. Router/AP role

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

## 14. HMI path

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

## 15. Power architecture

The rotating node needs stable local control power. Reuse existing rotating 24 V only after measuring quality and grounding; otherwise add local conditioning/DC-DC as required.

Conceptual split:

```text
rotating 24 V supply
      │
      ├── CONTROL branch → ESP32 + TLB
      │
      └── ACTUATOR branch → existing safety chain → solenoids/contactor coils
```

E-stop and safety-rated functions remain independent of ESP firmware and Wi-Fi.

Prefer controller/TLB to remain powered when actuator power is removed so faults remain observable.

## 16. FreeRTOS execution model

Use a PLC-like process image:

```text
read physical inputs
→ freeze INPUT IMAGE
→ read/validate latest TLB weight
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

Wi-Fi or web work may never own an actuator and may never block the high-priority control path.

## 17. v0.1 BOM — one SP01 node + one AP/router

**Budgetary only. Verify exact supplier and variant before purchase.**

| Item | Qty | Budget range, VND | Notes |
|---|---:|---:|---|
| LAUMAS TLB RS485 | 1 | 5.5–7.0 M | weighing transmitter |
| ESP32-S3 industrial 8DI/8DO + isolated RS485 + Wi-Fi carrier | 1 | 1.8–2.5 M | prototype controller |
| dedicated Wi-Fi AP/router | 1 | 0.5–2.0 M | stationary prototype network |
| 24 VDC supply/conditioning as required | 1 | 0.6–1.2 M | size after actual load survey |
| DIN enclosure/backplate/rail | 1 set | 0.4–0.8 M | rotating SP01 node |
| fused terminals/fuses/MCB | 1 set | 0.3–0.6 M | protection |
| safety/actuator isolation additions | 1 set | 0.3–0.8 M | depends on existing machine |
| shielded RS485 cable | 1 set | 0.1–0.2 M | local TLB link |
| wire/ferrules/labels/glands | 1 set | 0.3–0.6 M | cabinet build |
| optional external antenna/pigtail | 1 | 0.1–0.5 M | if RF survey requires |
| optional interposing output drivers | as needed | 0–1.0 M | after coil survey |

Do not multiply the BOM by eight yet.

## 18. Prototype stages

### P0 — current digital twin

- semantic I/O and cycle model;
- no hardware authority.

### P1 — SP01 controller bench

- ESP board + switches/24 V input simulator;
- lamps/dummy loads on all DO;
- AP/router + laptop HMI;
- no machine wiring.

### P2 — weighing bench

- TLB + representative load cell;
- calibration, filtering, latency/jitter and stale-data tests.

### P3 — wireless soak + dry FSM

- ESP + TLB + dummy outputs;
- AP/router running continuously;
- HMI traffic and reconnect stress;
- AP power-cycle/disconnect during every FSM state;
- control timing measured with Wi-Fi enabled/disabled.

### P4 — rotating RF survey / shadow SP01

- mount node/antenna in intended rotating location;
- record RSSI, packet loss, disconnect/reconnect through full 360° rotation at realistic machine speeds;
- read actual SP01 signals/weight where safely possible;
- calculate but physically block proposed outputs;
- compare against legacy sequence.

### P5 — controlled physical output pilot

- one low-risk output at a time;
- explicit authority and rollback;
- AP/router may be intentionally disconnected to prove network independence.

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

## 19. Mandatory v0.1 acceptance tests

Before SP01 live actuation:

1. safe local output states proven on boot/reset/watchdog/brownout;
2. every legacy SP01 field wire physically traced;
3. all eight coil/contactor loads measured;
4. TLB end-to-end weight latency/filter delay measured;
5. loss/staleness of TLB data forces bounded safe abort;
6. AP/router power-off during every FSM state has **zero control effect**;
7. browser/laptop disconnect has **zero control effect**;
8. Wi-Fi reconnect storms do not violate control timing budget;
9. RF quality measured through a complete rotation, not only with machine stationary;
10. configuration and event records survive temporary Wi-Fi loss;
11. safety chain remains independent of ESP/Wi-Fi;
12. rollback to legacy SP01 is documented and tested;
13. red-team blockers in `docs/REDTEAM_PROTOTYPE_HMI.md` are closed or explicitly accepted.

## 20. Frozen v0.1 decision

```text
PROTOTYPE SCOPE      SP01 only
CONTROLLER           ESP32-S3 / ESP-IDF / FreeRTOS
WEIGHER              LAUMAS TLB485
LOCAL INPUTS         7 used + 1 spare DI
LOCAL OUTPUTS        8 used DO
OUTPUT EXPANSION     optional RS485, auxiliary only initially
LOCAL FIELDBUS       isolated RS485
SUPERVISORY NETWORK  Wi-Fi
STATIONARY NETWORK   1 dedicated AP/router
HMI                  ESP-hosted web UI + laptop/browser
ETHERNET TO ROTOR    none
CARBON BRUSH DATA    none
SAFETY               existing independent hardwired/safety chain
```

The v0.1 architectural test is simple:

> **Turn off the AP/router while SP01 is filling. The local controller must remain deterministic and transition to the correct safe local state exactly as if Wi-Fi never existed.**
