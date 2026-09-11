# SP01 G7 remote precheck — 2026-09-11

Status: **PRECHECK ONLY — G7 NOT PASS**

Branch: `diag/sp01-g2-g9`

Purpose: use remote time to reconcile canonical views with the executable without fabricating missing physical evidence.

## Evidence already available

```text
G0 PASS
G1 PASS
G2 1 h soak PASS
G2 DI1..DI8 PASS
G2 DO/reset-safe DEFERRED
G3 dry FSM PASS
```

G2T, G4, G5, G6 and G8/G9 physical evidence remain incomplete or blocked.

## Static reconciliation finding R1-001 — CRITICAL before G8

The shared controller accepts a `PositionSnapshot` and the canonical REJECT route requires `PositionSnapshot.reject_window` near 210 degrees before DO3 may push a rejected bag.

However, current production `firmware/esp32-s3/main/app_main.cpp` calls:

```cpp
snapshot = g_controller->tick(now, inputs, weight);
```

No `PositionSnapshot` is supplied by the production adapter. The default value therefore leaves `reject_window=false`.

Consequence:

```text
controller unit tests / virtual bench can exercise REJECT_WAIT -> PUSH
but current production app_main cannot provide the real 210-degree reject-window event
```

This is not a hardware failure and is not resolved by inventing a new DI. The as-built mechanism that derives the 210-degree window must be measured/frozen in G8 and connected through a position adapter before live authority.

Disposition: **OPEN CRITICAL FINDING**. G7 cannot PASS while this is unresolved.

## Expected commissioning-only disabled values

`make_controller_config()` currently does not populate `broken_bag_loss_trip_kg`, `broken_bag_persist_us`, or `reject_wait_timeout_us`. This is consistent with the policy that broken-bag production thresholds remain disabled/unfrozen until G4/G8 measurement evidence exists. Do not fill these from simulation constants.

## Remote work that can continue

The following can be reviewed or prepared without claiming gate completion:

```text
V1..V13 source/code consistency
read-only HMI/collector authority boundary
production position-adapter interface and telemetry fields
G8 measurement checklist for deriving 210/355 timing
red-team list of unresolved constants and ownership paths
```

## Gate status

```text
G7 = PENDING / PRECHECK STARTED
R1-001 = OPEN CRITICAL
```

Formal G7 PASS still requires all canonical views reconciled against the physical evidence owned by G2/G2T/G4/G5/G6 and no unresolved critical R1 finding.
