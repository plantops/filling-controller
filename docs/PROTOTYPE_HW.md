# One-Spout Hardware Prototype

## Status

**Prototype proposal only — not approved for connection to live packer outputs.**

This document defines the first physical prototype for one filling spout of an eight-spout rotary cement packer. The machine-level target remains approximately 2,000 bags/h or higher. At 2,000 bags/h, one spout completes one cycle every:

```text
3600 s/h × 8 spouts / 2000 bags/h = 14.4 s/spout-cycle
```

The prototype deliberately uses **one independent controller per spout**. There is no central controller in the critical path.

## 1. Design objective

Build one self-contained filling node that can replace the legacy control logic for a single spout while preserving the proven mechanical sequence:

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

The prototype must support simulation, bench I/O, shadow operation, and later one-spout live actuation without changing the semantic controller logic.

## 2. Per-spout architecture

```text
                       ONE FILLING NODE

                 ┌────────────────────────┐
                 │ ESP32-S3 + FreeRTOS    │
                 │                        │
                 │ deterministic FSM      │
                 │ 8 isolated DI          │
                 │ 8 protected DO         │
                 │ isolated RS485         │
                 │ Ethernet               │
                 │ embedded web HMI       │
                 └───────┬────────┬───────┘
                         │        │
                    RS485│        │field I/O
                         │        │
                         ▼        ▼
                    LAUMAS TLB   machine
                         │       devices
                         │
                     load cell
```

The ESP32-S3 owns the machine sequence and I/O state. The TLB owns load-cell excitation, A/D conversion, filtering/calibration, and transmission of weight/status data.

The HMI is supervisory only. A laptop or browser must never be required for a fill cycle to complete safely.

## 3. Prototype controller hardware

Preferred prototype form factor:

- ESP32-S3 controller board with industrial 24 V interfaces;
- 8 optically isolated digital inputs;
- 8 protected transistor digital outputs;
- isolated RS485;
- wired Ethernet;
- 24 VDC supply input;
- watchdog/brownout support;
- DIN-rail or panel-mountable enclosure.

A current candidate is a Waveshare ESP32-S3 Ethernet 8DI/8DO class board. **The architecture must not depend on that brand or board.** It is a prototype carrier only.

Production acceptance of any board requires independent review of:

- actual input threshold and isolation rating;
- output current and thermal derating;
- common/ground topology;
- flyback and surge protection;
- EMC immunity/emissions;
- brownout/reset behavior;
- operating temperature;
- vibration and dust exposure;
- connector retention;
- long-term availability.

## 4. I/O baseline

### Digital inputs

| ID | Semantic signal | Legacy function |
|---|---|---|
| DI01 | `hopper.feeder_running` | hopper feeder status |
| DI02 | `downstream.conveyor_ready` | downstream conveyor status |
| DI03 | `machine.motor_running` | rotary/main machine motor status |
| DI04 | `process.initiative` | process permissive/initiative |
| DI05 | `cycle.fill_position` | fill-position trigger; exact legacy sensor to be confirmed |
| DI06 | `bag.present` | bag-detect pressure switch |
| DI07 | `position.push` | push/discharge-position signal |
| DI08 | `spare` | reserved |

### Weighing channel

| ID | Semantic signal | Hardware |
|---|---|---|
| W01 | `weight.net` | load cell → LAUMAS TLB → RS485 |

### Digital outputs

| ID | Semantic actuator | Legacy function |
|---|---|---|
| DO01 | `scanner.down` | scanner cylinder solenoid |
| DO02 | `bag_detect_air` | bag-detection air solenoid |
| DO03 | `dosing.valve_a` | dosing valve A |
| DO04 | `dosing.valve_b` | dosing valve B |
| DO05 | `dosing.valve_c` | dosing valve C |
| DO06 | `filling.motor` | filling motor contactor command |
| DO07 | `spout.aeration` | aeration solenoid |
| DO08 | `bag.push` | push-off cylinder solenoid |

No analog output is required by the legacy mechanism.

## 5. Weighing front end

Prototype default:

```text
LAUMAS TLB RS485
```

Reason for using an industrial transmitter rather than HX711:

- industrial load-cell excitation and signal conditioning;
- stable calibration and filtering;
- DIN-rail form factor;
- digital communication to the controller;
- reduced sensitivity to MCU-board analog layout;
- replaceable `WeighingUnit` adapter boundary.

