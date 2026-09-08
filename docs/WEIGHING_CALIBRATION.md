# SP01 Weighing Calibration

## Status

**Required v0.1 firmware/HMI feature.**

Weight calibration is part of the controller product, not a hidden engineering-only Modbus procedure. The web HMI must provide a controlled calibration workflow while the actual calibration authority remains inside the `WeighingUnit`/TLB adapter boundary.

## 1. Physical arrangement

```text
standard mass
    ↓
bag saddle / weighing mechanism
    ↓
load cell
    ↓
LAUMAS TLB485
    ↓ RS485
ESP32-S3 SP01
    ↓ Wi-Fi
web calibration UI
```

The browser does not write raw TLB registers. It calls a calibration service, which validates machine state and delegates supported operations to the TLB adapter.

## 2. User procedure

Current commissioning procedure:

```text
A. Empty bag saddle
   wait until stable
   SET ZERO

B. Place known 20.000 kg reference
   wait until stable
   CAPTURE / CHECK 20 kg

C. Place 50.000 kg standard weight
   wait until stable
   SET SPAN 50 kg

D. Remove all weight
   VERIFY ZERO

E. Replace 20 kg
   VERIFY 20 kg

F. Replace 50 kg
   VERIFY 50 kg

G. SAVE + LOCK CALIBRATION
```

If the 20 kg mass is itself a trusted/certified standard and the selected weighing transmitter explicitly supports a true multi-point calibration model that is justified for this machine, firmware may later support it as an additional calibration anchor. Otherwise the preferred default is **zero + 50 kg span**, with 20 kg used as an independent linearity/mechanical verification point.

Do not bend the calibration curve merely to hide saddle friction, binding, load-cell mounting error or other mechanical nonlinearity.

## 3. Required HMI

Calibration lives under a controlled service path, for example:

```text
SERVICE
  └── WEIGHING
       ├── Diagnostics
       ├── Zero
       ├── Calibration
       └── Verification
```

Minimum calibration screen:

```text
SP01 — WEIGHT CALIBRATION
SERVICE MODE

TLB status          ONLINE / FAULT
Current weight      xx.xxx kg
Raw value/count     available if supported
Filtered weight     xx.xxx kg
Stability           STABLE / MOVING
Signal quality      GOOD / STALE / FAULT

ZERO                captured / pending
20.000 kg CHECK     result / error
50.000 kg SPAN      captured / pending

[SET ZERO]
[CHECK 20 kg]
[SET 50 kg SPAN]
[VERIFY]
[SAVE CALIBRATION]
[CANCEL]
```

The screen must clearly identify `SP01`, the weighing device identity, firmware version and current calibration/version state.

## 4. Interlocks for calibration writes

Firmware accepts `SET ZERO`, `SET SPAN`, or calibration save only when all required conditions are true:

```text
machine/node in SERVICE or CALIBRATION mode
AND automatic filling disabled
AND all filling outputs in defined safe state
AND TLB communication healthy
AND weight sample fresh
AND measured value stable for the configured stability window
AND authenticated engineer/service role when authentication is enabled
```

Calibration must never begin or commit during `COARSE_FILL`, `FINE_FILL`, `CUTOFF`, `PUSH`, or another active production state.

A browser disconnect or Wi-Fi outage during the calibration wizard must not leave a half-applied ambiguous calibration. Calibration operations must be atomic at the controller/TLB boundary or recover to the previous known-good calibration.

## 5. Stability criterion

The UI must not rely on visual judgement alone. Firmware should expose an explicit stability result derived from the weighing unit or from a validated local rule such as bounded variation over a time window.

Exact numeric thresholds are bench measurements/configuration items and must be frozen before live commissioning.

The calibration button remains disabled while the weight is moving or stale.

## 6. ZERO, TARE and CALIBRATION are different operations

Do not merge these concepts in the UI:

```text
ZERO
  establish/adjust empty-saddle zero within an allowed range

TARE
  temporary operational tare if the final process design requires it

CALIBRATION
  establish/verify relationship between load-cell signal and engineering kg
```

A normal operator must not accidentally enter calibration when only a routine zero is intended.

## 7. Weighing adapter contract

The controller core does not know TLB register numbers.

Conceptual interface:

```text
WeighingUnit
  read_weight()
  read_status()
  read_raw()            optional if device supports it
  is_stable()
  zero()
  tare()
  begin_calibration()
  set_span(reference_kg)
  verify(reference_kg)
  save_calibration()
  cancel_calibration()
  health()
  diagnostics()
```

The exact operations exposed by the TLB adapter must follow the selected TLB model/manual. Unsupported commands must not be invented by the generic interface implementation.

## 8. Calibration record / audit

Every completed calibration creates an append-only record containing as much of the following as the hardware exposes:

```text
calibration_id
spout_id
weigher_model
weigher_serial/device_id
timestamp
operator/service identity
zero reading/raw value
20 kg verification reading/error
50 kg standard reference
50 kg reading/raw value
post-calibration zero verification
post-calibration 20 kg verification
post-calibration 50 kg verification
stability/noise metrics
firmware_version
controller_config_version
TLB configuration/calibration version if available
result = PASS / FAIL / CANCELLED
```

Do not silently overwrite the previous record. Preserve a last-known-good calibration reference and calibration history.

## 9. Verification criteria

Numeric acceptance tolerances are intentionally not invented here. They must be agreed from the required bag accuracy, scale behavior and metrology requirements, then encoded as explicit pass/fail thresholds.

At minimum the verification sequence must detect:

- failure to return near zero after unloading;
- excessive error at 20 kg;
- excessive error at 50 kg;
- unstable/noisy reading;
- stale TLB data;
- significant hysteresis between loading and unloading where tested;
- obvious nonlinearity indicating mechanical/load-cell problems.

## 10. Bench-first rule

The complete calibration UI and adapter workflow is first tested with the dummy/bench SP01 setup before any live actuator authority is granted.

Calibration is considered a required write feature in HMI v0.1 even if most other v0.1 HMI functions remain read-only.