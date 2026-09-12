# SP01 rapid-prototype release readiness

Date: 2026-09-12

## Decision

The project owner/operator instructed the commissioning effort to stop expanding bench rigor and to move a working prototype to the field quickly without rewriting the controller core.

For the rapid-prototype scope, software/view/release readiness is accepted as complete. This is a release milestone only. It is not a formal G7 PASS and does not override `docs/GATE_EXECUTION_PLAN.md` gate preconditions.

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

Therefore the first machine visit is a non-authoritative prototype shadow/data-collection exercise rather than a formal G8 acceptance run or live-actuator trial.

## Gate interpretation

```text
rapid-prototype software/view/release milestone: PASS
formal G7 gate:                              PENDING
formal G8 gate:                              BLOCKED-HW
```

Formal G7 remains governed by `docs/GATE_EXECUTION_PLAN.md` and requires the documented prerequisite evidence and red-team closure. Formal G8 cannot PASS or be treated as entered under the strict gate chain until its documented preconditions are met.

Deferred rather than claimed:

```text
G2T thermal/serviceability qualification
G4 real TLB dynamic-weighing measurements
G5 formal calibration campaign
G6 network-loss qualification
formal G7 review completion
real 210/355 timing authority
```

## Next authority boundary

The frozen artifact may be used locally to collect real-DI/Ethernet/HMI shadow observations while machine actuator outputs remain isolated. Those observations may become evidence for later gates, but do not themselves advance G7 or G8 unless the documented exit criteria and preconditions are satisfied.

G8/G9 remain physical site gates and must not be marked complete without local evidence. G9 authority remains prohibited until formal G8 completion and the documented local G8-to-G9 authorization.