The controller must not contain TLB register numbers outside the TLB adapter.

Required semantic interface:

```text
WeighingUnit
  read_weight()
  read_status()
  tare()
  zero()
  health()
  diagnostics()
```

If later replaced by Dini Argeo, Mettler Toledo, SIWAREX, or an open weighing board, the spout FSM must remain unchanged.

## 6. I/O electrical design

### Inputs

All 24 V field inputs must enter through isolated industrial DI stages. Do not connect legacy 24 V signals directly to ESP32 GPIO.

### Outputs

The controller output stage may directly drive a 24 V coil only after the following are measured and documented:

- coil nominal voltage;
- steady-state current;
- inrush current if relevant;
- polarity;
- inductive suppression already present;
- switching frequency/duty cycle;
- controller-channel current rating at cabinet temperature.

If any load exceeds the validated output capability, use an interposing MOSFET/SSR/relay driver.

`filling.motor` means **contactor/drive command only**. The ESP32 output must never switch the filling motor power directly.

### Output fail state

On boot, reset, watchdog timeout, firmware crash, RS485 failure requiring abort, or loss of control authority:

```text
dosing valves     CLOSED / de-energized
filling motor     OFF
aeration          OFF
push cylinder     OFF
bag-detect air    OFF or defined safe state
scanner           defined mechanical safe state
```

The exact safe state of scanner and pneumatic elements must be verified against the existing machine before physical actuation.

## 7. Safety boundary

The prototype is **not the safety system**.

Existing certified/hardwired safety functions remain authoritative, including emergency stop and motor/actuator isolation.

Recommended power split:

```text
                     230 VAC
                        │
                       MCB
                        │
                  24 VDC PSU
                        │
             ┌──────────┴──────────┐
             │                     │
       CONTROL 24 V           ACTUATOR 24 V
             │                     │
        ESP32 + TLB          safety contactor /
                             existing E-stop chain
                                   │
                                solenoids
                              contactor coils
```

On E-stop, controller electronics should preferably stay powered for diagnostics while actuator 24 V is removed by the safety chain.

The controller should receive a read-only `safety.healthy` or `actuator_power.available` status when available.

## 8. RS485 topology

Initial prototype:

```text
ESP32-S3 RS485 master
        │
        └──── LAUMAS TLB slave
```

Use:

- isolated transceiver;
- shielded twisted pair;
- correct termination at physical bus ends;
- defined signal reference/ground strategy;
- timeout and stale-data detection;
- bounded retry policy;
- explicit fault reaction to loss of weighing data.

Do not allow indefinite retries to stall the control task.

## 9. FreeRTOS execution model

One spout does not need complicated concurrency.

Preferred logical tasks:

```text
high priority:
  input/process-image acquisition
  spout FSM
  interlock evaluation
  output-image commit

medium priority:
  TLB/RS485 communications
  position acquisition

low priority:
  web HMI / WebSocket
  logging
  diagnostics
  configuration
```

The application should use a PLC-like process image:

```text
read physical inputs
→ freeze INPUT IMAGE
→ execute FSM
→ build DESIRED OUTPUT IMAGE
→ apply hard interlocks
→ commit PHYSICAL OUTPUTS
```

No state may use blocking `delay()` or unbounded waits.

Every state must have:

- entry action;
- tick logic;
- exit condition;
- timeout;
- defined fault reaction.

## 10. One firmware image, eight independent nodes

If the SP01 pilot succeeds, deploy the same firmware to eight nodes:

```text
SP01 ESP32 + TLB + local I/O
SP02 ESP32 + TLB + local I/O
...
SP08 ESP32 + TLB + local I/O
```

Only configuration differs:

```yaml
node:
  machine: PACKER01
  spout: SP03
  node_id: 3
  angle_offset_deg: 90
```

A failure of one spout node must not require the other seven nodes to stop unless the mechanical/safety architecture demands it.

Machine-wide permissives may be hardwired/distributed or delivered by a separately validated machine-level mechanism. The local node must not depend on another spout's HMI or application process to complete a safe abort.

## 11. HMI/network boundary

The ESP32 serves its own local engineering HMI over wired Ethernet. See `docs/HMI.md`.

Loss of:

- laptop;
- browser;
- Ethernet link;
- machine-level HMI;
- historian;

