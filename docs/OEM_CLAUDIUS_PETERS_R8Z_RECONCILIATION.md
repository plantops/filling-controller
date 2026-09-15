# SP01 OEM Reconciliation — Claudius Peters R8 ZML / PACTRON

Status: engineering reference for retrofit reconciliation. This document does **not** override measured installed-machine evidence.

Source:

- Claudius Peters, *Rotary Packer and Star Feeder — Technical Documentation Operation Mechanical / Electrical*
- TCEC / Chinfon Haiphong II
- CPP commission no. `02-1/158.215/750`
- OEM electrical drawing `02-1/158.215-360/A-0`
- R...Z operating manual `711.0009.3000A/02.en`
- PACTRON Master manual `711.0009.0021A/01.en`
- PACTRON calibrating instructions `711.0009.0022A/00.en`

## 1. Authority boundary

Use this document as an OEM reference layer only.

Ground truth remains:

```text
measured installed-machine evidence
> verified as-built wiring / field traces
> executable controller + frozen commissioning contracts
> OEM manual reference
> UI projections / historical design proposals
```

The 2007/2008 OEM documentation describes the delivered machine. The installed machine may have been modified since delivery.

## 2. OEM spout architecture

Each filling module is an automatic gravimetric filling instrument. The OEM process description states that each module has two control initiators:

```text
start initiator       -> begins filling sequence
bag-discharge initiator -> initiates discharge when the bag is completely filled
```

OEM sequence:

```text
packer rotating
-> start initiator reached
-> bag holding cylinder extends and clamps bag
-> air through holding rubber
-> pressure builds only if bag is correctly attached
-> pressure signal reports BAG AVAILABLE
-> filling turbine starts
-> filling gate opens
-> labyrinth/filling-vessel aeration starts
-> weighing electronics controls filling
-> filled bag waits for discharge initiator
-> discharge cylinder tilts/pushes bag off
```

If no bag is correctly attached, pressure does not build and filling is not started. The discharge mechanism removes an incorrectly attached bag.

## 3. OEM PACTRON I/O for one weighing unit

Electrical drawing pages for weighing unit 1 repeat the same structure for units 2..8.

### Inputs shown and named by OEM

```text
I0.0  SPOUT AERATION
I0.1  THROW OFF MANUAL
I0.2  REST DISCHARGE
I0.3  BAG ON SPOUT
I0.4  THROW OFF AUTOMATIC
I0.5  START
I1.2  STOP
I1.3  ENABLE THROW OFF
```

The drawing also exposes additional input pins without named field functions on the sheet:

```text
I0.6
I0.7
I1.0
I1.1
I1.4
I1.5
I1.6
I1.7
```

Do not assign retrofit semantics to those unnamed pins without as-built evidence.

### Outputs shown and named by OEM

```text
Q0.0  TURBINE
Q0.1  SPOUT AERATION
Q0.2  THROW OFF
Q0.3  BAG FIXING
Q0.6  FINE STREAM
Q0.7  COARSE STREAM
Q1.2  AERATION PACKER SILO
```

`Q0.4` and `Q0.5` appear on the drawing but have no field-function label on that sheet.

## 4. Retrofit semantic comparison

Current commissioning semantics in `SP01_CANONICAL_VIEWS.md` are not a literal OEM PACTRON map.

Current model:

```text
DI1 hopper.feeder_running
DI2 downstream.conveyor_ready
DI3 machine.motor_running
DI4 process.initiative
DI5 cycle.fill_position
DI6 bag.present
DI7 position.discharge_ref_a
DI8 position.discharge_ref_b

DO1 scanner.down
DO2 bag_detect_air
DO3 bag.push
DO4 dosing.valve_a
DO5 dosing.valve_b
DO6 dosing.valve_c
DO7 filling.motor
DO8 spout.aeration
```

OEM terminology instead provides directly recognizable functions such as:

```text
START
BAG ON SPOUT
THROW OFF AUTOMATIC
REST DISCHARGE
COARSE STREAM
FINE STREAM
TURBINE
SPOUT AERATION
BAG FIXING
THROW OFF
```

Therefore no current `DIx/DOx` semantic should be called an OEM mapping unless verified against the installed wiring.

## 5. Candidate semantic equivalences

These are naming-level correspondences only, not physical terminal assignments:

```text
OEM BAG ON SPOUT       <-> bag.present
OEM START              <-> fill/start-position event
OEM THROW OFF          <-> bag.push / eject
OEM COARSE STREAM      <-> coarse-feed command
OEM FINE STREAM        <-> fine-feed command
OEM TURBINE            <-> filling turbine command
OEM SPOUT AERATION     <-> spout aeration command
OEM BAG FIXING         <-> bag clamp / fixing command
OEM REST DISCHARGE     <-> residual-discharge request/state
```

