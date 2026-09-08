# Weight Calibration

Calibration is a required service UI function.

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

## UI rules

Calibration is allowed only when:

```text
machine stopped
outputs disabled
service/calibration mode active
TLB healthy
weight stable
```

The browser calls a calibration service. It never writes raw Modbus registers.

## Keep with each calibration

```text
spout_id
timestamp
operator
TLB serial/revision
zero reading
20 kg check
50 kg span/verification
result
firmware version
```