must not stop or corrupt a local filling cycle.

## 12. Budgetary BOM — one spout

**Budgetary only. Verify supplier quotations and exact variants before purchase.**

| Item | Qty | Budget range, VND | Notes |
|---|---:|---:|---|
| LAUMAS TLB RS485 | 1 | 5.5–7.0 M | weighing transmitter |
| ESP32-S3 industrial 8DI/8DO + isolated RS485 + Ethernet carrier | 1 | 1.8–2.5 M | prototype carrier |
| 24 VDC industrial PSU, approx. 5 A | 1 | 0.6–1.0 M | size after coil survey |
| DIN enclosure/backplate/rail | 1 set | 0.4–0.8 M | prototype |
| fused terminals/fuses/MCB | 1 set | 0.3–0.6 M | protection |
| safety/actuator isolation additions | 1 set | 0.3–0.8 M | depends on existing circuit |
| RS485/Ethernet cabling | 1 set | 0.1–0.3 M | shielded field wiring |
| wire/ferrules/labels/glands | 1 set | 0.3–0.6 M | cabinet build |
| optional interposing output drivers | as needed | 0–1.0 M | only after coil survey |

Expected prototype hardware budget excluding existing load cell, sensors, cylinders, valves, contactors, and motor:

```text
approximately 9.3–14.6 M VND per spout
```

Do **not** multiply this by eight for purchase authorization until the one-spout pilot passes the acceptance gates.

## 13. Prototype stages

### P0 — simulation only

- current Python digital twin;
- semantic I/O frozen;
- no hardware.

### P1 — controller bench

- ESP32 board powered on bench;
- all DI simulated by switches/24 V test source;
- all DO connected to lamps/dummy loads;
- no machine wiring.

### P2 — weighing bench

- TLB + representative load cell;
- calibration;
- weight stream latency/jitter measurement;
- tare/zero tests;
- noise/vibration tests where practical.

### P3 — integrated dry cycle

- ESP + TLB + dummy outputs;
- full FSM at realistic timing;
- replay and HMI verification;
- fault injection.

### P4 — shadow on SP01

- read real machine inputs and weight;
- calculate outputs but do not energize them;
- compare proposed state/output timeline with legacy controller.

### P5 — controlled physical output pilot

- one low-risk output at a time;
- explicit commissioning authorization;
- immediate rollback path to legacy control.

### P6 — full SP01 pilot

- one spout under open control;
- other seven spouts remain legacy;
- throughput/accuracy/fault metrics recorded.

### P7 — replication

Only after SP01 proves stability, accuracy, maintainability, and safe failure behavior.

## 14. Acceptance measurements

Record, do not assume:

- controller-cycle period and worst-case jitter;
- maximum FSM execution time;
- RS485 round-trip latency distribution;
- maximum age of accepted weight measurement;
- command-to-output electrical latency;
- output-to-pneumatic/mechanical response latency;
- boot time;
- watchdog recovery time;
- brownout behavior;
- flash/config integrity after repeated power cycling;
- HMI CPU/RAM/network impact on control task;
- output state during reset and firmware update;
- final bag weight distribution;
- cutoff weight and residual/inflight mass;
- coarse/fine fill times;
- missed/false bag detection;
- fault recovery behavior.

## 15. Prototype pass criteria

Before connecting all SP01 outputs, at minimum:

1. safe output states are demonstrated on boot/reset/watchdog/power loss;
2. browser/network failure has zero control effect;
3. loss/staleness of weighing data forces a bounded safe abort;
4. every state timeout has been tested;
5. direct coil-drive current/thermal limits are measured, not assumed;
6. the existing emergency/safety chain remains independent;
7. shadow-mode timing agrees sufficiently with the proven legacy sequence;
8. configuration changes are versioned and recoverable;
9. a physical rollback to legacy SP01 control is documented and tested;
10. red-team blockers in `docs/REDTEAM_PROTOTYPE_HMI.md` are closed or explicitly accepted.

## 16. Non-goals for prototype

The first prototype does not attempt to prove:

- functional-safety certification;
- eight-spout machine-wide optimization;
- custom PCB readiness;
- wireless machine control;
- cloud dependency;
- full plant MES integration;
- replacement of every legacy machine circuit.

The purpose is to prove one independent, open, observable, recoverable filling node.