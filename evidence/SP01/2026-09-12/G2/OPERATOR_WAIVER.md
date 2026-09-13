# SP01 G2 closeout — operator acceptance / reduced-rigor waiver

Date: 2026-09-12
Branch: `diag/sp01-g2-g9`

## Decision

The project owner/operator explicitly accepted the current G2 bench evidence as sufficient and instructed the team to stop further exhaustive DO loopback testing.

This is recorded as a **scope waiver**, not as fabricated per-channel proof.

## Evidence already available

- 1 h USB/Ethernet soak: PASS.
- DI1..DI8 dry-contact mapping: PASS.
- TCA9554 output control path exercised on the physical board.
- During the attempted DO->DI loopback run, several physical DI state changes were observed while DO sequencing was active, confirming that the output stage was switching electrically.
- The first loopback procedure was mismatched to the operator's one-jumper method, so its channel-by-channel PASS/FAIL lines are not treated as authoritative hardware verdicts.

Representative capture from the operator:

```text
BASELINE: DIraw=0x7f
DO2 OFF: DIraw=0xff
DO4 ON:  DIraw=0xbf
DO7 ON:  DIraw=0xdf
DO8 OFF: DIraw=0xff
```

These values show real input-state movement during the physical output exercise, but they do not prove a complete one-to-one DO1..DO8 mapping because the movable jumper was being repositioned while the automatic sequence was running.

## Accepted gate interpretation

For this prototype program, G2 is accepted for progression with the following explicit limitation:

```text
G2: PASS WITH OPERATOR WAIVER
```

Waived at G2:

- exhaustive one-by-one DO1..DO8 loopback proof;
- separate reset-safe timing capture dedicated only to G2.

Not waived:

- production actuator wiring remains disconnected until the later shadow/live gates;
- G8/G9 must still verify actual machine-channel mapping, safe behavior, and commanded-output correctness before live authority;
- any unexplained output behavior during later commissioning reopens the relevant hardware check.

This keeps the repository evidence honest: the project is deliberately accepting less bench rigor here rather than claiming measurements that were not completed.
