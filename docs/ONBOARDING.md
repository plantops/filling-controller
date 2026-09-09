# SP01 Beginner Hardware Onboarding

Vietnamese-first guide for a colleague who has never worked with ESP32, ESP-IDF, industrial DI/DO or RS485.

> **Goal of this guide:** take one new Waveshare ESP32-S3 8DI/8DO board from unopened/loose hardware to a safe first boot, first firmware flash, first 24 V power test, first DI test and first dummy-DO test **without connecting the real packer actuators**.

Current target board:

```text
Waveshare ESP32-S3-POE-ETH-8DI-8DO
MLAB SKU 32108
24 V terminal powered in final SP01 use
USB Type-C used for first flash/debug
Ethernet/PoE unused in SP01 v0.1
```

Manufacturer references:

- https://www.waveshare.com/ESP32-S3-POE-ETH-8DI-8DO.htm
- https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO

Manufacturer states the board accepts **7–36 VDC from the power screw terminal or 5 V/1 A from USB Type-C**, supports 8 isolated digital inputs, 8 isolated Darlington open-collector digital outputs, USB firmware download/debug, isolated RS485, Wi-Fi and an external SMA antenna.

---

# 1. Golden rules — read before touching wires

For the first bench session:

```text
NO machine actuator wiring
NO solenoid wiring
NO filling motor contactor wiring
NO packer 24 V field signals
NO load cell wiring directly to ESP
NO TLB required yet
NO PoE
NO Ethernet
```

Use only:

```text
board
USB-C data cable
laptop
external Wi-Fi antenna
bench 24 VDC supply later
multimeter
small screwdriver
simple switches for DI test
dummy lamps / test loads for DO test later
```

**Do not guess terminal order.** Read the printed silk-screen/label next to each terminal on the actual board. If a terminal label is unclear, stop and take a clear photo before applying voltage.

For the first flash, use **USB power only**. Do not connect 24 V at the same time. This removes almost every wiring risk from the first step.

---

# 2. What each connector does

## 2.1 USB Type-C

Use for:

```text
5 V board power
firmware flashing
serial/debug communication
```

This is the preferred first-power method.

## 2.2 7–36 VDC power screw terminal

Final SP01 power source will be 24 VDC.

Conceptually:

```text
24 VDC PSU +  ----> board DC+
24 VDC PSU 0V ----> board DC-
```

**Use the actual `+` / `-` markings printed on the board. Never identify polarity from physical left/right position alone.**

For a first 24 V bench test, use a regulated bench supply with current limiting. A 1 A current limit is a conservative board-only bench setting, not a board rating and not the final actuator supply design.

## 2.3 External antenna

Attach the supplied 2.4 GHz antenna to the SMA connector before relying on Wi-Fi.

Do not use antenna orientation or enclosure position as an electrical ground point.

## 2.4 Digital inputs DI1…DI8

The board supports isolated dry-contact and active 5–36 V input arrangements.

SP01 semantic allocation is:

| Board input | SP01 signal | Meaning |
|---|---|---|
| DI1 | `hopper.feeder_running` | hopper feeder permissive |
| DI2 | `downstream.conveyor_ready` | downstream ready; AUTO only |
| DI3 | `machine.motor_running` | OFF=MANUAL, ON=AUTO |
| DI4 | `process.initiative` | AUTO enable / MANUAL fill ON-OFF |
| DI5 | `cycle.fill_position` | AUTO fill-position reference |
| DI6 | `bag.present` | bag pressure/presence switch |
| DI7 | `position.discharge_ref_a` | discharge reference A |
| DI8 | `position.discharge_ref_b` | discharge reference B |

For first onboarding, use the manufacturer's **passive/dry-contact input topology** with one simple switch and test one channel at a time. Do not inject packer 24 V signals on day one.

Because terminal-strip physical order may vary by board revision, the exact common/return terminal must be identified from the printed board label/manufacturer diagram before wiring the switch. Never assume a COM location from a drawing for another Waveshare product.

Firmware mapping behind the board interface is currently:

```text
DI1 GPIO4
DI2 GPIO5
DI3 GPIO6
DI4 GPIO7
DI5 GPIO8
DI6 GPIO9
DI7 GPIO10
DI8 GPIO11
```

Technicians do **not** wire to these GPIO pins. They wire to the isolated DI screw terminals.

## 2.5 Digital outputs DO1…DO8

SP01 semantic allocation:

