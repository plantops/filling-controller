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

## Bench first

```text
DI1..DI8 <- 24 V switches
DO1..DO8 -> lamps/dummy loads
RS485    -> TLB485 -> bench load cell
```

Use representative 24 V inductive loads after logic testing.

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
