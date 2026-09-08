# SP01 v0.1 BOM and Procurement List

## Status

**Approved for one-spout bench prototype procurement only. Not approval for live packer actuation or eight-spout rollout.**

The purpose of this BOM is to build one SP01 controller on a bench, connect it first to dummy physical I/O, integrate the weighing transmitter and standard weights, and only later connect to the real rotating packer through explicit commissioning gates.

Current assumptions:

- one prototype only: `SP01`;
- rotating 24 VDC supply is assumed available;
- exact 24 V quality, grounding and available current still require measurement before machine connection;
- no Ethernet connection crosses the rotating/stationary boundary;
- one dedicated stationary Wi-Fi AP/router provides supervisory access;
- SP01 remains autonomous if Wi-Fi/AP/router is absent;
- all eight local DO channels are occupied;
- RS485 expansion is optional and is not required for v0.1;
- existing machine load cell, pneumatic devices, position sensors, contactors and safety chain are reused only after tracing/verification.

## 1. Core procurement — buy for SP01

| Item | Qty | Procurement status | Purpose / requirement |
|---|---:|---|---|
| ESP32-S3 controller carrier with 8 isolated DI, 8 protected DO, isolated RS485, Wi-Fi and external antenna support | 1 | BUY | SP01 controller prototype |
| LAUMAS TLB485 RS485 weighing transmitter | 1 | BUY | load-cell excitation, A/D, calibration/filtering, weight/status |
| Dedicated 2.4 GHz Wi-Fi AP/router | 1 | BUY | stationary supervisory network only |
| External 2.4 GHz antenna + suitable pigtail/connector | 1 + 1 spare | BUY | rotating steel-machine RF testing |
| USB-RS485 engineering adapter | 1 | BUY | independent TLB diagnostics/commissioning |
| Shielded twisted-pair RS485 cable | 5–10 m | BUY | local ESP↔TLB bench/rotor wiring |
| DIN rail / prototype enclosure / backplate | 1 set | BUY | SP01 bench and later rotating mounting |
| Fused terminals, terminal blocks, fuses/MCB | 1 set | BUY | protected bench/control wiring |
| Ferrules, wire labels, glands, wiring | 1 set | BUY | maintainable prototype build |
| 24 V test toggle switches / maintained selectors | 8 | BUY | dummy DI panel |
| 24 V indicator lamps / electronic dummy loads | 8 | BUY | first DO logic tests |
| Representative 24 V inductive dummy loads or spare solenoid coils | several | BUY/BORROW | DO inrush/thermal/inductive switching tests |
| USB programming/debug cable | 2 | BUY | firmware development/service |

No purchase quantity above should be multiplied by eight until SP01 passes the defined gates.

## 2. Existing / assumed available

The following are not part of the initial purchase order unless inspection proves otherwise:

```text
24 VDC rotating supply
SP01 load cell
scanner cylinder/solenoid
bag-detect pneumatic circuit
bag pusher cylinder/solenoid
dosing valves A/B/C
filling-motor contactor/drive interface
spout aeration valve
fill-position signal
push-position signal
machine permissive signals
existing hardwired safety/E-stop chain
50 kg standard calibration weight
20 kg known reference weight if available
```

The 24 V supply is an **assumption for planning, not an electrical acceptance result**. Before real-machine connection record at minimum:

```text
nominal voltage
minimum/maximum voltage during operation
available current
voltage dip during valve/contactor operation
0 V / PE / shield relationship
noise/transient observations where practical
```

## 3. Do not buy yet

Delay these until SP01 evidence justifies them:

```text
7 additional ESP controllers
7 additional TLB transmitters
RS485 remote I/O expansion
custom PCB
custom eight-node panel production
industrial-grade second AP
rotating Ethernet switch
wireless bridge
Ethernet-rated rotary joint/slip ring
large interposing relay bank
new rotary encoder
plant historian/gateway hardware
```

Interposing output drivers are purchased only after actual machine coil voltage/current/inrush/suppression are measured.

## 4. Dummy physical I/O bench harness

The first electrical connection is **not the packer**.

Build a keyed/labelled dummy harness matching the future SP01 terminal layout:

```text
DI01  hopper.feeder_running      <- 24 V switch
DI02  downstream.conveyor_ready  <- 24 V switch
DI03  machine.motor_running      <- 24 V switch
DI04  process.initiative         <- 24 V switch
DI05  cycle.fill_position        <- 24 V switch
DI06  bag.present                <- 24 V switch
DI07  position.push              <- 24 V switch
DI08  spare/test                 <- 24 V switch

DO01  scanner.down               -> lamp/dummy load
DO02  bag_detect_air             -> lamp/dummy load
DO03  bag.push                   -> lamp/dummy load
DO04  dosing.valve_a             -> lamp/dummy load
DO05  dosing.valve_b             -> lamp/dummy load
DO06  dosing.valve_c             -> lamp/dummy load
DO07  filling.motor              -> lamp/dummy contactor load
DO08  spout.aeration             -> lamp/dummy load
```

After logic verification, selected DO channels are retested using representative inductive loads/spare coils to measure switching behavior and output-stage temperature.

The machine-output harness and dummy-output harness should be physically distinguishable/keyed where practical. Software mode alone must not be the only protection against accidental machine actuation during bench work.

## 5. Bench power

Because 24 V is assumed available on the machine, a new production PSU is not automatically part of the SP01 purchase.

For bench development use an existing verified 24 V bench supply or procure a small industrial bench supply if one is not already available.

Before moving the controller to the rotor, decide whether the existing rotating 24 V can power:

```text
ESP32 control electronics
TLB485
local control/diagnostic circuits
```

without unacceptable dips/noise. Add filtering/DC-DC isolation only if measurements require it.

## 6. Procurement hold points

Purchasing one SP01 set is allowed now. Further purchasing is held at these gates:

```text
H0  parts received / inspected
H1  dummy DI/DO bench passes
H2  TLB + load-cell calibration/latency bench passes
H3  complete dry FSM + Wi-Fi stress passes
H4  rotating RF survey + shadow comparison passes
H5  live SP01 pilot accepted
H6  only then consider x8 replication
```

## 7. Evidence to retain with the BOM

Record actual manufacturer/variant, supplier, serial number where applicable, purchase date, firmware/hardware revision and measured electrical characteristics. The BOM should evolve from a budget list into an as-built record for SP01.