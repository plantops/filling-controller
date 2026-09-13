# SP01 G7 R1 position-reference update — 2026-09-11

Status: **operator-provided machine information only — pending physical verification; no gate advanced**

Branch: `diag/sp01-g2-g9`

## Legacy position-reference inventory reported by operator

The installed legacy packer is reported to have approximately these position references:

```text
~10 deg   reset/index reference
~20 deg   scanner reference
~210 deg  broken-bag reject/eject position reference
~350 deg  count-up reference
~355 deg  count-down / normal-discharge reference
```

Bag presence remains a separate input function (`bag.present`) and is not one of these rotor-position references.

This report identifies a plausible installed-machine source for the semantic reject window, but it is not yet electrical/timing evidence. Exact sensor identity, polarity, terminal/wire mapping, pulse width, repeatability and relation to rotor speed have not yet been measured in this commissioning record.

## R1-001 impact

R1-001 remains **OPEN / CRITICAL before G8 authority**, but its unresolved question is narrowed.

Previous question:

```text
What real installed signal or timing method supplies PositionSnapshot.reject_window near 210 deg?
```

Current question:

```text
Verify the installed ~210 deg legacy reference electrically and in time,
then decide how it is adapted into PositionSnapshot.reject_window.
```

Do not invent an extra DI assignment. The current 8DI contract is already allocated and must not be rewritten from the reported legacy reference list without an as-built wiring decision.

## Candidate commissioning strategy

For initial G8 shadow, retain the legacy reference set as ground truth where it can be observed without taking actuator authority. Timestamp-align at least the reset/index, reject (~210), count-up (~350) and count-down/normal-discharge (~355) references with the legacy outputs and SP01 desired behavior.

A later software phase estimator may be evaluated from measured period/jitter evidence, but no sensor may be retired from the production design solely from simulation or assumed fixed timing.

## Exact local evidence required

With SP01 actuator outputs still isolated:

```text
1. Identify the actual legacy sensor/wire/terminal for each available reference.
2. Record electrical polarity and idle/active levels.
3. Capture timestamped transitions for >= 20 consecutive revolutions at representative speed.
4. Record reset/index -> scanner -> reject -> count-up -> count-down ordering and intervals.
5. Record rotor-speed variation / revolution period over the same capture.
6. Confirm bag-present sensor separately from the rotor references.
7. Correlate the ~210 reference with the legacy reject push command and ~355 reference with normal push timing.
```

These measurements belong to G8 timing/reference freeze and do not substitute for G2/G2T/G4/G5/G6 prerequisites.

## Gate impact

```text
G2  ACTIVE
G2T BLOCKED-HW
G3  PASS
G4  BLOCKED-HW
G5  BLOCKED-HW
G6  PENDING
G7  PENDING; R1-001 remains open
G8  BLOCKED-HW; outputs isolated
G9  BLOCKED-HW
```
