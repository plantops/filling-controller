# SP01 Hardware v0.1

## Topology

```text
STATIONARY
Laptop -> dedicated 2.4 GHz AP/router
                         ))) supervisory Wi-Fi

ROTATING SP01
ESP32-S3
  |- 8 isolated DI
  |- 8 protected DO
  |- isolated RS485 -> TLB485 -> load cell
  `- local web HMI
```

No Ethernet crosses the rotating boundary.

## Weighing hardware boundary

Production weight follows one canonical path:

```text
load cell bridge
    -> LAUMAS TLB485
    -> isolated RS485 terminal on ESP32 board
    -> digital WeightSnapshot in firmware
```

Rules:

- do not connect the raw mV/V load-cell bridge to ESP32 ADC/GPIO;
- do not use a 24 V DI for continuous weight values;
- all eight DIs remain reserved for machine signals;
- optional binary TLB status via DI is future-only and would require I/O reallocation;
- use the board RS485 A/B terminal, not raw UART pins at the panel boundary.

Current onboard RS485 firmware mapping is TX GPIO17, RX GPIO18, RTS GPIO21 behind the board transceiver.

Current communication bring-up profile is 9600 bit/s, address 1, 50 ms poll. After G4 proves clean transport, target high-rate operation is 115200 bit/s with 20 ms polling; 10 ms is test-only after measured margin.

See [`WEIGHING.md`](WEIGHING.md).

## Bench first

```text
DI1..DI8 <- 24 V switches
DO1..DO8 -> lamps/dummy loads
RS485    -> TLB485 -> bench load cell
```

Use representative 24 V inductive loads after logic testing. During G4, measure RS485 errors, weight age/latency and reconnect behavior while inductive loads are switching.

## Power

24 VDC is assumed available on the rotating machine. Before field connection measure voltage range, available current, grounding and voltage dip during switching.

Keep control power and actuator power separately protected. Existing E-stop/safety isolation remains independent of ESP32.

## Outputs

All local DO are used. `filling.motor` drives only a contactor/drive command, never motor power.

On boot/reset/watchdog/fault, software commands all outputs OFF. Confirm the real pneumatic/mechanical safe state before machine connection.

## Network rule

AP/router loss may make the HMI offline, but must not change local control behavior.

## Hardware references

See [`../hardware/README.md`](../hardware/README.md).
