# SP01 G3 dry-FSM software evidence — 2026-09-10

**Verdict: PASS for G3 software/dry-FSM scope.**

Branch: `diag/sp01-g2-g9`

This verdict does **not** advance G2, G2T, G4, G5, G8 or G9. It does not validate real machine angles, TLB noise thresholds or physical outputs.

## Evidence

The shared C++ controller now implements and tests the installed-process routing required for G3:

```text
normal AUTO full cycle
MANUAL fill-only cycle; no automatic bag push
fault/timeout matrix
safe output image on controller faults

broken bag while COARSE/FINE filling:
  qualified persistent weight loss
  -> latch REJECT
  -> remove DO4..DO8 in the same controller decision
  -> REJECT_WAIT
  -> semantic ~210 deg reject window
  -> DO3 bag.push once
  -> COMPLETE
  -> no later normal ~355 deg push

healthy bag:
  normal fill/cutoff/settle
  -> latch GOOD
  -> semantic reject window is ignored
  -> normal A/B discharge path
  -> DO3 bag.push once
```

Canonical executable elements:

```text
BagDisposition {UNDECIDED, GOOD, REJECT}
State::RejectWait
PositionSnapshot.reject_window
bounded high-water/persistence broken-bag detector
```

The detector counts persistence only across new WeightSnapshot sequence values; repeated controller ticks over one sample cannot manufacture broken-bag evidence.

## Automated test coverage

`firmware/host/tests/gate_g3_test.cpp` covers the original normal-cycle/fault matrix.

`firmware/host/tests/gate_reject_test.cpp` additionally covers:

```text
increasing weight -> no reject
single negative spike -> no reject
sustained qualified loss -> REJECT
same-tick DO4..DO8 shutdown
REJECT latch
~210 semantic window -> DO3 push
completed REJECT cycle -> no normal discharge path
GOOD path ignores ~210 semantic window
GOOD path uses normal A/B discharge
reject-window timeout -> fault + safe outputs
negative movement outside filling -> no broken-bag decision
MANUAL broken bag -> filling stops, no automatic push
```

## CI evidence

Firmware workflow run `34483469950` / run #172 passed both:

```text
linux-amd64  PASS
esp32-s3     PASS
```

This run includes the executable broken-bag reject controller and virtual-bench route through commit `ca279e6fd0f51e67a03f4601e033d30faead820d`.

A later branch head added the explicit GOOD-path reject-window assertion plus supervisory edge/web files only. On workflow run `34484293629` / run #181, `linux-amd64` passed the expanded shared-controller suite. The ESP32 job was still running when this evidence note was updated; no ESP32 controller/main source changed after the already-green `ca279e6f` firmware build except host-only tests/docs/edge-web files.

The dedicated `edge-web` workflow also passed collector formatting, Go tests/vet and the static 13-view console check on its first complete run.

## What G3 does not prove

G3 uses the semantic software boundary:

```text
PositionSnapshot.reject_window
```

It deliberately does not claim how the installed machine derives the real ~210 deg window. It also uses test-only broken-bag thresholds.

The following remain owned by later measured gates:

```text
G4: TLB sample rate/filter/noise/latency and detector noise envelope
G8: production broken-bag thresholds/persistence and real ~210/~355 timing references/lead
G9: physical one-spout acceptance
```

## Gate conclusion

The deterministic dry-FSM requirements are now executable and covered by automated evidence. G3 may be treated as `PASS` for its software/dry-FSM scope. Physical I/O truth remains G2, and real machine/timing truth remains G4/G8/G9.
