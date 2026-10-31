# SP01 G7 — collector position/source event instrumentation (software-only)

Date: 2026-10-27
Branch: `diag/sp01-g2-g9`
Scope: read-only edge collector; no controller, firmware, board I/O, or machine actuator authority changes.

## Finding

`edge/collector/main.go` retained changes to `state`, `desired_do`, `commanded_do` and other process fields, but its event ring did not track `reject_window`, `position_source`, `dev_source`, `authority`, or independently supplied `physical_do`. This omitted the origin of a 210-degree reject-window observation and transitions between real and simulated sources. Such gaps matter when reviewing a G8 shadow timeline. The current embedded field API explicitly reports `reject_window:null`, `position_source:"UNAVAILABLE"`, and `physical_do:null`; none of those is physical proof.

## Change

- `5c86fca49948e8a0980a8b57ac8223e72c875985`: include the five optional canonical fields in tracked event keys. The existing per-field missing/null observation-gap rule remains in force.
- `25708fea858655622ffc11f8c0f97b9bba10413e`: add `TestStoreTracksRejectWindowAndSourceChanges`. Fixture covers a previously unavailable position source, false→true→false reject-window transitions, independently populated zero→one physical feedback, and HW_TEST→FULL_SW source/authority changes. It asserts no fabricated null→value or value→null physical feedback transition.
- `13bd9683665ac56eab0c641493d9c04af97db746`: update collector README and state that poll-time events are not a synchronized high-resolution machine trace.

These are additive event observations, not a change in control authority or a claim that a physical sensor exists.

## Validation

- Source and test changes committed to the commissioning branch.
- Local Go 1.23.2 **isolated Store.Update harness**: `GO111MODULE=off go test -v -count=1` PASS (one regression test); `GO111MODULE=off go vet .` PASS. The harness reproduces the Store.Update comparison algorithm and the new fixture; it is **not** the repository, Go 1.25, or ESP-IDF build.
- Repository-wide `go test ./...`, `go vet ./...`, `gofmt`, and SHA-specific GitHub Actions acceptance are **NOT VERIFIED**. Direct Git clone cannot resolve `github.com` in the execution environment.
- After README commit `13bd968`, GitHub reports **zero check runs** and **zero associated PR workflow runs** for that SHA. The branch's latest visible `edge-web`, `fw`, and `fw-artifact` successes remain 2026-09-13 at `bd7be51`. The cause of missing October Actions execution remains unknown; the connected GitHub tool cannot dispatch workflows.

## Formal gate decision

**No gate advances.** G2 PASS-WAIVER; G2T BLOCKED-HW; G3 PASS; G4/G5 BLOCKED-HW; G6/G7 PENDING; G8/G9 BLOCKED-HW. R1-001 remains CRITICAL: the installed 210-degree reference and timing must be measured, a production position adapter must be validated, and actual legacy-versus-SP01 GOOD/REJECT shadow traces must be captured before formal G8 acceptance.

Next physical gate G2T still requires measured installed/elevated temperature with representative I/O/Ethernet/RS485 load; safe-output tests at boot/reset/brownout/fault; preflashed spare replacement and approved configuration restore; and TLB calibration persistence. G4 requires real TLB transport/noise/latency evidence, G5 real zero/20/50 kg calibration, G6 runtime network-loss proof.

**Machine actuator outputs remain physically isolated** until G8 is accepted and the documented G8-to-G9 transition is explicitly authorized locally. Remote review cannot establish physical isolation.
