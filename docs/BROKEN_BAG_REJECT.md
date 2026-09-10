# SP01 Broken-Bag Reject Path — Detection + 210° Reject vs Normal 355°

This document records the installed-machine behavior supplied during design review.

## Confirmed process behavior

```text
while filling is active:
    if measured bag weight is falling because loss exceeds incoming fill
        -> classify current bag REJECT
        -> later push/eject this bag at approximately 210°

healthy/good bag
        -> keep the bag
        -> push/eject normally at approximately 355°
```

**Important:** 210° is the **early reject/eject position**, not a broken-bag sensor position.

The broken-bag detector is derived from the weighing trajectory while filling is active.

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

Therefore the confirmed broken-bag condition described by the installed-machine logic is:

```text
q_loss > q_in
    <=> measured net weight decreases while the filling command is active
    <=> dW/dt < 0 after suitable filtering / persistence checks
```

This does **not** require an extra broken-bag DI.

Production implementation must not trip from one noisy derivative sample. The preferred deterministic detector is a finite-window weight loss check:

```text
fill_active = state in {COARSE_FILL, FINE_FILL}
weight_good = latest WeightSnapshot is fresh and quality==GOOD

delta_w = filtered_weight(now) - filtered_weight(now - detection_window)

if fill_active && weight_good && delta_w < -loss_trip_kg
   for the required persistence/debounce interval:
       BROKEN_BAG_DETECTED
       latch disposition = REJECT for this spout_id + cycle_id
```

`detection_window`, `loss_trip_kg`, filtering and persistence values are **not frozen yet**. They must come from G4/G8 replay and machine evidence so normal vibration, flow pulsation and TLB filtering do not cause false rejects.

A point-sample derivative is diagnostic only; it is not the preferred production trip mechanism.

## 2. One cycle, two disposition paths

The controller must distinguish the disposition of the current bag:

```text
GOOD    -> normal discharge path -> push near 355°
REJECT  -> early reject path     -> push near 210°
```

A rejected bag must not later receive the normal 355° push. The REJECT decision is latched to the affected `spout_id + cycle_id` until the bag is ejected or the cycle is otherwise terminated.

Required invariants:

```text
GOOD -> never push at 210°
REJECT -> push at 210° and suppress later 355° push
one cycle has one final disposition
network/HMI never owns the detection or eject timing
```

## 3. Immediate action at detection

The confirmed information in this review defines how the bag is **detected** and where it is later **rejected**.

The exact immediate legacy output response at the instant of detection is still to be verified in G8. In particular, the project must trace whether legacy logic immediately removes dosing/motor/aeration commands or performs another bounded sequence before reaching the 210° reject window.

Until that is measured, do not invent additional output channels or timing constants.

## 4. Position references

The firmware needs a trustworthy way to know when the affected spout reaches:

```text
reject eject window ~210°
normal eject window ~355°
```

The current controller already has discharge references A/B used for the normal discharge timing path. Those references must not be assumed to provide the 210° window until the actual machine geometry is mapped.

For the 210° path, field commissioning must determine whether timing is derived from:

```text
an existing angular/reference signal
previous/current revolution timing
another legacy position signal
a machine-level position encoder/cam
or another verified source
```

No new DI is allocated merely from this requirement.

## 5. Output authority

Current SP01 output semantics include:

```text
DO3 = bag.push
```

Both reject and normal disposition use the same semantic push/eject action at different rotor positions:

```text
DO3 @ ~210° only when disposition == REJECT
DO3 @ ~355° only when disposition == GOOD
```

This requirement does not create a new `OUT_REJECT` output.

The as-built actuator/electrical path still has to be verified before production output authority is enabled.

## 6. Current executable gap

The current `sp01::Controller` implements the healthy-bag path:

```text
SETTLE -> WAIT_DISCHARGE -> PUSH -> COMPLETE
```

It does not yet implement:

```text
filtered negative-weight detection while filling
per-cycle GOOD/REJECT disposition latch
210° early reject scheduling
suppression of the later 355° push after reject
```

Therefore G3 remains software-active until these behaviors are represented and tested deterministically.

## 7. Timeline / digital-twin requirement

V1 and V11 should record:

```text
cycle_id
spout_id
fill_active
raw/filtered weight
weight delta over detection window
broken_bag_detected
reject_latched
detection timestamp
reject_due / reject_push_on / reject_push_off around 210°
normal_discharge_due / normal_push_on / normal_push_off around 355°
final disposition = REJECTED | NORMAL
```

This allows the detector threshold/filter to be tuned from real traces rather than guessed.

## 8. G3/G4/G8 test requirements

### G3 software

Use deterministic synthetic/replay traces:

```text
normal increasing weight -> GOOD
noisy but increasing weight -> GOOD
single negative spike -> must not reject
sustained negative finite-window delta while filling -> REJECT
negative delta outside filling states -> no broken-bag decision
REJECT latched -> 210° push only, no later 355° push
GOOD -> no 210° push, normal 355° push
```

### G4 weighing bench

Measure enough signal behavior to bound:

```text
sample/update rate
TLB filtering
zero and dynamic noise
latency/jitter
finite-window weight-delta noise
```

### G8 shadow

Capture real machine cycles including, where plant procedure permits, broken-bag/reject examples. Freeze:

```text
detection_window
loss_trip_kg
persistence/debounce
210° timing reference and actuator lead
355° timing reference and actuator lead
legacy immediate response at broken-bag detection
```

Angles remain approximate until measured commissioning values are frozen.

## 9. Gate impact

```text
G3   implement/test deterministic weight-loss detector and GOOD/REJECT routing
G4   characterize TLB dynamic signal/noise needed for detector thresholds
G7   review detector, disposition latch and two eject paths
G8   shadow-compare broken-bag detection + 210° reject and healthy ~355° discharge
G9   live pilot validates both paths under local commissioning procedure
```

This document is the canonical description of broken-bag detection and disposition until more precise as-built evidence is frozen.