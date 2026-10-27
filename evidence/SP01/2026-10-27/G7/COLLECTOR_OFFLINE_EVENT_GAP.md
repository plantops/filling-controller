# SP01 G7 — collector offline-gap event integrity

Date: 2026-10-27
Branch: `diag/sp01-g2-g9`
Scope: edge collector observation/event history only; software evidence, **not** physical gate evidence.

## Finding

`Store.Update` previously compared tracked controller values whenever a previous snapshot existed, including transitions from an online snapshot to an offline snapshot with `Data=nil`, and from that offline snapshot to the first reconnected observation.

This emitted false process events (for example `state: COARSE_FILL -> null -> REJECT_WAIT`) even though the collector had no machine observations during the network gap. G8 shadow analysis must not interpret collector unavailability as a measured controller transition.

## Focused correction

- `01f6e3c79353e15c89d0772f07196d5e899ecbe5`: compare process telemetry only when both consecutive snapshots are online. Continue recording explicit `online` state changes. Preserve normal process change detection across continuously online observations.
- `5f939386586ff0df4909a1f6715c798bb5ddae99`: add `TestStoreOfflineGapDoesNotInventProcessEvents` to assert two connectivity transitions, zero invented process events across the gap, and a genuine subsequent online state transition.

No firmware, controller FSM, TLB485 acquisition, board I/O, output suppression, or actuator authority was changed.

## Verification

- Read back both committed files and confirmed the intended guard and regression assertions are present.
- An **isolated, manually reproduced Go harness** covering the same online/offline transition rule passed `GO111MODULE=off go test -v` (one test). This is **not** the repository's full test suite and is not a SHA-specific CI result.
- Direct Git clone from the execution environment failed DNS resolution (`Could not resolve host: github.com`); repository `go test ./...`, `go vet ./...` and `gofmt` remain **NOT VERIFIED**.
- GitHub Actions check-runs for `5f939386586ff0df4909a1f6715c798bb5ddae99` were zero at inspection; the latest branch workflows still refer to `bd7be51` (2026-09-13). The `edge-web` workflow contains `edge/**` push/PR triggers, but the reason for absent runs cannot be verified. Do not mark this patch CI-accepted without a successful run for its SHA.

## Gate decision

Formal G7 remains PENDING. The collector correction does not close red-team R1-001 or supply G2T, G4, G5, G6, or G8 physical evidence. No formal gate advanced.

Machine actuator outputs must remain physically isolated until G8 acceptance and explicit local G8-to-G9 authorization. Field isolation cannot be verified from this software review.
