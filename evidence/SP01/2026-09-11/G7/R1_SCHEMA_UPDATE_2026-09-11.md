# SP01 G7 R1 telemetry-schema update — 2026-09-11

Status: **software-path closure only — G7 NOT PASS; no physical gate advanced**

Reviewed implementation head: `92458f0d059ff046607ea96e9e6f05ac7319cf3c`

## R1-005 — CLOSED for collector schema ownership

The collector now owns telemetry schema version `1` and tracks canonical event keys:

```text
mode
state
fault
disposition
cycle_id
di
desired_do
commanded_do
broken_bag_detected_us
```

During embedded `/api/state` migration, legacy aliases are accepted only at the collector ingress:

```text
cycle              -> cycle_id
do                 -> desired_do + commanded_do
broken_detected_us -> broken_bag_detected_us
```

Canonical values take precedence when both canonical and legacy keys are present. Legacy aliases are removed from the normalized collector snapshot, so downstream consumers see one schema. Tests cover alias normalization, canonical precedence, and event generation for state/disposition/desired DO/commanded DO/broken-bag detection.

CI evidence for head `92458f0d059ff046607ea96e9e6f05ac7319cf3c`:

```text
edge-web workflow run 34575271490 PASS
- canonical-web PASS
- collector format PASS
- collector test PASS
- collector vet PASS
```

This closes the collector-side R1-005 naming/schema gap. It does **not** close R1-002 or R1-006 because the embedded runtime still must emit the richer canonical telemetry and distinguish desired versus commanded DO at source.

No gate status changes. Physical blocking chain remains G2 approved dummy-load DO1..8 + reset/restart safe-output evidence.