| Board output | SP01 signal | Real-machine destination later |
|---|---|---|
| DO1 | `scanner.down` | scanner cylinder valve command |
| DO2 | `bag_detect_air` | bag-detect air valve command |
| DO3 | `bag.push` | bag push/eject valve command |
| DO4 | `dosing.valve_a` | dosing valve A |
| DO5 | `dosing.valve_b` | dosing valve B |
| DO6 | `dosing.valve_c` | dosing valve C |
| DO7 | `filling.motor` | external contactor/VFD command only |
| DO8 | `spout.aeration` | aeration valve command |

The Waveshare outputs are **open-collector/open-drain Darlington transistor sinking outputs**, not 24 V voltage sources. Manufacturer rating is up to 500 mA/channel and the board includes flyback protection.

Conceptual load wiring is therefore:

```text
external +24 V
    |
   LOAD              lamp / relay / representative coil
    |
   DOx  <---- board transistor sinks current when ON
    |
output return/common according to the board's printed terminal scheme
```

Do **not** connect a real filling motor to DO7. DO7 is only a logic command to an external contactor/VFD interface.

For G2, start with lamps or other benign dummy loads. Do not begin with machine solenoids.

Again: before energizing a DO load, identify the exact output common/isolated-supply terminal from the actual board silk-screen/manufacturer diagram. `DOx` is a sinking node; it is not `+24 V`.

## 2.6 RS485

Not needed for the first board session because the TLB485 has not arrived yet.

Later canonical connection is:

```text
TLB485 A / D+  ----> board RS485 A
TLB485 B / D-  ----> board RS485 B
COM/reference  ----> only if required by the verified TLB/board wiring scheme
```

Firmware mapping behind the isolated onboard transceiver is:

```text
TX   GPIO17
RX   GPIO18
RTS  GPIO21
```

Technicians wire to the **RS485 screw terminal**, not directly to GPIO17/18/21.

---

# 3. Minimum bench kit

Prepare:

```text
1 x Waveshare ESP32-S3-POE-ETH-8DI-8DO
1 x supplied SMA Wi-Fi antenna
1 x known-good USB-C DATA cable
1 x Windows or Linux laptop
1 x 24 VDC regulated bench supply
1 x multimeter
1 x small flat screwdriver
8 x simple switches or one switch moved channel-by-channel
8 x 24 V lamps/test loads or one load moved channel-by-channel
wire + ferrules + labels
```

Recommended but not mandatory for first boot:

```text
USB power meter
bench fuse holder
DIN rail
terminal blocks
camera/phone for evidence photos
```

---

# 4. Before power — 2 minute visual inspection

Do this every time a new board arrives.

1. Confirm product label is `ESP32-S3-POE-ETH-8DI-8DO`.
2. Check for shipping damage, loose screw terminals, cracked case or bent connector.
3. Verify there is no wire strand or metal debris across terminals.
4. Attach the Wi-Fi antenna.
5. Leave every DI/DO/RS485/CAN/power screw terminal empty.
6. Do not connect Ethernet/PoE.
7. Record one clear front photo and product/serial/revision information if visible.

Expected starting condition:

```text
USB disconnected
24 V disconnected
all field terminals empty
machine completely disconnected
```

---

# 5. First power-on — USB only

This is the safest first electrical test.

```text
Laptop USB
    |
 USB-C DATA cable
    |
Waveshare board
```

No other cable should be connected except the antenna.

Procedure:

1. Plug USB-C into the board.
2. Plug USB into the laptop.
3. Confirm the board PWR indicator comes on.
4. Wait 10 seconds.
5. Touch nothing on the screw terminals.
6. If the board becomes abnormally hot, smells, smokes, repeatedly disconnects from USB or the laptop reports over-current: unplug immediately and quarantine the board.

Passing this step only proves basic board power. It does **not** pass G1 yet.

---

# 6. Get the firmware from GitHub Actions

Repository:

```text
plantops/filling-controller
```

For a colleague who does not build firmware locally:

1. Open the repository on GitHub.
2. Open **Actions**.
3. Select workflow **fw**.
4. Open the latest successful run for the approved commit.
5. Scroll to **Artifacts**.
6. Download:

```text
sp01-esp32-s3-<git-sha>
```

7. Extract the ZIP to a simple local folder, for example:

```text
C:\sp01\firmware\
```

The artifact contains the firmware binaries plus version/build metadata and the onboarding/checklist documents.

Always record both:

```text
VERSION
GIT_SHA
```

before flashing.

Do not flash an artifact whose commit/version has not been approved for the bench session.

