# SP01 G7 — partial telemetry must not invent field transitions

Date: 2026-10-27
Branch: `diag/sp01-g2-g9`
Scope: read-only edge collector event instrumentation. Software evidence only.

## Finding

The existing offline-gap guard compared process keys only across consecutive online snapshots. However, an HTTP 200 snapshot with a valid `state` and absent or JSON-null *other tracked fields* was still online. The collector compared absent fields as `nil` and could record invented events such as `desired_do: 8 -> null` or `disposition: UNDECIDED -> null`. Subsequent restoration could create a second invented transition. Such events are not measurements of controller behavior and are unsuitable for G8 shadow analysis.

## Focused correction

- `8e86fb06959db916541ea1d6ac300c29a4d10e4e`: in `Store.Update`, compare each tracked key only if it exists and is non-null in both consecutive online snapshots. Remove the now-unused `trackedValue` helper. Numeric zero remains a valid observed value.
- `8c3c9ebd938e85f7b6bf3e2d6ff62560d913dcf0`: add `TestStorePartialTelemetryDoesNotInventProcessEvents` covering missing and null fields, genuine state changes, and subsequent observed transitions to zero for desired/commanded output images.

No firmware, controller FSM, TLB485, board I/O, output suppression or actuator authority was changed. This collector change cannot establish any physical gate.

## Verification

- GitHub read-back confirmed the updated guard and regression test on the branch. Compare against previous HEAD `e5fb7a46a6f211b9d1064fe4f8bf7eb0cd2af67c` shows exactly two commits affecting only `edge/collector/main.go` and `edge/collector/main_test.go`.
- A locally constructed isolated Go 1.23.2 harness reproducing the event-guard rule passed `GO111MODULE=off go test -v -count=1` (`TestPartialTelemetry`, PASS). This is **not** a full-repository or SHA-specific CI test.
- Repository clone failed DNS resolution for `github.com`; the repository declares Go 1.25. Repository `go test ./...`, `go vet ./...`, `gofmt` and ESP-IDF build are **NOT VERIFIED**.
- At inspection, commit `8c3c9ebd938e85f7b6bf3e2d6ff62560d913dcf0` had zero check runs. The latest branch workflow runs were successful on 2026-09-13 at `bd7be51`, not on the October collector commits. The `edge-web` workflow has `edge/**` push/PR triggers, but why expected runs are absent is unverified. No CI acceptance is claimed.

## Formal gate decision

G2 PASS-WAIVER; G2T BLOCKED-HW; G3 PASS; G4/G5 BLOCKED-HW; G6/G7 PENDING; G8/G9 BLOCKED-HW. No gate advanced. R1-001 remains unresolved; the 210-degree production position reference still requires local measurement and adapter/shadow validation.

Next documented site evidence: G2T measured installed/elevated temperature under representative I/O/Ethernet/RS485 activity, safe outputs through boot/reset/brownout/fault, spare swap and configuration recovery, and TLB calibration persistence. G4 actual TLB485 latency/noise/stale behavior, G5 physical zero/20/50 kg calibration, G6 runtime network-loss independence, and G8 real DI/TLB and aligned legacy-versus-SP01 GOOD/REJECT traces remain outstanding.

Machine actuator outputs must stay physically isolated until formal G8 acceptance and explicit local authorization for G8-to-G9. Field isolation cannot be verified remotely.
