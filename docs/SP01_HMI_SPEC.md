# SP01 HMI and I/O specification (frozen 2026-09-13)

Everything here was confirmed by the plant owner. Do not re-derive it, do not
"correct" it from general packer knowledge, and do not change it without asking.

## 1. Machine facts

- Rated output 2000 bags/h of 50 kg net, 8 spouts, **14.4 s per bag per spout**.
- One ESP32 board serves one spout. SP01 = spout 01.
- Empty bag weighs 0.2 kg. Scale is zeroed before each fill.
- Target is set above nominal to compensate cement lost leaving the spout;
  50.2 kg is the field practice for a 50 kg bag.

## 2. Digital input map — 8 channels, 11 signals

The five position sensors all sit on one rotating shaft, one pulse per
revolution each, widely separated in angle. Four of them are OR-wired onto a
single input and decoded by time since the index pulse.

| DI | Semantic name | Notes |
|---|---|---|
| DI1 | hopper.feeder_running | permissive |
| DI2 | downstream.conveyor_ready | permissive |
| DI3 | machine.motor_running | permissive; also selects AUTO vs MANUAL |
| DI4 | process.initiative | permissive |
| DI5 | cycle.fill_position | starts a cycle |
| DI6 | bag.present | bag on spout |
| DI7 | **position.index** | S4 COUNT UP alone, ~315°, one pulse per revolution |
| DI8 | **position.mark** | S1 + S2 + S3 + S5 wired in parallel |

Position sensors and their angles:

| Sensor | Angle | Purpose | Offset after index |
|---|---|---|---|
| S4 COUNT UP | 315° | index / ref_a | 0 ms |
| S5 COUNT DOWN | 340° | ref_b | ~1000 ms |
| S1 RESET | 10° | after discharge | ~2200 ms |
| S2 INIT SCANNER | 55° | lower scanner | ~4000 ms |
| S3 REJECT | 210° | eject broken bag | ~10200 ms |

Minimum separation is ~1 s, which tolerates well over 10% speed variation.

**Safety rule.** Exactly one index pulse per revolution is required. A missing
index, an extra pulse, or a mark outside its expected window raises
`DISCHARGE_TIMING_INVALID` and drives outputs to the safe state. Position is
never guessed when decoding fails.

Board DI is active-low; invert mask 0xFF.

## 3. Digital output map

| DO | Semantic name |
|---|---|
| DO1 | scanner.down |
| DO2 | bag_detect_air |
| DO3 | bag.push |
| DO4 | dosing.valve_a |
| DO5 | dosing.valve_b |
| DO6 | dosing.valve_c |
| DO7 | filling.motor |
| DO8 | spout.aeration |

Board DO is active-low; invert mask 0xFF.

Three distinct terms, never conflated:

- `desired_do` — what the FSM requests.
- `commanded_do` — what was actually sent to the board adapter.
- `physical_do` — reserved for independently measured electrical state. Not
  available today; never use this label for anything else.

## 4. Discharge and reject

- Healthy bag: push at normal discharge, ~355°.
- Broken bag: dosing outputs DO4..DO8 off immediately, REJECT latched, push at
  the reject window ~210° (S3).
- A rejected bag must never be pushed a second time at ~355°.
- Broken-bag detector: weight high-water loss of 2.0 kg persisting 100 ms across
  new `WeightSnapshot.sequence` values. Provisional until real TLB data exists.

## 5. Aeration

Aeration (DO8) assists when flow stalls.

| Stage | Rate below | Persisting | Action |
|---|---|---|---|
| COARSE_FILL | 10.0 kg/s | 300 ms | aeration on |
| FINE_FILL | 4.0 kg/s | 200 ms | aeration on |

Thresholds are tunable per recipe.

## 6. Recipes

Selected at run time, grouped by bag weight family:

- 50 kg family: 50.1 … 50.7
- 40 kg family: 40.1 … 40.6

Each recipe holds: target net kg, coarse cutoff, filling speed thresholds,
aeration rate and persistence, settle time, push time, and tunable delays.

Stored in NVS on the ESP32; optionally mirrored to microSD.

Later, with enough operating data, the system may suggest an optimal recipe —
out of scope for now.

## 7. Identity

One firmware binary for all boards. Machine ID and spout ID live in **NVS**, set
once through a `/setup` page during commissioning. The controller refuses to run
the process until identity is set.

Rationale: hard-coding identity per board would produce one binary per spout and
reintroduce the "which image is on this board" problem that already cost a full
day of bring-up.

## 8. Operator (OP) screen

Audience: plant operators. Today reached from a phone or laptop over the ESP32
Wi-Fi AP; later a 10" tablet or thin client mounted on the cabinet.

Must be responsive across phone, tablet and desktop. Bilingual EN + VN.

Shows, in priority order:

1. machine ID and spout ID
2. weight of the bag just discharged — the settled net value latched at push
3. bags this shift
4. bags today

Large type, readable at a distance.

The only operator action is selecting the target from the recipe list. Counters
reset automatically. Shift boundaries: **00:00, 08:00, 16:00**. Day rolls at
00:00.

Faults are shown with the remedy written in Vietnamese, not just a code.

Reporting exists but is low priority — data is pushed to the server.

## 9. Permissions

- Operator: select target from the list.
- Supervisor: password-protected; tunes recipe values.

## 10. DEV screen

Used for understanding controller behaviour, tuning, and diagnosing stoppages —
all three equally, because this is a prototype. Used beside the machine on a
laptop; remote later.

Three modes:

- **HW TEST** — real DI read; per-channel state, source, last transition time.
- **FULL SW** — semantic simulation injected at the `InputImage` /
  `WeightSnapshot` / `PositionSnapshot` boundary. Never at raw GPIO. Step,
  pause, resume, reset. Drives the existing `Controller`, with the **production
  recipe**, not a private config.
- **INTERLOCK** — controller decides from real inputs. Two sub-modes:
  - *pump only*: machine not rotating;
  - *full auto*: machine rotating, real bags, real cement.

Requirements:

- One live timeline containing DI, weight, FSM state, desired DO, commanded DO.
- Signals drawn as **level bars** showing ON duration, not transition ticks.
  Short pulses keep a minimum visible width.
- Default window 30 s, covering one 14.4 s cycle with margin. Zoom and freeze.
- Time axis follows the server clock, not the last event.
- Rolling event trace with timestamp, signal, old value, new value.
- History review; export to microSD when the trace is too large to hold.
- Manual DO test per channel: OFF / ON / PULSE with duration in ms, showing
  requested, commanded and elapsed time.
- Control authority always unambiguous: AUTO FSM | MANUAL TEST | SHADOW
  SUPPRESSED.

**All derived values are computed in firmware.** The browser renders; it never
re-implements permissive logic, output maps, or transition tables. The previous
implementation kept a second copy of these in JavaScript and is the main reason
for this rebuild.

## 11. Visual design

- Light theme, modern web, Inter typeface.
- ISA-101 discipline: neutral light grey background, colour reserved for
  abnormal conditions. A normally running machine is nearly colourless.
- Avoid the earlier failure: wasted space in some areas, missing information in
  others, and excessive scrolling.

## 12. Commissioning boundary

The prototype is for shadow commissioning. Machine actuator authority is not
granted. Real TLB, calibration, and G4/G5 remain open. Software tests never
stand in for physical verification.
