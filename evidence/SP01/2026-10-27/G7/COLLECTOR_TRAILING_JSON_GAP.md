# SP01 G7 — reject trailing HTTP JSON as an observation gap

Date: 2026-10-27
Branch: `diag/sp01-g2-g9`
Scope: read-only edge collector; software instrumentation evidence only.

## Finding

`edge/collector/main.go` decoded the first JSON value from HTTP 200 and accepted the snapshot without checking for trailing data. A response containing concatenated JSON values (e.g. an old `state` object followed by another object), a primitive, or garbage after the first object could be accepted as an online controller observation. Such data is unsuitable for reconstructing commissioning transitions.

## Change

- `a1124451ef9ea7c31a8e830d4bd2712033050084`: require the second decoder read to return `io.EOF`. Any second value or trailing invalid data is recorded as `Online:false`, error `invalid JSON: trailing data`, retaining the established offline/reconnect observation-gap behavior. Trailing whitespace remains valid.
- `ed66aeeb996d0c23da66edd4ac3584c39eb1910f`: add `TestPollOneRequiresSingleJSONValue` with concatenated objects, trailing garbage, trailing primitive, and whitespace fixtures. It checks recovery and absence of fabricated process events.
- `5d44162d27fe4b04102df67821dc69c9b66e4034`: correct the whitespace fixture to a raw Go string literal.

No controller FSM, firmware, board I/O, TLB485, output suppression, or actuator authority changed.

## Verification and limitations

- Read back the updated branch files and confirmed the EOF guard and four regression fixtures.
- Local Go 1.23.2 isolated decoder harness: `GO111MODULE=off go test -v -count=1` PASS (one table-driven test with four cases). This reproduces the JSON EOF rule; **it is not a repository test**, not a Go 1.25 test, and does not validate the complete HTTP collector.
- Repository Git clone failed DNS resolution for `github.com`. Full repository `go test ./...`, `go vet ./...`, `gofmt`, and SHA-specific CI acceptance are **NOT VERIFIED**.
- At inspection, HEAD `5d44162d27fe4b04102df67821dc69c9b66e4034` had zero check runs. The latest branch `edge-web`, `fw` and `fw-artifact` successes were 2026-09-13 on `bd7be51`. The cause of absent October workflow execution is not established.

## Formal gate decision

No formal gate advanced. G2 PASS-WAIVER; G2T BLOCKED-HW; G3 PASS; G4/G5 BLOCKED-HW; G6/G7 PENDING; G8/G9 BLOCKED-HW. R1-001 remains CRITICAL and unresolved.

G2T requires local measured installed/elevated temperatures under representative I/O/Ethernet/RS485 load, safe-output behavior on boot/reset/brownout/fault, spare-controller swap and approved configuration recovery, and TLB calibration persistence. G4 requires actual TLB485 measurements; G5 physical zero/20/50 kg calibration; G6 runtime network-loss independence; G8 measured 210-degree reference and aligned legacy/SP01 GOOD/REJECT shadow traces.

Machine actuator outputs must remain physically isolated until G8 acceptance and explicit local G8-to-G9 authorization. Remote review cannot verify field isolation.
