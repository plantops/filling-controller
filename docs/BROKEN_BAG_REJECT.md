# SP01 Broken-Bag Reject Path — 210° vs Normal 355°

This document records the corrected installed-machine behavior supplied during design review.

## Confirmed process behavior

```text
broken bag detected -> eject/push the affected bag at approximately 210°
healthy/good bag     -> keep the bag and eject/push normally at approximately 355°
```

**Important correction:** 210° is the **early reject/eject position**, not a dedicated broken-bag sensor position. The previous interpretation of a "210° broken-bag sensor" was wrong and is superseded by this document.

The mechanism that determines that a bag is broken is a separate concern and still has to be mapped from the installed machine/legacy logic before production firmware authority is assigned.

## 1. Architectural consequence — one cycle, two disposition paths

The controller must eventually distinguish the disposition of the current bag:

```text
GOOD    -> normal discharge path -> push near 355°
REJECT  -> early reject path     -> push near 210°
```

A rejected bag must not later receive the normal 355° push as though it were a good bag. The reject decision therefore needs to be latched to the affected `spout_id + cycle_id` until that bag is physically ejected or the cycle is otherwise terminated.

This is a routing/timing requirement, not evidence for an additional physical output channel.

## 2. Detection is separate from the 210° position

Do not infer a sensor at 210°.

The following remain to be established from as-built evidence:

```text
what signal or logic classifies the current bag as broken
when in the cycle that classification can occur
whether detection is local to the spout or machine-level
how the affected spout/cycle is identified
what electrical interface carries the broken-bag indication
what action the legacy controller takes immediately at detection before 210°
```

Possible weight/bag-present diagnostics may be useful later, but they are not automatically the primary detector and must not be promoted to production authority without measured evidence.

## 3. Position references are also separate

The firmware needs a trustworthy way to know when the affected spout reaches the two physical eject windows:

```text
reject eject window ~210°
normal eject window ~355°
```

The current controller already has discharge references A/B used for the normal discharge timing path. Those references must not be assumed to provide a valid 210° reference in the same revolution until the actual mechanical geometry is mapped.

For the 210° path, field commissioning must determine whether timing is derived from:

```text
an existing angular/reference signal
previous/current revolution timing
another legacy position signal
a machine-level position encoder/cam
or another verified source
```

No new DI is allocated merely from this requirement.

## 4. Output authority

Current SP01 output semantics include:

```text
DO3 = bag.push
```

The corrected process description says both reject and normal disposition are performed by a **push/eject action at different rotor angles**. Therefore this requirement does not by itself justify inventing an `OUT_REJECT` output.

Canonical intent:

```text
same semantic action: bag.push
routing difference:   210° reject window vs 355° normal window
```

The as-built actuator/electrical path still has to be verified before production output authority is enabled.

## 5. Current executable gap

The current `sp01::Controller` implements one normal discharge path:

```text
SETTLE -> WAIT_DISCHARGE -> PUSH -> COMPLETE
```

It does **not yet** implement a separately latched broken-bag disposition with an early 210° push. Therefore G3 remains software-active until this behavior is represented and tested; existing normal-cycle tests cannot be treated as complete coverage of the installed machine behavior.

Do not add an implementation until the broken-bag detection contract and the 210° position-reference contract are frozen enough to test deterministically.

## 6. Conceptual state/routing model

This is a design requirement, not yet executable state names:

```text
                         +-> GOOD   -> wait normal eject window ~355° -> bag.push
filled/current bag ------|
                         +-> REJECT -> wait reject eject window ~210° -> bag.push
```

Required invariants once implemented:

```text
one bag has one disposition per cycle
REJECT is latched to the affected cycle
REJECT suppresses the later normal 355° push for that cycle
GOOD does not trigger the 210° reject push
reset/fault leaves physical outputs in the safe image
network/HMI is not the timing authority
```

## 7. Timeline / digital-twin requirement

V1 and V11 should distinguish classification from ejection position:

```text
cycle_id
spout_id
broken_bag_detected / reject_latched
detection timestamp and source
weight + bag-present context
reject_due / reject_push_on / reject_push_off around 210°
normal_discharge_due / normal_push_on / normal_push_off around 355°
final disposition = REJECTED | NORMAL
```

This makes it possible to compare the new controller with the legacy machine without pretending that the 210° position itself detects the broken bag.

## 8. G8 survey checklist

Before production integration, capture:

```text
broken-bag detection source and electrical path
signal polarity / pulse or level semantics
which controller/logic currently owns the detection
how detection is associated with spout_id and cycle_id
actual rotor reference used to schedule 210°
actual rotor reference used to schedule ~355° normal push
measured angular/timing windows and actuator lead
whether the same physical pusher/solenoid is used for both actions
legacy behavior immediately after broken-bag detection
legacy behavior at 210° reject
legacy behavior for a healthy bag at ~355°
behavior if detection arrives too late for the 210° window
behavior if the position reference is missing/invalid
```

Angles are approximate machine references until field measurements freeze the actual timing/lead values.

## 9. Gate impact

```text
G3   add deterministic GOOD-vs-REJECT routing tests once detection/position contracts are defined
     REJECT path must push at the simulated 210° window and suppress normal ~355° push
     GOOD path must skip 210° and push only at the normal ~355° window
G7   design review must include the two-disposition model
G8   mandatory shadow comparison of broken-bag reject at 210° and healthy-bag normal push at ~355°
G9   live pilot must validate both paths under local commissioning procedure before full acceptance
```

This document is the canonical description of the broken-bag disposition behavior until more precise as-built evidence is frozen.