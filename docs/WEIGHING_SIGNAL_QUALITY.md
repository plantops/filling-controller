# SP01 View 12 — Weighing Signal Quality, Calibration and Tare

This view was added after Purple-Team design review. It closes a gap between the controller state view and the real mechanical/weighing behavior of a rotary cement packer.

## 1. Canonical production boundary

```text
load cell bridge
   -> LAUMAS TLB485
   -> isolated RS485
   -> asynchronous Tlb485 task
   -> validated WeightSnapshot
   -> sp01::Controller
```

The ESP32 production controller does **not** read raw mV/V and does not own a HX711/raw-ADC calibration path.

Therefore DSP/filtering is treated as part of the weighing chain and acquisition contract, not as decorative HMI logic.

## 2. Three different concepts — never mix them

### A. Calibration zero/span

Calibration changes the weighing instrument relationship between physical load and indicated weight.

```text
ZERO / SPAN
```

It belongs to the TLB485/load-cell chain and is a guarded service action. It is not a per-bag operation.

See `CALIBRATION.md`.

### B. Cycle tare / pre-fill baseline

A cycle tare is a temporary process baseline used to express bag fill weight relative to the pre-fill mechanical condition.

**Current v0.1 does not issue a TLB zero/tare command for every bag.** The controller state name `TARE_READY` is currently a validation point: fresh/good weight must be available before filling. It must not be interpreted as permission to rewrite TLB calibration every cycle.

If a software cycle-tare offset is introduced later, it shall be:

```text
runtime-only
bounded
observable
reset at a defined lifecycle boundary
independent from TLB calibration zero/span
```

### C. Recipe target compensation

50.0 / 50.1 / 50.2 / 50.3 kg recipes compensate production behavior without changing calibration.

Calibration, cycle tare and recipe compensation are separate controls.

## 3. Tare-drift / buildup protection

Purple Team correctly identified cement buildup on the spout/saddle as a real plant blind spot.

Do **not** silently absorb unlimited pre-fill offset.

Before any future automatic cycle tare becomes production-authoritative, define and validate:

```text
pre_fill_baseline_kg
baseline_delta_from_clean_reference
max_abs_cycle_tare_kg
baseline_drift_rate
consecutive_out_of_range_cycles
```

Required behavior when the permitted baseline window is exceeded:

```text
no silent tare correction
raise explicit maintenance/cleaning condition
inhibit or fault filling if the validated safety case requires it
retain the measured baseline in evidence
```

No numeric limit such as +/-0.5 kg is frozen yet. The limit shall come from G4/G8 machine measurements, weighing uncertainty and maintenance practice.

## 4. DSP / filter contract

The rotary packer has mechanical vibration and bag motion, so weight quality cannot be reduced to one unqualified number.

The G4/G8 evidence set shall record the actual TLB/filter configuration together with transport behavior:

```text
TLB model / firmware
sample/update rate
configured digital filter / averaging parameters where available
stable-bit behavior
zero noise / peak-to-peak noise
step response / settling time
RS485 update latency and jitter
stale/error/recovery behavior
```

The controller consumes `WeightSnapshot { net_kg, sample_time_us, stable, quality, sequence }` and does not block on Modbus.

Filtering must not create so much phase lag that cutoff accuracy degrades. Filter tuning is therefore a measured trade-off between noise rejection and response delay.

## 5. dW/dt and adaptive in-flight rule

Current v0.1 cutoff is **not dependent on dW/dt**. It uses the configured `cutoff_margin_kg` in the deterministic C++ FSM.

A future flow-rate/in-flight estimator may be useful, but it must not be introduced as an unbounded derivative of noisy weight.

If implemented later, the estimator must have all of these properties:

```text
input comes from validated/filtered weight samples
sample timing is explicit
spike/outlier rejection is defined
quality/confidence is reported
estimate is bounded
loss of estimator quality falls back to the validated static cutoff margin
control remains deterministic
```

The static margin is the fallback authority until an adaptive method has separate bench and shadow evidence.

## 6. Bag rupture / sudden mass-loss protection

Existing protection already includes physical `bag.present` loss -> `BAG_LOST` fault during bag-required states.

That does not cover every rupture mode: a damaged bag may remain detected while net mass falls or fails to rise normally.

Add a candidate **weight-flow anomaly** protection layer for validation, based on measured data rather than guessed constants. Candidate evidence signals:

```text
negative weight step while filling
sustained near-zero weight gain while filling outputs are active
implausible positive/negative flow estimate
bag.present state at the same timestamp
current FSM state and active DO image
```

Possible future classifications:

```text
WEIGHT_DROP / POSSIBLE_BAG_RUPTURE
FLOW_LOSS / POSSIBLE_BLOCKAGE
```

These are **not current `Fault` enum values** and must not be shown as implemented faults until detection thresholds, debounce/time windows and safe response are tested in G3/G4/G8.

If promoted to a controller fault, the expected process result is the standard safe output image unless a different response is explicitly reviewed and validated.

## 7. HMI requirements from this view

The HMI/digital twin should expose measured signal quality, not only the weight value:

```text
net weight
age of latest sample
stable / quality
sample/update rate
TLB communication errors
filter profile identifier
pre-fill baseline / tare offset if implemented
static cutoff margin
adaptive-estimator value + confidence only if actually implemented
```

Do not display dW/dt, inflight correction, tare drift or DSP status as live if firmware/TLB does not provide the underlying evidence.

## 8. Gate mapping

```text
G3  prove deterministic static-margin behavior and candidate anomaly logic in simulation only
G4  measure TLB update/filter/noise/latency/stale/recovery behavior
G5  prove zero/span calibration independently from cycle tare and recipe offsets
G7  review signal-quality limits and fault semantics
G8  measure real rotating vibration, baseline drift, rupture/flow signatures and cutoff behavior
G9  only validated protections and thresholds gain live authority
```

This view is canonical for weighing signal quality and cycle-tare semantics.