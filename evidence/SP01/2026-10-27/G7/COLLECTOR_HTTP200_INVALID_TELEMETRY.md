# SP01 G7 — invalid HTTP 200 telemetry must not create process events

Date: 2026-10-27
Branch: `diag/sp01-g2-g9`
Scope: read-only edge collector; software instrumentation integrity only.

## Finding

The collector accepted an HTTP 200 response with JSON `null`, `{}`, or an absent/non-string/empty `state` as `Online:true`. A valid JSON decode alone did not establish a usable controller observation. A previously valid snapshot followed by such a response could create synthetic process events such as `state: COARSE_FILL -> null`, misleading later G8 shadow analysis.

The embedded `/api/state` producer emits a named controller `state`; the collector uses that field for event tracking.

## Correction

- `d776870fa7a08c0d0446afbfb94fca221f6da6c3`: classify HTTP 200 responses with missing/invalid/blank `state` as `Online:false` and `invalid telemetry: missing state`. The existing consecutive-online event guard treats these as observation gaps.
- `1a09db408bb33d4ff55b6f6c36cd27d715552ec0`: add HTTP collector regression tests for `null`, `{}`, `state:null`, empty/whitespace state and numeric state. Assert offline/reconnect events, no invented process transitions, and recovery with valid telemetry.

No controller firmware, FSM, board I/O, TLB485, calibration, output suppression or actuator authority was changed.

## Validation and limits

- GitHub read-back confirmed both commits on the commissioning branch and the new test source.
- An isolated Go 1.23.2 HTTP/JSON/store harness reproducing the changed validation and observation-gap rule passed six cases with `GO111MODULE=off go test -v -count=1`. This is **not** a full-repository test or CI result.
- The repository declares Go 1.25. Local Git clone failed because `github.com` DNS resolution was unavailable. Repository `go test ./...`, `go vet ./...` and `gofmt` are **NOT VERIFIED**.
- At inspection, commit `1a09db408bb33d4ff55b6f6c36cd27d715552ec0` had zero check runs/statuses. The latest successful branch `edge-web` workflow was 2026-09-13 on `bd7be51`. The `edge/**` workflow trigger exists, but the missing CI execution cause is not established. No SHA-specific acceptance is claimed.

## Gate decision

Formal G7 remains PENDING. G2T, G4, G5, G6 and R1 red-team requirements remain open; this software correction does not establish any physical gate. No gate advanced.

Keep machine actuator outputs physically isolated until G8 acceptance and explicit locally authorized G8-to-G9 transition. Actual field isolation cannot be verified remotely.
