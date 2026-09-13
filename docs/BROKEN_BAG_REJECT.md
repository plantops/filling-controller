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

Production logic must not trip from one noisy point derivative. The commissioning branch implements a bounded loss detector using a running peak and persistence over new TLB samples. It is intentionally disabled by default until measured G4/G8 values are supplied.

Conceptually:

```text
fill_active = state in {COARSE_FILL, FINE_FILL}
weight_good = latest WeightSnapshot is fresh and quality==GOOD

loss = recent_peak_weight - current_weight

if fill_active && weight_good
   && loss >= broken_bag_loss_trip_kg
   && loss persists for broken_bag_persist_us across new samples:
       BROKEN_BAG_DETECTED
       latch disposition = REJECT for this spout_id + cycle_id
```

`broken_bag_loss_trip_kg`, `broken_bag_persist_us`, TLB filtering and the production noise envelope are commissioning values. G4/G8 must freeze them from real traces.

## 2. Immediate action — frozen machine requirement

When `BROKEN_BAG_DETECTED` is accepted during `COARSE_FILL` or `FINE_FILL`, filling is stopped immediately by the local controller.

The fill-energy outputs are removed in the same controller decision:

```text
DO4 dosing.valve_a = OFF
DO5 dosing.valve_b = OFF
DO6 dosing.valve_c = OFF
DO7 filling.motor  = OFF
DO8 spout.aeration = OFF
```

The REJECT disposition remains latched after those outputs are removed.

Current executable behavior keeps `scanner.down` and `bag_detect_air` active in `REJECT_WAIT` and keeps `bag.push` OFF until the semantic reject window is reached. This is a software working model to be checked against G8 machine evidence before live authority.

The immediate response is local process logic. HMI/network availability does not participate.

## 3. One cycle, two disposition paths

```text
GOOD    -> no push at 210° -> normal push near 355°
REJECT  -> immediate fill shutdown -> REJECT_WAIT -> push near 210° -> no push at 355°
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

A broken bag is a controlled reject disposition, not automatically a controller-wide `FAULT`.

## 4. Position references

The controller needs trustworthy local timing/reference methods for:

```text
reject eject window ~210°
normal eject window ~355°
```

The existing A/B discharge references support the current normal path. Their relation to the 210° reject window is not assumed.

For software conformance the controller receives a semantic:

```text
PositionSnapshot.reject_window
```

That semantic boundary allows G3 testing without inventing a ninth DI or a fake 210° sensor. G8 must determine how the installed machine produces this semantic timing window from real references/geometry.

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

## 6. Current executable status

The commissioning branch now implements the software model needed for G3:

```text
BagDisposition {UNDECIDED, GOOD, REJECT}
State::RejectWait
bounded broken-bag weight-loss detector
same-tick DO4..DO8 removal when REJECT is accepted
latched REJECT disposition
semantic PositionSnapshot.reject_window
DO3 push from REJECT_WAIT only at reject_window
normal A/B discharge path for GOOD bags
completed REJECT cycle does not re-enter the normal discharge path
```

The detector defaults disabled because production thresholds are not yet measured. `reject_wait_timeout_us` also remains a commissioning/configuration value. This is deliberate: software semantics are implemented, but G4/G8 still own the real signal/timing parameters.

## 7. Canonical-view requirements

The standard view pack is `SP01_CANONICAL_VIEWS.md`. Broken-bag behavior must appear consistently in:

```text
V1 Runtime Timeline
   weight loss -> detector decision -> DO4..DO8 OFF -> reject_due -> push@210 -> no push@355

V2 State/Process Matrix
   COARSE/FINE branch to REJECT_WAIT; fill energy removed immediately

V3 Interlock/Process Flow
   detector -> REJECT latch -> wait 210 -> push -> complete

V4 Interlock Predicates
   broken-bag predicate + REJECT_PUSH_ALLOWED vs NORMAL_PUSH_ALLOWED

V6 Executable Engine
   disposition + detector + RejectWait + PositionSnapshot

V8 Exception/Fault Matrix
   controlled REJECT distinct from controller/measurement faults

V11 Digital Twin HMI
   detector evidence, disposition, desired/physical DO and selected eject path

V12 Weighing Signal Quality
   owns noise/filter/persistence/threshold evidence

V13 Eight-Spout Topology
   every reject event bound to the correct spout_id + cycle_id
```

## 8. Gate requirements

### G3 — software/dry FSM

Required deterministic cases:

```text
normal increasing weight                       -> no false reject
single negative spike                          -> no reject
sustained qualified weight loss in fill        -> REJECT
negative delta outside fill                    -> no broken-bag decision
REJECT decision                                -> DO4..DO8 OFF in same tick
REJECT latched                                 -> no fill output reopens
REJECT                                         -> one push at simulated ~210°, none at ~355°
GOOD                                           -> no push at ~210°, one normal push at ~355°
MANUAL broken bag                              -> fill stops, no automatic bag push
reject-window timeout                          -> bounded fault-safe result
```

### G4 — TLB bench

Measure:

```text
sample/update rate
TLB filtering profile
latency and jitter
dynamic noise / vibration response
finite-window/high-water loss noise envelope
stale/fault/reconnect behavior
```

Freeze detector configuration only after this evidence and G8 machine traces agree.

### G7 — bench review

Canonical views and executable tests must agree on detector semantics, immediate shutdown, disposition latch and the two eject routes.

### G8 — shadow

With real DI + TLB weight and new DO physically isolated, compare against legacy:

```text
broken-bag detection timestamp
legacy fill-output shutdown timestamp
SP01 desired DO4..DO8 shutdown timestamp
reject disposition latch
210° reject timing/reference
355° normal timing/reference
wrong/duplicate push absence
```

Freeze detector thresholds, filters, angular windows and actuator lead from recorded evidence.

### G9 — live pilot

A locally authorized one-spout live pilot must demonstrate both normal and reject paths, including immediate fill shutdown for a detected broken bag, with rollback and a known-good spare available.

This document is the canonical broken-bag process contract until later measured evidence refines its numeric parameters.