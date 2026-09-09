# SP01 First Board Checklist — Print This

Board: `Waveshare ESP32-S3-POE-ETH-8DI-8DO`

Technician: ____________________  Date: ____________________

Board ID / label: ____________________

Firmware VERSION: ____________________

GIT_SHA: ____________________

Reference: [`BOARD_TERMINALS.md`](BOARD_TERMINALS.md) / [`assets/BOARD_TERMINALS.svg`](assets/BOARD_TERMINALS.svg)

## A. Before power

- [ ] Correct board model confirmed
- [ ] No visible damage / loose metal / wire strands
- [ ] SMA Wi-Fi antenna attached
- [ ] DI terminals empty
- [ ] DO terminals empty
- [ ] RS485 empty
- [ ] CAN empty
- [ ] Ethernet/PoE not connected
- [ ] 24 V not connected
- [ ] Machine completely disconnected

## B. USB-only first power

- [ ] Known-good USB-C **data** cable used
- [ ] Board PWR indicator ON
- [ ] No smell / smoke / abnormal heating
- [ ] Laptop sees serial/USB device

STOP if any abnormal heating, smell, smoke or USB over-current occurs.

## C. Firmware artifact

GitHub:

```text
Actions -> fw -> approved successful run -> Artifacts
```

Download:

```text
sp01-esp32-s3-<git-sha>
```

- [ ] Artifact SHA matches approved SHA
- [ ] `VERSION` recorded
- [ ] `GIT_SHA` recorded
- [ ] `ONBOARDING.md` read
- [ ] `BOARD_TERMINALS.md` read

## D. Flash

ESP-IDF path:

```bash
idf.py -p <PORT> flash monitor
```

Artifact/esptool path:

```bash
python -m esptool --chip esp32s3 -p <PORT> write_flash @flash_args
```

- [ ] Flash successful
- [ ] Board reset/rebooted
- [ ] No reboot loop

## E. G1 — safe first boot, no TLB

Expected:

```text
TLB absent/error/stale = expected
controller alive
no random output activity
ALL DO safe OFF
```

- [ ] Firmware identity correct
- [ ] TLB missing is handled safely
- [ ] Control remains alive
- [ ] DO1 OFF
- [ ] DO2 OFF
- [ ] DO3 OFF
- [ ] DO4 OFF
- [ ] DO5 OFF
- [ ] DO6 OFF
- [ ] DO7 OFF
- [ ] DO8 OFF
- [ ] RESET causes no unintended DO pulse

G1 result:  PASS / FAIL

Notes: ____________________________________________________________

## F. 24 V board-power test

USB disconnected first.

Actual dedicated power group is printed:

```text
7~36 V   +   -
```

- [ ] PSU OFF before wiring
- [ ] PSU set to 24.0 VDC
- [ ] Bench current limit set conservatively
- [ ] PSU + -> dedicated terminal printed `+`
- [ ] PSU 0 V -> dedicated terminal printed `-`
- [ ] Polarity checked with multimeter
- [ ] 24 V connected
- [ ] PSU ON
- [ ] Normal PWR indication
- [ ] No current-limit hit / voltage collapse
- [ ] No abnormal heating
- [ ] PSU OFF before moving wires

## G. G2a — DI one-by-one dry-contact test

Exact first-bench wiring:

```text
INPUT COM ---- dry switch ---- DIx
INPUT GND ---- unused for this passive-contact test
```

Do not inject machine 24 V signals during this first test.

| DI | Semantic signal | OFF | ON | PASS |
|---|---|---|---|---|
| DI1 | hopper.feeder_running | [ ] | [ ] | [ ] |
| DI2 | downstream.conveyor_ready | [ ] | [ ] | [ ] |
| DI3 | machine.motor_running | [ ] | [ ] | [ ] |
| DI4 | process.initiative | [ ] | [ ] | [ ] |
| DI5 | cycle.fill_position | [ ] | [ ] | [ ] |
| DI6 | bag.present | [ ] | [ ] | [ ] |
| DI7 | discharge_ref_a | [ ] | [ ] | [ ] |
| DI8 | discharge_ref_b | [ ] | [ ] | [ ] |

## H. G2b — DO dummy-load test

**Dummy lamp/test load only. No machine solenoid yet.**

Exact first-bench concept:

```text
PSU +24 V -> OUTPUT COM
PSU 0 V   -> OUTPUT GND
PSU +24 V -> lamp -> DOx
```

Remember: DO is a sinking/open-collector transistor output, not a +24 V source.

| DO | Semantic signal | OFF | ON | Reset->OFF | PASS |
|---|---|---|---|---|---|
| DO1 | scanner.down | [ ] | [ ] | [ ] | [ ] |
| DO2 | bag_detect_air | [ ] | [ ] | [ ] | [ ] |
| DO3 | bag.push | [ ] | [ ] | [ ] | [ ] |
| DO4 | dosing.valve_a | [ ] | [ ] | [ ] | [ ] |
| DO5 | dosing.valve_b | [ ] | [ ] | [ ] | [ ] |
| DO6 | dosing.valve_c | [ ] | [ ] | [ ] | [ ] |
| DO7 | filling.motor command only | [ ] | [ ] | [ ] | [ ] |
| DO8 | spout.aeration | [ ] | [ ] | [ ] | [ ] |

G2 result: PASS / FAIL / PARTIAL

## I. RS485 — leave alone for now

Actual terminal group:

```text
A+   B-   PE
```

- [ ] TLB not yet connected
- [ ] RS485 `NC/120R` jumper not moved

G4 starts only when the TLB485 is in hand.

## J. Stop immediately if

- [ ] smoke / smell / abnormal heating
- [ ] PSU current limit reached unexpectedly
- [ ] polarity uncertain
- [ ] terminal label unclear
- [ ] unexpected DO pulse
- [ ] output remains ON after reset
- [ ] repeated reboot
- [ ] one DI changes another channel unexpectedly

If any item above occurs: **POWER OFF FIRST. Do not move wires while energized.**

## K. Evidence saved

- [ ] Board front photo
- [ ] Terminal-label photo
- [ ] Board/revision label photo
- [ ] Firmware VERSION + GIT_SHA
- [ ] USB boot result
- [ ] 24 V power result
- [ ] G1 result
- [ ] DI table
- [ ] DO table
- [ ] Reset/power-cycle result
- [ ] Bench PSU voltage/current
- [ ] Ambient temperature
- [ ] Abnormal observations recorded

Next detailed reference: [`ONBOARDING.md`](ONBOARDING.md)

Next gates after G1/G2:

```text
G2T thermal/serviceability
G4  TLB485
G3/G6 process dry cycles when suitable weight source exists
G8  shadow
G9  controlled machine pilot
```