---

# 7. First flash — easiest supported paths

## Path A — technician has ESP-IDF

From repository source:

```bash
cd firmware/esp32-s3
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Windows example:

```powershell
idf.py -p COM6 flash monitor
```

Linux example:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

## Path B — technician uses GitHub Actions artifact

Use the artifact's `flash_args` with Espressif `esptool`.

Install once:

```bash
python -m pip install --upgrade esptool
```

Open a terminal inside the extracted artifact directory.

Windows example:

```powershell
python -m esptool --chip esp32s3 -p COM6 write_flash @flash_args
```

Linux example:

```bash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 write_flash @flash_args
```

Use the actual serial port detected on the laptop.

If normal download cannot start, first try a different known-good data cable and USB port. As a fallback ESP32-S3 download procedure, hold **BOOT**, press/release **RESET**, then release **BOOT** and retry. This fallback is only for entering the ROM download mode; do not use buttons as normal operating controls.

After flash completes successfully, press RESET once or unplug/replug USB.

---

# 8. What a good first boot looks like

With only USB connected and no TLB:

```text
board boots
firmware identifies itself
control task stays alive
TLB communication is absent/error/stale
controller remains safe
ALL physical DO must remain OFF
no reset loop
```

The missing TLB is expected during this first session. It must **not** cause random output activity.

Record:

```text
firmware VERSION
GIT_SHA
boot time
reset reason if shown
TLB status/error
any repeated reboot
```

G1 cannot be marked PASS until all eight physical outputs have been checked safe through boot/reset with suitable indicators/dummy verification.

---

# 9. Wi-Fi / HMI expectation

Wi-Fi is supervisory only and never required for local control.

The current production firmware still uses build/menuconfig values for Wi-Fi credentials. A generic GitHub Actions artifact may therefore have no usable site Wi-Fi credentials configured.

Do not interpret "HMI not reachable" as a controller failure during first USB/G1 testing.

When Wi-Fi credentials are configured and the board joins the dedicated AP/router:

```text
browser -> http://<esp32-ip>/
```

Current live pages show:

```text
Status
I/O
Calibration
Diagnostics
```

Do not use browser workarounds or raw GPIO/Modbus writes to force outputs.

---

# 10. First 24 V power test

Do this **after USB-only boot/flash is understood**.

For a novice, use only one power source at a time during initial bring-up.

1. Shut down/unplug USB.
2. Set bench PSU to 24.0 VDC.
3. Set a conservative board-only current limit, for example 1 A.
4. Confirm PSU is OFF.
5. With multimeter, verify polarity at the loose cable end.
6. Connect PSU `+` to the board terminal explicitly marked positive.
7. Connect PSU `0 V/-` to the board terminal explicitly marked negative.
8. Re-check polarity with another person if available.
9. Turn PSU ON.
10. Confirm PWR indication and normal boot behavior.
11. Turn PSU OFF before moving any wire.

Never identify `+` and `-` from terminal position alone.

If current immediately hits the limit, voltage collapses, the board repeatedly reboots or heats abnormally: switch OFF immediately.

Do not connect USB and 24 V together during this beginner test procedure. Dual-power/debug arrangements can be introduced later only after the exact board power-path behavior is explicitly accepted.

---

# 11. First DI test — one dry-contact channel at a time

Objective: prove that physical terminal DI1 maps to software DI1, then DI2 ... DI8.

Keep DO loads disconnected.

Use the Waveshare **passive/dry-contact** topology shown in the manufacturer wiring diagram. Identify the correct isolated input common/return from the actual board labelling before connecting the switch.

Test sequence:

```text
DI1 OFF -> software/HMI DI1 OFF
close switch
DI1 ON  -> software/HMI DI1 ON
open switch
DI1 OFF -> software/HMI DI1 OFF
```

Repeat for DI2 ... DI8.

Record a table:

| Channel | OFF correct | ON correct | Semantic signal | Result |
|---|---|---|---|---|
| DI1 | | | hopper.feeder_running | |
| DI2 | | | downstream.conveyor_ready | |
| DI3 | | | machine.motor_running | |
| DI4 | | | process.initiative | |
| DI5 | | | cycle.fill_position | |
| DI6 | | | bag.present | |
| DI7 | | | discharge_ref_a | |
| DI8 | | | discharge_ref_b | |

If one channel appears inverted, do not immediately change firmware. First verify wiring and then document whether the final installation requires the DI inversion mask.

---

# 12. First DO test — dummy loads only

Do not use machine solenoids yet.

The production firmware owns DO through the controller FSM; technicians must not bypass it with random GPIO code. A dedicated service/bench I/O test path should be used once available/approved.

For each channel, the electrical test objective is:

```text
command OFF -> load OFF
command ON  -> load ON
reset       -> load OFF
power cycle -> no unintended pulse
fault       -> safe OFF
```

Use one small 24 V lamp/test load first, then repeat channel-by-channel.

The output is a **sinking transistor output**. Wire the load according to the actual board's output/common labels and the Waveshare output wiring diagram. Do not wire a lamp as though DOx were a +24 V source.

Before representative solenoid testing, measure the coil voltage/current and confirm it is within the intended output-stage design. The vendor's 500 mA/channel figure is a maximum device capability, not permission to ignore 70 °C thermal derating or multi-channel heating.

---

# 13. RESET / power-cycle safety test

After dummy-output control is available:

For each relevant condition:

```text
normal boot
RESET button
USB unplug/replug
24 V OFF/ON
firmware restart/watchdog test when available
```

verify:

```text
no unexpected DO pulse
all outputs settle to safe OFF
controller returns to expected state
```

Photograph/video any unexpected flash/pulse of a dummy output and stop the gate until explained.

---

# 14. No-TLB behavior — expected now

Because the TLB485 has not arrived yet:

```text
RS485 terminal = empty
TLB = absent
weight = unavailable/stale/fault
```

Expected controller behavior:

```text
no valid fill authority based on fake weight
no random DO activity
controller stays responsive
TLB error is visible in diagnostics/logs
```

Do not jumper RS485 or create a fake DI to imitate weight.

When TLB arrives, follow [`WEIGHING.md`](WEIGHING.md).

---

# 15. What not to touch in first onboarding

Do not connect:

```text
real load cell mV/V wires to ESP
real solenoid bank
filling motor power
VFD power terminals
machine safety/E-stop wiring
mains AC
PoE injector
CAN
Ethernet for control
```

Do not change:

```text
TLB calibration writes -> must remain disabled until TLB/manual verification
raw Modbus registers
GPIO assignments
DO inversion mask
DI inversion mask
sensor semantics
```

without an approved reason and recorded change.

---

# 16. Stop conditions

Stop immediately if any of these happens:

```text
smoke / smell / visible overheating
USB or PSU over-current
24 V polarity uncertain
terminal label cannot be identified
unexpected output pulse
board reboots repeatedly
one DI affects another DI unexpectedly
output remains ON after reset
metal debris / loose conductor strands
```

Do not "try another wire" while powered.

Power OFF first, then diagnose.

---

# 17. Evidence to save for each board

Create a simple record:

```text
board ID / label
purchase source
hardware revision if visible
photo front/back/terminal labels
firmware VERSION
GIT_SHA
flash date
technician
USB boot PASS/FAIL
24 V boot PASS/FAIL
DI1..DI8 mapping PASS/FAIL
DO1..DO8 dummy result PASS/FAIL
reset safe-output PASS/FAIL
notes / abnormal behavior
```

For production/spare use, add this evidence to the as-built machine record.

---

# 18. G1/G2 beginner acceptance

A colleague who has never used ESP32 should be able to follow this document and demonstrate:

```text
G1a USB power + firmware flash
G1b normal boot
G1c missing TLB handled safely
G1d ALL DO safe through reset

G2a DI1..DI8 terminal identity
G2b DO1..DO8 dummy-load identity
G2c no unintended pulse at boot/reset
```

Only after that move to:

```text
G2T elevated-temperature/serviceability
G4 TLB485
G3/G6 process sequence with suitable simulated/real weight source
G8 shadow
G9 machine authority
```

---

# 19. One-page printable checklist

Use [`FIRST_BOARD_CHECKLIST.md`](FIRST_BOARD_CHECKLIST.md) beside this detailed guide during the actual bench session.

---

# 20. One remaining as-built item

This guide intentionally does **not** guess the physical left-to-right order of the screw terminals from memory or from another Waveshare model.

Before the first 24 V DI/DO wiring session, capture a clear straight-on photo of the actual SKU 32108 terminal strips and freeze an **as-built terminal drawing** in `hardware/photos/` or the machine record.

Once that photo is available, this document can be upgraded from semantic/exact-label wiring to a literal terminal-by-terminal drawing such as:

```text
terminal 01 -> ...
terminal 02 -> ...
...
```

That final drawing should be what a new technician follows at the machine.