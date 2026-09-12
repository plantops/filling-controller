# SP01 G7 rapid-prototype release readiness

Date: 2026-09-12

## Decision

The project owner/operator instructed the commissioning effort to stop expanding bench rigor and to move a working prototype to the field quickly without rewriting the controller core.

For the rapid-prototype scope, software/view/release readiness is accepted as complete. This does not convert deferred physical checks into fabricated PASS results.

## Frozen field image

```text
branch: field/sp01-prototype-v0
commit: 24a0443580ed523049e31e140c31bd06076f1f99
workflow run: 34694506046
artifact: sp01-field-prototype
artifact id: 10297969116
sha256: 161463d78ff4cac8123a1c856338947b972886708f2c24683cbc4fa77b1c477e
```

CI result for the frozen commit:

```text
linux-amd64: PASS
esp32-s3:   PASS
```

## Frozen behavior

The field artifact runs the production controller path with:

```text
real DI1..DI8
Ethernet/HMI
synthetic/dummy weight
TLB485 disabled
normal process outputs suppressed before the board write
```

Therefore the first machine visit is a shadow exercise rather than a live-actuator trial.

## Prototype G7 interpretation

```text
G7 rapid-prototype software/view/release readiness: PASS
full formal G7 physical evidence package: DEFERRED
```

Deferred rather than claimed:

```text
G2T thermal/serviceability qualification
G4 real TLB dynamic-weighing measurements
G5 formal calibration campaign
G6 long network-loss qualification
real 210/355 timing authority
```

These items can be reopened individually if field behavior requires them.

## Next authority boundary

The frozen shadow artifact is ready for G8 rapid field shadow with real DI and Ethernet/HMI. It is not a G9 live-authority image because dummy-weight mode suppresses normal process outputs.

G8/G9 remain physical site decisions and must not be marked complete without local evidence.
