# SP01 G7 R1 embedded-runtime update — 2026-09-11

Status: **software-path closure only — G7 NOT PASS; no physical gate advanced**

Reviewed implementation head: `4a4df04fb71f0f98908c9be146a396a3c03b7974`

## R1-004 — CLOSED as a software-path finding

The G2 bench DO endpoint now requires the configured non-empty service token before accepting `POST /api/bench/do`. The embedded bench UI sends the same `X-Service-Token` header used by calibration service calls.

The endpoint remains compile-time gated by `CONFIG_SP01_BENCH_DO_TEST_ENABLE`, which defaults OFF. This closure does not authorize machine connection or substitute for the documented G2 approved-dummy-load physical evidence.

## R1-006 — CLOSED at source instrumentation level

The runtime now keeps two distinct output images:

```text
ControllerSnapshot.outputs  = controller desired DO image
HmiSnapshot.commanded_outputs = semantic image reconstructed from the last successful TCA9554 write
```

The bench profile no longer overwrites the controller snapshot with its one-hot physical test image. `/api/state` now emits canonical telemetry keys:

```text
desired_do
commanded_do
```

The legacy `do` field remains as a compatibility alias for `commanded_do` during migration. `commanded_do` is explicitly the last successfully commanded expander image; it is not field-actuator feedback and must not be described as actual physical actuation.

`service_ready` now additionally requires the commanded output image to be all-off.

## R1-002 — PARTIALLY CLOSED, remains OPEN

The embedded `/api/state` contract now exposes additional G8/G7 evidence fields already owned by the executable:

```text
disposition
cycle_id
desired_do
commanded_do
broken_bag_detected_us
broken_bag_peak_kg
broken_bag_weight_kg
discharge_ref_interval_us
discharge_due_us
weight_sequence
weight_sample_time_us
```

R1-002 remains open because firmware/config identity, reset reason/uptime, reject-window/position-adapter state, and the final measured G8 timing fields are not yet present. Those missing fields must not be fabricated.

## CI evidence

Implementation head `4a4df04fb71f0f98908c9be146a396a3c03b7974`:

```text
edge-web workflow run 34584895408 PASS
fw workflow run       34584895426 PASS
  linux-amd64          PASS
  esp32-s3             PASS
```

CI is software evidence only.

## Gate impact

No gate status changes:

```text
G2  ACTIVE — approved dummy-load DO1..8 + reset/restart safe-output evidence still required
G2T BLOCKED-HW
G3  PASS
G4  BLOCKED-HW
G5  BLOCKED-HW
G6  PENDING physical/runtime evidence
G7  PENDING; R1-001 CRITICAL remains open and prerequisite physical evidence is incomplete
G8  BLOCKED-HW; outputs isolated
G9  BLOCKED-HW
```

Machine outputs remain isolated until G8 is complete and the documented G8-to-G9 transition is locally authorized.
