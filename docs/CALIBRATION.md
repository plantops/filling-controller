# Weight Calibration

Calibration is a required service UI function.

## Production signal boundary

Calibration belongs to the TLB485 weighing chain:

```text
load cell -> TLB485 -> isolated RS485 -> ESP32
```

The ESP32 does not calibrate a raw load-cell ADC in production. The browser calls a guarded calibration service; it never writes raw Modbus registers.

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

## Keep with each calibration

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

Weighing transport design and G4 evidence are defined in [`WEIGHING.md`](WEIGHING.md).
