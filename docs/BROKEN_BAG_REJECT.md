# SP01 Broken-Bag Reject Path — Detection + 210° Reject vs Normal 355°

This document records the installed-machine behavior supplied during design review.

## Confirmed process behavior

```text
while filling is active:
    if measured bag weight falls because loss exceeds incoming fill
        -> BROKEN_BAG_DETECTED
        -> latch current bag disposition = REJECT
        -> immediately stop filling outputs
        -> carry the rejected bag to ~210°
        -> push/eject once at ~210°
        -> suppress the normal ~355° push

healthy/good bag:
        -> complete normal filling
        -> carry bag past ~210° without push
        -> push/eject once at ~355°
```

**210° is the early reject/eject position, not a broken-bag sensor position.**

Broken-bag detection is derived from the TLB485 weight trajectory while filling is active. It does not require an extra broken-bag DI.

## 1. Detection semantics

Let:

```text
q_in   = cement mass-flow entering the bag
q_loss = cement mass-flow escaping from a broken bag
W      = measured bag weight from TLB485
```

During active filling:

```text
dW/dt = q_in - q_loss
```

The installed-machine condition is therefore:

```text
q_loss > q_in
    <=> measured net weight decreases while filling is commanded
```

Production logic must not trip from one noisy point derivative. Use a bounded finite-window detector over validated weight data:

```text
fill_active = state in {COARSE_FILL, FINE_FILL}
weight_good = latest WeightSnapshot is fresh and quality==GOOD

delta_w = filtered_weight(now) - filtered_weight(now - detection_window)

if fill_active && weight_good
   && delta_w < -loss_trip_kg
   && condition persists for configured debounce/persistence:
       BROKEN_BAG_DETECTED
       latch disposition = REJECT for this spout_id + cycle_id
```

`detection_window`, `loss_trip_kg`, filtering and persistence are commissioning values. G4/G8 must bound them from real TLB and machine traces.

## 2. Immediate action — frozen machine requirement

When `BROKEN_BAG_DETECTED` is accepted during `COARSE_FILL` or `FINE_FILL`, filling must be stopped immediately by the local controller.

The fill-energy outputs that were active for filling must be removed in the same control decision:

```text
DO4 dosing.valve_a = OFF
DO5 dosing.valve_b = OFF
DO6 dosing.valve_c = OFF
DO7 filling.motor  = OFF
DO8 spout.aeration = OFF
```

The REJECT disposition remains latched after those outputs are removed.

Do not automatically infer additional changes to `scanner.down` or `bag_detect_air` from this requirement; their post-detection behavior must follow the verified machine sequence. `bag.push` must remain OFF until the reject eject window around 210°.

The immediate response is local process logic. HMI/network availability must not participate.

## 3. One cycle, two disposition paths

```text
GOOD    -> no push at 210° -> normal push near 355°
REJECT  -> immediate fill shutdown -> push near 210° -> no push at 355°
```

Required invariants:

```text
one spout/cycle has one latched disposition
REJECT cannot return to GOOD in the same cycle
REJECT removes DO4..DO8 immediately
REJECT never reopens fill outputs before ejection
REJECT produces one push opportunity at ~210°
REJECT suppresses the later ~355° push
GOOD never produces the ~210° push
network/HMI never owns detection, fill shutdown or eject timing
```

A broken bag is a controlled reject disposition, not automatically a controller-wide `FAULT`. Separate true controller/measurement faults remain in V8.

## 4. Position references

The controller needs a trustworthy local timing/reference method for:

```text
reject eject window ~210°
normal eject window ~355°
```

The current A/B discharge references support the existing normal path, but their relation to the 210° window must be frozen from machine geometry/timing evidence before production authority.

No new DI is allocated merely because the reject path exists.

## 5. Output authority

Current SP01 semantics use the same physical action for both ejections:

```text
DO3 = bag.push
```

Therefore:

```text
DO3 @ ~210° only when disposition == REJECT
DO3 @ ~355° only when disposition == GOOD
```

No extra `OUT_REJECT` channel is required by the known process behavior.

## 6. Current executable gap

The current `sp01::Controller` still implements only the healthy-bag normal discharge path. G3 is not complete until executable logic and tests cover:

```text
finite-window negative-weight detection
latched GOOD/REJECT disposition
immediate DO4..DO8 shutdown on REJECT
no fill-output reactivation after reject
210° early reject scheduling
one reject push
suppression of the later 355° push
```

## 7. Canonical-view requirements

The following standard views must show the same behavior:

```text
V1 Runtime Timeline
   weight trace -> detector decision -> DO4..DO8 OFF -> reject_due -> push@210 -> no push@355

V2 State Logic Matrix
   COARSE/FINE may branch to REJECT disposition; fill outputs removed immediately

V3 Interlock Flow
   broken-bag detector is a process branch, not a fake 210° sensor

V4 Interlock Equations
   BROKEN_BAG = fill_active && weight_good && bounded_negative_delta

V8 Exception/Fault Matrix
   controlled REJECT is distinct from WeightFault/WeightStale/IoFault

V11 Digital Twin HMI
   show raw/filtered weight, reject latch, immediate fill shutdown and selected eject window

V12 Weighing Signal Quality
   owns filter/window/noise evidence and threshold derivation

V13 Eight-Spout Topology
   reject state is associated with the correct spout_id + cycle_id while rotating
```

## 8. Gate requirements

### G3 — software/dry FSM

Required deterministic cases:

```text
normal increasing weight                       -> GOOD
noisy but net increasing weight                -> GOOD
single negative spike                          -> no reject
sustained negative finite-window delta in fill -> REJECT
negative delta outside fill                    -> no broken-bag decision
REJECT decision                                -> DO4..DO8 OFF immediately
REJECT latched                                 -> no fill output reopens
REJECT                                         -> one push at simulated ~210°, none at ~355°
GOOD                                           -> no push at ~210°, one normal push at ~355°
```

For software evidence, record detector decision time and output-image transition time. The acceptable physical/timing limit is frozen later from measured machine evidence rather than invented here.

### G4 — TLB bench

Measure the signal characteristics needed to make the detector credible:

```text
sample/update rate
TLB filtering profile
latency and jitter
dynamic noise / vibration response
finite-window negative-delta noise envelope
stale/fault/reconnect behavior
```

### G7 — bench review

The view pack and executable tests must agree on the detector, immediate shutdown, disposition latch and two eject paths.

### G8 — shadow

With real DI + TLB weight and new DO physically isolated, compare against legacy:

```text
broken-bag detection timestamp
legacy fill-output shutdown timestamp
SP01 desired DO4..DO8 shutdown timestamp
reject disposition latch
210° reject timing
355° normal timing
wrong/duplicate push absence
```

Freeze `detection_window`, `loss_trip_kg`, persistence, filters, angular windows and actuator lead from recorded evidence.

### G9 — live pilot

A locally authorized live pilot must demonstrate both normal and reject paths, including immediate filling shutdown for a detected broken bag, with rollback and known-good spare available.

This document is the canonical broken-bag process contract until later measured evidence refines its numeric parameters.