# SP01 Broken-Bag Reject Station — 210°

This document records a confirmed installed-machine fact supplied during design review:

```text
A dedicated broken-bag reject sensor exists at approximately 210° mechanical position.
```

This is now part of the physical machine model. The exact electrical implementation, signal polarity, terminal/cabinet location, pulse timing, and reject actuator path are still to be surveyed on the installed packer before firmware authority is assigned.

## 1. Architectural consequence

Broken-bag handling must not be designed as a purely inferred `dW/dt` feature. The machine already provides a dedicated physical detection/reject reference at the 210° station.

Therefore the future protection hierarchy is:

```text
primary field evidence      = dedicated 210° broken-bag reject sensor
secondary corroboration     = bag-present status / weight trajectory / cycle context
controller or machine action = only after the legacy wiring and actuator path are mapped
```

A weight-drop or low-flow estimator may later be useful as diagnostics or redundancy, but it must not replace the installed sensor without measured evidence and a separate design decision.

## 2. Do not invent an I/O channel

Current SP01 v0.1 already allocates all eight local DIs:

```text
DI1 hopper.feeder_running
DI2 downstream.conveyor_ready
DI3 machine.motor_running
DI4 process.initiative
DI5 cycle.fill_position
DI6 bag.present
DI7 position.discharge_ref_a
DI8 position.discharge_ref_b
```

No local DI is currently free for the 210° sensor.

Do **not** silently repurpose DI7/DI8 or another input. First determine whether the 210° signal is:

```text
- a per-spout signal carried on the rotating assembly;
- a fixed machine/stationary signal shared by all eight spouts;
- part of an existing legacy reject controller;
- available through another fieldbus/discrete interface;
- or already encoded through wiring not yet documented.
```

This is an explicit architecture gap to close in G8 shadow commissioning.

## 3. Detection versus rejection authority

Keep these concepts separate:

```text
BROKEN_BAG_SENSOR_210     physical observation
BROKEN_BAG_DETECTED       interpreted event
REJECT_REQUIRED           process decision
REJECT_ACTUATOR_COMMAND   physical authority
```

The presence of the sensor does not by itself prove which actuator performs rejection or which controller owns it.

Current 8DO SP01 allocation contains no dedicated reject output. Do not create an `OUT_REJECT` alias on an existing output until the as-built legacy mechanism is traced.

## 4. Timeline / digital-twin requirement

V1 and V11 should eventually show the 210° station explicitly once its reference is field-verified:

```text
cycle_id
spout_id
210° sensor edge / level
bag-present state
weight at event
controller state
legacy reject action
SP01 desired action (shadow only during G8)
```

The central eight-spout view must correlate the fixed 210° station with the spout currently passing it. A raw sensor pulse without `spout_id/cycle_id` correlation is insufficient evidence for per-spout fault history.

## 5. G8 survey checklist

Before integrating this signal into executable logic, capture:

```text
exact mechanical reference used for 210°
sensor manufacturer/model if available
sensor type and supply voltage
NO/NC or active-high/active-low behavior
pulse width / dwell time at operating speed
physical terminal and cabinet reference
whether the sensor rotates or is stationary
how the legacy controller identifies the affected spout
legacy response when the sensor trips
actual reject actuator and its electrical command path
behavior when sensor is stuck ON / stuck OFF / disconnected
behavior for an intact bag passing 210°
behavior for an intentionally simulated reject condition where plant procedure permits
```

Record timestamped evidence together with rotor/cycle identity.

## 6. Fault-model impact

Until G8 evidence is complete, do not add a hard-coded new production `Fault` enum solely from this document.

The design target is to distinguish at least:

```text
broken-bag detection event
sensor/interface fault
reject requested
reject completed / not confirmed, if feedback exists
```

Whether a detected broken bag should enter the local SP01 `FAULT` state, mark the current bag as reject-only, inhibit the normal discharge sequence, or remain under a separate machine-level reject function must be decided from the observed legacy behavior and mechanical reject path.

## 7. Gate impact

```text
G3   software can model a generic broken-bag event only as a non-authoritative test hook
G7   design review must include this station as a known machine feature
G8   mandatory: map 210° sensor + spout correlation + legacy reject behavior in shadow
G9   no new reject-output authority until the G8 mapping is accepted locally
```

This document is the canonical placeholder for the 210° station until the as-built electrical and mechanical evidence is frozen.