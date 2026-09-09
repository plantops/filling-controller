# SP01 Weighing Interface v0.1

This document defines the production boundary between the load cell system and the SP01 controller.

## Canonical signal path

```text
load cell bridge
    |
    | mV/V
    v
LAUMAS TLB485
    |
    | digital weight / status
    | Modbus RTU over isolated RS485
    v
Waveshare ESP32-S3 RS485 terminal
    |
    v
asynchronous TLB task
    |
    v
latest validated WeightSnapshot
    |
    v
100 Hz controller loop
```

Production ESP32 firmware does **not** read the raw load-cell bridge directly.

Weight is **not carried on a DI**. A DI can only carry a binary status and cannot replace the continuous digital weight stream required by filling control. All eight SP01 DIs are already assigned to machine signals, so v0.1 allocates no DI to weighing.

## Lifecycle boundary

The weighing chain is deliberately separated from the low-cost replaceable ESP controller.

```text
valuable / calibrated chain                replaceable control node
load cell -> TLB485 -> RS485 A/B    --->   ESP controller
```

Replacing only the ESP controller must not require changing load-cell wiring or automatically recalibrating the TLB485.

The spare controller must recover the correct TLB serial profile/address and resume read-only weight acquisition before any calibration-write capability is enabled.

If the TLB itself is replaced or its calibration state changes, follow the formal calibration procedure in [`CALIBRATION.md`](CALIBRATION.md).

## Physical connection

Use the board's isolated RS485 terminal:

```text
TLB485 A / D+  -> ESP board RS485 A
TLB485 B / D-  -> ESP board RS485 B
COM/GND        -> only as required by the verified TLB/board wiring scheme
```

Firmware board mapping behind the onboard RS485 transceiver is currently:

```text
TX   GPIO17
RX   GPIO18
RTS  GPIO21
```

Do not bypass the onboard RS485 transceiver and do not connect TLB485 directly to normal 24 V DI terminals.

## Runtime architecture

The weighing task owns Modbus polling. The high-priority controller never blocks waiting for RS485.

```text
TLB task -> WeightSnapshot {
  net_kg,
  sample_time_us,
  stable,
  quality,
  sequence
}

control task -> reads latest validated snapshot only
```

A stale, invalid or communication-fault weight snapshot causes bounded safe handling by the controller.

## Communication profile

Current bench bring-up defaults remain conservative:

```text
9600 bit/s
Modbus address 1
poll 50 ms
```

After the installed TLB485 is configured and G4 measurements are clean, the intended high-rate operating profile is:

```text
115200 bit/s
poll 20 ms   ~= 50 weight updates/s
```

A 10 ms poll interval is permitted for testing only after measured latency, TLB response time, RS485 error rate and controller timing show adequate margin. Faster polling is not a goal by itself.

The TLB and ESP settings must always match. G4 records actual update rate, latency, jitter, communication errors, stale-data behavior and reconnect behavior.

## 70 °C environment

Possible machine ambient may reach approximately 70 °C. The TLB485 is not automatically assigned the same disposable-lifetime philosophy as the inexpensive ESP controller.

During G2T/G4, record temperature at the actual TLB location together with:

```text
weight update rate
RS485 error count
stale events
reconnect behavior
zero stability / reading behavior
```

If the installed transmitter's verified environmental capability or measured field behavior is inadequate, prefer relocating it to a cooler point or selecting an appropriate transmitter rather than accepting uncontrolled weighing-chain failures.

## Filling target strategy

v0.1 keeps the proven operator compensation method simple. Filling recipes differ primarily by target weight while the other tuned filling parameters remain unchanged.

Example recipe bank:

```text
50.0 kg
50.1 kg
50.2 kg
50.3 kg
...
```

During production, an external check scale is used to verify actual bag weight. The operator can switch target recipe quickly to compensate for observed plant conditions.

This is deliberately operator-controlled in v0.1:

- no PID is required for the three-state pneumatic dosing system;
- no automatic AI/learning target correction is required;
- do not alter calibration to compensate a production target offset;
- calibration and operating target are separate concepts.

## Optional binary weighing status

A future TLB relay/status output may be wired to a DI only if the machine I/O allocation is redesigned. It may provide an independent status such as `weight_valid` or `overweight`, but it remains secondary diagnostics/interlock information. It does not replace RS485 digital weight.

## G4 acceptance evidence

Before production authority, measure and record:

```text
0 / 20 / 40 / 49 / 50 kg digital readings
update frequency
end-to-end age/latency
jitter
Modbus error count
stale detection
cable disconnect behavior
reconnect behavior
noise with representative 24 V inductive loads switching
behavior at representative elevated temperature
```

## Controller replacement check

After replacing only the ESP node:

```text
confirm firmware/config identity
confirm TLB address/profile
confirm read-only digital weight is healthy
confirm no zero/span write occurred
compare a known reference / external check as required
return calibration writes to disabled unless service action explicitly requires them
```

Accuracy/calibration evidence is handled separately in [`CALIBRATION.md`](CALIBRATION.md). Controller lifecycle and spare policy are defined in [`SERVICEABILITY.md`](SERVICEABILITY.md).
