# SP01 Actual Board Terminal Map

This document freezes the **literal terminal labels visible on the physical board received for SP01**.

Board photographed 2026-09-09:

```text
Waveshare ESP32-S3-POE-ETH-8DI-8DO
MLAB SKU 32108
```

This file is the terminal-reference companion to [`ONBOARDING.md`](ONBOARDING.md). The visual summary is [`assets/BOARD_TERMINALS.svg`](assets/BOARD_TERMINALS.svg).

## 1. Physical field-I/O edge — exact labels from the board cover

The cover identifies three field groups:

```text
DIGITAL OUTPUTS                 DIGITAL INPUTS                 POWER
COM GND 8 7 6 5 4 3 2 1        COM GND 8 7 6 5 4 3 2 1       7~36 V  +  -
```

The open-board photo confirms the corresponding PCB silkscreen around the long terminal strips:

```text
DO1 ... DO8 ... DGND
DI1 ... DI8 ... DGND
DC 7~36V  -  +
```

The safest way to identify a terminal is always by its printed label, not by memorizing left/right orientation. The board can be mounted or photographed rotated.

## 2. SP01 semantic map

### Digital inputs

| Physical terminal | SP01 signal | Meaning |
|---|---|---|
| DI1 | `hopper.feeder_running` | hopper feeder permissive |
| DI2 | `downstream.conveyor_ready` | downstream ready, AUTO only |
| DI3 | `machine.motor_running` | OFF=MANUAL, ON=AUTO |
| DI4 | `process.initiative` | AUTO enable / MANUAL fill ON-OFF |
| DI5 | `cycle.fill_position` | fill-position reference |
| DI6 | `bag.present` | bag presence/pressure switch |
| DI7 | `position.discharge_ref_a` | discharge reference A |
| DI8 | `position.discharge_ref_b` | discharge reference B |
| GND | input field ground/reference terminal | do not confuse with board DC- unless the chosen input topology calls for it |
| COM | digital-input common (`DICOM`) | used according to dry/wet-contact topology |

Firmware GPIO mapping behind the isolated input stage is currently DI1..DI8 = GPIO4..GPIO11. **Technicians never wire directly to these GPIO pins.**

### Digital outputs

| Physical terminal | SP01 signal | Machine destination later |
|---|---|---|
| DO1 | `scanner.down` | scanner cylinder valve command |
| DO2 | `bag_detect_air` | bag-detect air valve command |
| DO3 | `bag.push` | bag push/eject valve command |
| DO4 | `dosing.valve_a` | dosing valve A |
| DO5 | `dosing.valve_b` | dosing valve B |
| DO6 | `dosing.valve_c` | dosing valve C |
| DO7 | `filling.motor` | external contactor/VFD command only |
| DO8 | `spout.aeration` | aeration valve command |
| GND | output field 0 V/reference | output-side return |
| COM | output common (`DOCOM`) | freewheel-diode/common supply terminal |

The DO stage is NPN Darlington/open-collector sinking output. `DOx` is **not** a +24 V source.

## 3. Power terminal — exact board label

The dedicated 2-pole screw terminal is marked:

```text
DC 7~36 V
+   -
```

SP01 final use:

```text
24 VDC PSU +  -> board power +
24 VDC PSU 0V -> board power -
```

Do not use the DI/DO `COM` or `GND` terminals as substitutes for the dedicated board-power terminals.

For first onboarding, USB-only power/flash comes before the 24 V terminal test.

## 4. First DI bench wiring — passive/dry contact

The physical board has the same `COM / GND / DI1..DI8` naming used by Waveshare's isolated 8DI family. Waveshare's published dry-contact scheme leaves the input COM electrically floating from the external supply and closes a dry contact between `COM` and the selected `DIx`.

For the first G2 input test:

```text
INPUT COM ---- simple switch ---- DI1
INPUT GND ---- not used for this dry-contact test
```

