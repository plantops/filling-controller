# SP01 Core 11-View Reconciliation Review — 2026-09-12

Branch: `diag/sp01-g2-g9`

Scope: documentation/UI reconciliation of the eleven core engineering views V1..V11 against the current C++ controller, board map, embedded telemetry contract, edge collector and available G2/G3 evidence.

This review does **not** close G7. G7 still requires the physical evidence owned by G2T/G4/G5/G6 and the later red-team review.

## Result

```text
V1   REVIEWED / reconciled; full runtime event producer remains partial
V2   REVIEWED / reconciled with current controller output images
V3   REVIEWED / AUTO + MANUAL reject routes explicit
V4   REVIEWED / corrected from finite-window wording to high-water + persistence
V5   REVIEWED / local authority vs read-only supervisory boundary explicit
V6   REVIEWED / controller core separated from active G2 diagnostic artifact
V7   REVIEWED / DI PASS recorded; DO evidence remains ACTIVE
V8   REVIEWED / AUTO vs MANUAL reject behavior separated
V9   REVIEWED / PUSH classified by disposition
V10  REVIEWED / desired_do vs commanded_do vs physical_do semantics frozen
V11  REVIEWED / no pseudo-live values; core 11 + V12/V13 extension navigation
```

## Contradictions found and corrected

1. Earlier view text described the broken-bag detector as finite-window/delta logic. Current executable `controller.cpp` uses a running high-water mark, loss threshold and persistence counted only on new `WeightSnapshot.sequence` values. The canonical view now matches the code.
2. Earlier state tables said only “fill energy OFF” after cutoff/reject without stating that current executable logic retains DO1 scanner and DO2 bag-detect air through `CUTOFF`, `SETTLE`, `REJECT_WAIT` and `WAIT_DISCHARGE`. The exact output image is now recorded.
3. Earlier reject matrix implied every reject routes automatically to the ~210° eject. Current controller routes MANUAL broken-bag detection directly to `COMPLETE`; only AUTO uses `REJECT_WAIT` and automatic DO3 push.
4. Earlier supervisory projection classified all `PUSH` as discharge. V9 now uses disposition: REJECT push = `REJECTING`, GOOD push = `DISCHARGE`.
5. Earlier HMI text could blur `commanded_do` with physical output state. V10/V11 now freeze the terms: `desired_do` = FSM request, `commanded_do` = image sent to board adapter, `physical_do` = independent electrical feedback only if actually measured.
6. The old duplicate `docs/SP01_ENGINEERING_VIEWS.md` contained stale pre-reject-implementation statements. It is now an archive pointer to the canonical pack.
7. The project previously described all 13 views as one flat canonical set. V1..V11 are now the core engineering language; V12/V13 remain commissioning extensions.

## Current physical boundary

```text
G2 sustained USB/Ethernet soak   PASS
G2 DI1..DI8 physical truth      PASS
G2 DO physical/loopback truth   ACTIVE
G2 reset/restart safe outputs   PENDING
```

No physical gate was advanced by this documentation review.

## Updated sources

```text
docs/SP01_CANONICAL_VIEWS.md
docs/ENGINEERING_VIEW_INDEX.md
docs/SP01_ENGINEERING_VIEWS.md
web/engineering-console/index.html
web/engineering-console/README.md
docs/GATE_EXECUTION_PLAN.md
```

## G7 status

```text
VIEW RECONCILIATION SUBTASK: COMPLETE
G7 GATE: PENDING
```