The current `dosing.valve_a/b/c`, `scanner.down`, `bag_detect_air`, and A/B discharge-reference semantics require field reconciliation; they are not established by the OEM PACTRON sheets alone.

## 6. OEM weighing chain

OEM load cell wiring into a PACTRON Basic Module is six-wire analog:

```text
+supply
+sense
+signal
-signal
-sense
-supply
```

PACTRON Basic Modules then communicate on an RS-485 bus to the PACTRON Master.

Retrofit boundary remains conceptually valid:

```text
6-wire analog load cell
-> industrial ADC / weight transmitter
-> isolated digital link
-> SP01 WeightSnapshot
```

The replacement ADC/transmitter must be selected from actual load-cell electrical requirements and required update-rate/filter behavior, not by treating the OEM load cell as a digital RS-485 cell.

## 7. OEM coarse/fine weighing model

PACTRON bagging mode normally uses main + fine feed.

Sort parameters include:

```text
target weight
after flow / in-flight compensation
dribble feed
fine-feed time
discharge threshold
tolerance plus
tolerance minus
```

The OEM description defines `after flow` as material that continues to flow after fine feed is switched off. This value can be automatically corrected by the controller.

For retrofit logic this means a simple static `net >= target` cutoff is not equivalent to the full OEM weighing strategy. Commissioning should retain explicit concepts for:

```text
coarse -> fine transition
fine-feed cutoff
in-flight / after-flow compensation
settle / no-motion evaluation
tolerance evaluation
```

Do not freeze numeric values from the manual examples as production SP01 values.

## 8. Filters and stability

OEM calibration/setup includes:

```text
dynamic filter     active during dosage
static filter      used for no-motion recognition
no-motion time
blocking time after discharge
```

OEM notes that increasing the filter setting increases vibration damping but reduces scale response speed.

The project must therefore treat filter latency as part of G4/G5 weighing characterization, especially because cutoff and broken-bag detection share the same measured signal.

## 9. Broken-bag detection — important OEM finding

PACTRON Master contains an explicit `bag break delay` function.

OEM behavior:

```text
if actual filling speed < configured minimum filling speed
    during main or fine feed
    -> start bag-break-delay timer

if filling speed recovers above minimum
    -> reset timer

if timer expires
    -> set "bag break / fillspeed < min." signal
```

The timer may also be gated by an external `enable bag break detection` input if that signal is configured.

OEM parameters include:

```text
bag break delay
minimum fill speed main feed
minimum fill speed fine feed
```

A zero minimum-fill-speed value disables bag-break detection for that feed stage.

### Reconciliation with current SP01 detector

Current SP01 executable logic uses a running high-water measured-weight-loss detector with persistence.

That is **not identical** to the OEM algorithm:

```text
OEM:     low filling speed for a configured delay
SP01:    measured weight loss from recent peak for a configured persistence
```

Both use the weight trajectory and persistence, but they have different trip domains. A slow/no-flow condition may satisfy OEM logic without producing a negative weight excursion; conversely, a strong negative excursion directly satisfies the current SP01 model.

Do not silently call the current detector an OEM clone.

The installed-machine reject requirement recorded in `BROKEN_BAG_REJECT.md` remains a separate field requirement until G4/G8 traces reconcile the two algorithms.

## 10. 210° / 355° routing status

The OEM manual sections reviewed here establish start and discharge initiators and automatic throw-off behavior, but do not establish the project's specific `~210° reject` and `~355° normal` timing values.

Those values remain installed-machine / commissioning evidence, not OEM-manual-derived values.

Keep:

```text
PositionSnapshot.reject_window
normal discharge timing adapter
```

as semantic boundaries until G8 establishes the physical reference geometry and actual as-built signals.

## 11. Immediate engineering consequences

Do not rewrite the controller around PACTRON names yet.

Required reconciliation sequence:

```text
R1  OEM reference extraction                 DONE in this document
R2  as-built terminal trace SP01             required
R3  map each existing machine wire -> OEM function / modified function
R4  freeze physical DI1..DI8 assignment
R5  freeze physical DO1..DO8 assignment
R6  capture live weight + output timing trace
R7  compare SP01 loss detector vs OEM low-fill-speed detector
R8  freeze 210° / 355° position adapter from measured geometry/signals
R9  only then revise canonical V7/V2 mappings and production controller contract
```

Until R2-R8 are complete, use explicit truth labels:

```text
OEM_CONFIRMED
FIELD_CONFIRMED
SOFTWARE_MODEL
UNVERIFIED
```

This avoids turning a legacy drawing, a software model, or a commissioning assumption into false physical ground truth.