Then move the DI side of the same switch to DI2 ... DI8 one channel at a time.

Expected behavior:

```text
switch OPEN   -> DIx OFF
switch CLOSED -> DIx ON
```

If software polarity appears inverted, verify the physical wiring before changing `DI_INVERT_MASK`.

Do **not** inject machine 24 V input signals during the first dry-contact onboarding test.

### Active input later

The board supports active NPN/PNP 5–36 V inputs, but that topology is a later machine-interface step. Freeze the real packer sensor type and reference wiring before connecting it.

## 5. First DO bench wiring — dummy lamp only

For an NPN/open-collector sinking output, use a small 24 V lamp/test load.

Bench concept:

```text
24 V PSU + -------------------+--------------------> OUTPUT COM
                              |
                              +---- LAMP -----> DO1

24 V PSU 0V --------------------------------------> OUTPUT GND
```

When DO1 turns ON, the board sinks current through the lamp. Repeat DO2 ... DO8 one channel at a time.

For the first DO test:

```text
NO machine solenoid
NO motor contactor
NO real packer actuator
```

Required observation for every channel:

```text
command OFF -> lamp OFF
command ON  -> lamp ON
RESET       -> lamp OFF
power cycle -> no unintended pulse
fault       -> safe OFF
```

`DOCOM` is also the common terminal for the output flyback-diode network. With 24 V field loads, connect it to the positive side of the output load supply according to the manufacturer output topology.

The published 500 mA/channel capability is a maximum device capability, not a commissioned load allowance at 70 °C. Real coils require measured current/inrush and thermal testing.

## 6. Top/service edge — exact labels visible on the cover

The cover shows these service groups:

### Multi-function terminal

```text
IO1  IO0  RXD  TXD  SDA  SCL  GND  VCC
```

These terminals are **not needed** for first SP01 bench onboarding. Do not use them as alternate DI/DO terminals.

### CAN

```text
CAN: H  L  PE/earth
```

CAN is unused in SP01 v0.1.

### RS485

```text
RS485: A+  B-  PE
```

Future TLB connection:

```text
TLB485 A / D+ -> board A+
TLB485 B / D- -> board B-
PE/reference  -> only as required by the verified final wiring scheme
```

The open-board photo shows the selectable RS485 termination marked `NC / 120R`. **Do not move that jumper during first-board testing.** Termination is chosen later from the actual G4 RS485 topology.

Firmware mapping behind the onboard isolated RS485 transceiver is currently:

```text
TX  GPIO17
RX  GPIO18
RTS GPIO21
```

Again, technicians wire the A+/B-/PE terminal, not raw GPIO.

## 7. Buttons and visible interfaces

The physical board shows:

```text
RESET button
BOOT button
USB Type-C
PoE/Ethernet port
SMA antenna connector
RGB / PWR / TX / RX indicators
TF-card slot
```

SP01 first-board rule:

```text
USB-C = first power + flash/debug
ANT   = attach antenna
PoE   = unused
Ethernet = unused
BOOT/RESET = service only
```

Do not use BOOT/RESET as normal process controls.

## 8. Beginner visual orientation rule

Because the board may be rotated, **never write instructions such as “third screw from the left” without also writing the printed terminal name**.

Allowed instruction:

```text
Connect PSU + to the screw terminal printed "+" in the 7~36 V group.
```

Not allowed:

```text
Connect PSU + to the left screw.
```

The same rule applies to `COM`, `GND`, DI1..DI8, DO1..DO8, A+, B- and PE.

## 9. Evidence status

```text
physical board model photo        RECEIVED
cover terminal-label photo        RECEIVED
open-board PCB photo              RECEIVED
literal terminal map              FROZEN IN THIS FILE
G1 safe physical boot             PENDING
G2 physical DI/DO validation      PENDING
TLB485                             NOT YET IN HAND
```

If the next purchased board revision has different labels/layout, create a new as-built revision rather than silently reusing this map.
