# Weight Calibration

Calibration is a required service UI function and belongs to the weighing chain, not to the disposable ESP controller.

## Production signal boundary

```text
load cell -> TLB485 -> isolated RS485 -> ESP32
```

The ESP32 does not calibrate a raw load-cell ADC in production. The browser calls a guarded calibration service; it never writes raw Modbus registers.

## Controller replacement rule

Replacing only the ESP32 controller while retaining the same healthy TLB485/load-cell chain must **not** automatically change zero or span.

A normal ESP replacement therefore restores communication/configuration and verifies the existing weighing chain; it does not issue calibration writes on boot.

Recommended post-swap check:

```text
TLB online
correct Modbus address/profile
zero reading plausible
known reference / external check agrees as expected
no calibration-write command issued
```

If the TLB485 itself, load cell, mechanical weighing assembly or calibration state changes, then use the formal calibration procedure below.

## Procedure

```text
1. Empty saddle
   wait STABLE
   SET ZERO

2. Place known 20.000 kg
   wait STABLE
   CHECK / CAPTURE

3. Place 50.000 kg standard
   wait STABLE
   SET SPAN 50 kg

4. Remove weight
   VERIFY ZERO

5. Place 20 kg
   VERIFY

6. Place 50 kg
   VERIFY

7. SAVE
```

The 50 kg standard is the span reference. The 20 kg point is primarily a linearity/mechanical check unless a validated multi-point TLB procedure is adopted.

## Calibration is not target compensation

Operating targets such as 50.0 / 50.1 / 50.2 kg are production recipes. They may be switched quickly based on an external check scale while the calibrated weighing chain remains unchanged.

Do not move zero/span to compensate a production target offset.

## UI rules

Calibration is allowed only when:

```text
machine stopped
fill switch OFF
controller idle/safe
all outputs OFF
TLB healthy
weight fresh + stable
calibration writes explicitly enabled
service token valid
```

DI7 and DI8 remain discharge-reference inputs; they are not calibration/service switches.

A newly replaced ESP controller should start with calibration writes disabled until service state and installed TLB identity are verified.

## Keep with each calibration

Calibration audit data must be recoverable outside one ESP flash device.

```text
spout_id
timestamp
operator
TLB serial/revision
RS485 profile
zero reading
20 kg check
50 kg span/verification
result
firmware version
```

Also keep controller-replacement history separately so a board swap is not confused with a weighing recalibration event.

Weighing transport design and G4 evidence are defined in [`WEIGHING.md`](WEIGHING.md). Service/replacement rules are in [`SERVICEABILITY.md`](SERVICEABILITY.md).
