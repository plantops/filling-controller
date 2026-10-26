# SP01 G7 telemetry-integrity correction — legacy DO semantics

Date: 2026-10-26
Branch: `diag/sp01-g2-g9`
Scope: edge collector read-only compatibility mapping; **software evidence only**.
Formal G7: PENDING. G8/G9: BLOCKED-HW. No actuator authority change.

## Source-grounded finding

The historical embedded HMI `firmware/esp32-s3/components/web_hmi/web_hmi.cpp` at commit
`92458f0d059ff046607ea96e9e6f05ac7319cf3c` emits legacy `"do"` from
`pack_outputs(s.controller.outputs)`. This is the **controller-desired** output
image, not `BoardIo::last_commanded_outputs()` or independent physical feedback.

The 2026-10-26 collector correction at `021451a8` incorrectly described legacy
`do` as the last commanded expander image, and mapped it to `commanded_do`.
The accompanying test at `3e497c16` asserted the same incorrect semantic.
That would mislabel intent as an executed board command in archived or older
runtime telemetry and could distort G8 shadow comparison.

## Correction

- `fceb4eb067731eaf80a582abaa7fceb0afacb72a`: map legacy `do` to
  `desired_do` only. Do not synthesize `commanded_do` or `physical_do`.
- `6a99f64974e50993bdc615eb53b83becd85e7f24`: update regression test
  to require desired-output compatibility and absent commanded/physical feedback.
  Canonical `desired_do` and `commanded_do` retain precedence.
- `2f8d3663ce91bbaaa388202c79e203a8f908505c`: align collector README
  with the actual canonical and legacy telemetry contracts.

Current embedded runtime emits separate canonical `desired_do` and
`commanded_do`, with `physical_do:null` when feedback is not measured.
No source change to controller FSM, board I/O, machine wiring, or output authority.

## Validation and limitations

- Isolated local Go compatibility harness: 2 tests PASS (legacy DO maps only to
  desired output; canonical fields retain precedence). This is a reproduction
  of the normalization logic, **not** the repository's full `go test ./...`.
- Full repository Go tests, `go vet`, and ESP-IDF build: **NOT VERIFIED** in
  this iteration. Direct GitHub clone unavailable in the local runtime
  (DNS resolution failure).
- At post-change inspection, GitHub Actions had no check runs for branch head
  `2f8d3663ce91bbaaa388202c79e203a8f908505c`. Last branch CI PASS was
  `bd7be51` on 2026-09-13. CI on old SHAs does not validate this change.

## Gate boundary

No gate advanced. The G2T installed thermal/safe-output/spare-swap evidence,
G4 real TLB transport/noise evidence, G5 zero/20/50 kg calibration, G6 runtime
network-loss proof, G7 review and R1 critical closure, and G8 physical position
reference plus legacy-versus-SP01 shadow timeline remain outstanding.

Machine actuator outputs remain physically isolated until G8 is formally
accepted and the documented G8-to-G9 transition is locally authorized.
