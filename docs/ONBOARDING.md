# SP01 Beginner Hardware Onboarding

Vietnamese-first procedure for a colleague who has never worked with ESP32, ESP-IDF, industrial DI/DO or RS485.

> **Goal:** take one real Waveshare ESP32-S3 8DI/8DO board from loose hardware to a safe first boot, first firmware flash, first 24 V power test, first DI test and first dummy-DO test **without connecting the real packer actuators**.

Current board:

```text
Waveshare ESP32-S3-POE-ETH-8DI-8DO
MLAB SKU 32108
24 V terminal powered in final SP01 use
USB Type-C used for first flash/debug
Ethernet/PoE unused in SP01 v0.1
```

Actual-board terminal layout has now been photographed and frozen in:

- [`BOARD_TERMINALS.md`](BOARD_TERMINALS.md) — literal labels and wiring;
- [`assets/BOARD_TERMINALS.svg`](assets/BOARD_TERMINALS.svg) — one-page visual map;
- [`FIRST_BOARD_CHECKLIST.md`](FIRST_BOARD_CHECKLIST.md) — printable first-run sheet.

Manufacturer references:

- https://www.waveshare.com/ESP32-S3-POE-ETH-8DI-8DO.htm
- https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO

---

# 1. Golden rules — read first

For the first bench session:

```text
NO machine actuator wiring
NO solenoid wiring
NO filling motor contactor wiring
NO real packer 24 V field signals
NO load cell wired directly to ESP
NO TLB required yet
NO PoE
NO Ethernet
```

Use only:

```text
board
SMA Wi-Fi antenna
USB-C DATA cable
laptop
24 V regulated bench supply later
multimeter
small screwdriver
simple dry-contact switch
small 24 V lamp/test load
wire + ferrules + labels
```

Power OFF before moving any screw-terminal wire.

For the first flash, use **USB power only**. Do not connect 24 V at the same time during novice bring-up.

---

# 2. Learn the board by printed labels

The actual SP01 board has been photographed. The cover shows these groups.

## 2.1 Field-I/O edge

```text
DIGITAL OUTPUTS                 DIGITAL INPUTS                 POWER
COM GND 8 7 6 5 4 3 2 1        COM GND 8 7 6 5 4 3 2 1       7~36 V  +  -
```

Do **not** memorize physical left/right because the board can be mounted rotated. Always say the printed terminal name.

Correct instruction:

```text
Connect PSU + to terminal printed "+" in the 7~36 V group.
```

Bad instruction:

```text
Connect PSU + to the left screw.
```

## 2.2 Service edge

The cover also shows:

```text
IO1 IO0 RXD TXD SDA SCL GND VCC
CAN:   H L PE
RS485: A+ B- PE
USB
PoE
ANT
BOOT
```

For the first board test use only **USB** and **ANT** from this edge. CAN, PoE, Ethernet, RS485 and the multi-function terminal are not needed yet.

---

# 3. What every connector means

## 3.1 USB Type-C

Use for:

```text
5 V board power
firmware flashing
serial/debug communication
```

This is the preferred first-power path.

## 3.2 7–36 VDC power terminal

Final SP01 board power will be 24 VDC.

```text
24 VDC PSU +  -> board terminal printed +
24 VDC PSU 0V -> board terminal printed -
```

Do not use DI/DO `COM` or `GND` as substitutes for the dedicated board-power `+/-` pair.

For a first 24 V bench test, a 1 A current limit is a conservative board-only bench setting, not a final design rating.

## 3.3 SMA antenna

Attach the supplied 2.4 GHz antenna before relying on Wi-Fi.

## 3.4 DI1...DI8

SP01 allocation:

| DI | Signal | Meaning |
|---|---|---|
| DI1 | `hopper.feeder_running` | hopper feeder permissive |
| DI2 | `downstream.conveyor_ready` | downstream ready; AUTO only |
| DI3 | `machine.motor_running` | OFF=MANUAL, ON=AUTO |
| DI4 | `process.initiative` | AUTO enable / MANUAL fill ON-OFF |
| DI5 | `cycle.fill_position` | fill-position reference |
| DI6 | `bag.present` | bag presence/pressure switch |
| DI7 | `position.discharge_ref_a` | discharge reference A |
| DI8 | `position.discharge_ref_b` | discharge reference B |

Physical group also has input `COM` and input `GND`.

For first G2 input test use **dry contact only**:

```text
INPUT COM ---- switch ---- DI1
INPUT GND ---- unused for this dry-contact test
```

Then move DI1 to DI2 ... DI8 one channel at a time.

Expected:

```text
switch OPEN   -> DI OFF
switch CLOSED -> DI ON
```

Firmware mapping behind isolation is currently DI1..DI8 = GPIO4..GPIO11. Technicians never wire directly to these GPIO pins.

## 3.5 DO1...DO8

SP01 allocation:

| DO | Signal | Real destination later |
|---|---|---|
| DO1 | `scanner.down` | scanner-cylinder valve command |
| DO2 | `bag_detect_air` | bag-detect air valve command |
| DO3 | `bag.push` | bag push/eject valve command |
| DO4 | `dosing.valve_a` | dosing valve A |
| DO5 | `dosing.valve_b` | dosing valve B |
| DO6 | `dosing.valve_c` | dosing valve C |
| DO7 | `filling.motor` | contactor/VFD command only |
| DO8 | `spout.aeration` | aeration valve command |

The output stage is NPN Darlington/open-collector sinking output. `DOx` is not a +24 V source.

First dummy-lamp wiring:

```text
24 V PSU + -------------------+--------------------> OUTPUT COM
                              |
                              +---- LAMP -----> DO1

24 V PSU 0V --------------------------------------> OUTPUT GND
```

Repeat DO1 to DO8 one channel at a time.

Do not connect real machine solenoids during first G2.

## 3.6 RS485 — later when TLB arrives

Actual terminal names:

```text
A+   B-   PE
```

Later:

```text
TLB485 A / D+ -> board A+
TLB485 B / D- -> board B-
```

The board photo shows an RS485 jumper marked `NC / 120R`. Leave it at the received/default position until G4 defines whether this node is a bus end requiring termination.

---

# 4. Minimum bench kit

Prepare:

```text
1 x board
1 x SMA antenna
1 x known-good USB-C DATA cable
1 x Windows/Linux laptop
1 x regulated 24 V bench supply
1 x multimeter
1 x small flat screwdriver
1 x simple switch
1 x small 24 V lamp/test load
wire + ferrules + labels
phone/camera for evidence
```

Recommended:

```text
bench fuse holder
USB power meter
DIN rail / stable mounting plate
second person for polarity check
```

---

# 5. Visual inspection before power

1. Confirm cover says `ESP32-S3-POE-ETH-8DI-8DO`.
2. Check no shipping damage, loose terminal, cracked case or metal debris.
3. Check no wire strand is trapped in a terminal.
4. Attach SMA antenna.
5. Leave all screw terminals empty.
6. Do not connect PoE/Ethernet.
7. Photograph board front and terminal labels.
8. Record visible revision/ID if present.

Starting state:

```text
USB disconnected
24 V disconnected
DI empty
DO empty
RS485 empty
CAN empty
machine completely disconnected
```

---

# 6. First power — USB only

```text
Laptop USB
   |
USB-C DATA cable
   |
Board
```

Procedure:

1. Plug USB-C into board.
2. Plug USB into laptop.
3. Confirm PWR indication.
4. Wait 10 s.
5. Check no abnormal heating, smell, smoke or repeated USB disconnect.
6. If any abnormality occurs, unplug immediately.

This proves only basic power. G1 is not yet passed.

---

# 7. Get firmware from GitHub Actions

Repository:

```text
plantops/filling-controller
```

For a colleague who does not compile locally:

1. GitHub -> repository.
2. Open **Actions**.
3. Select workflow **fw**.
4. Open the latest **successful approved** run.
5. Scroll to **Artifacts**.
6. Download:

```text
sp01-esp32-s3-<git-sha>
```

7. Extract to a simple folder, e.g. `C:\sp01\firmware\`.
8. Read `BUILD_INFO.txt`.
9. Record `VERSION` and `GIT_SHA`.
10. Read `ONBOARDING.md`, `BOARD_TERMINALS.md` and `FIRST_BOARD_CHECKLIST.md` included in the artifact.

Do not flash an unapproved commit merely because it is newer.

---

# 8. First flash

## 8.1 With ESP-IDF

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

## 8.2 From GitHub artifact with esptool

Install once:

```bash
python -m pip install --upgrade esptool
```

Open terminal in the extracted artifact folder.

Windows:

```powershell
python -m esptool --chip esp32s3 -p COM6 write_flash "@flash_args"
```

**The quotes are required on PowerShell.** `@` is PowerShell's splatting
operator, so an unquoted `@flash_args` expands to an undefined variable and is
dropped before esptool ever sees it. esptool then connects, configures the
flash, resets the chip and exits reporting success — having written nothing.
Verified on Windows 11 / esptool 5.4.0, 2026-09-10; this silent no-op cost a
full day of bring-up.

A real write prints `Writing at 0x...` progress and a `Wrote N bytes` line for
each region. If those lines are absent, nothing was flashed.

If esptool rejects the underscore options inside `flash_args`, write the three
regions explicitly:

```powershell
python -m esptool --chip esp32s3 -p COM6 --baud 460800 write-flash `
  --flash-mode dio --flash-freq 40m --flash-size 16MB `
  0x0     bootloader\bootloader.bin `
  0x8000  partition_table\partition-table.bin `
  0x10000 sp01_filling_controller.bin
```

Linux:

```bash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 write_flash @flash_args
```

Use the real detected port.

### Always confirm which image is running

A successful flash report is not evidence that the application partition
changed. After every flash, capture the boot log and compare the reported
`App version` against the `GIT_SHA` file in the bundle:

```powershell
python tools\g1_usb_probe.py --port COM6 --seconds 30
Get-Content .\GIT_SHA
```

If they do not match, the flash did not land. Fix that before interpreting any
other test result. See `docs/G1_REMOTE_BRINGUP.md`.

If download does not start:

1. try another known-good DATA cable;
2. try another USB port;
3. fallback ROM-download procedure: hold **BOOT**, press/release **RESET**, release **BOOT**, retry flash.

After successful flash, RESET once or unplug/replug USB.

---

# 9. Expected first boot with no TLB

The TLB is not yet in hand. That is okay.

Expected:

```text
board boots
firmware identifies itself
control task stays alive
TLB = absent/error/stale
controller remains safe
ALL DO remain OFF
no reboot loop
```

Record:

```text
VERSION
GIT_SHA
boot time
reset reason if shown
TLB error/status
reboot count/behavior
```

A missing TLB must not cause random outputs.

---

# 10. Wi-Fi / HMI expectation

Wi-Fi is supervisory only.

A generic CI artifact may have no site Wi-Fi credentials. Therefore:

```text
HMI unreachable != controller failed
```

When credentials are configured and board joins the AP:

```text
http://<esp32-ip>/
```

Current HMI:

```text
Status
I/O
Calibration
Diagnostics
```

Never use ad-hoc browser/raw GPIO writes to force outputs.

---

# 11. First 24 V board-power test

Do this only after USB flash/boot is understood.

1. Unplug USB.
2. PSU OFF.
3. Set PSU to 24.0 VDC.
4. Set conservative current limit, e.g. 1 A board-only.
5. Verify cable polarity with multimeter.
6. Connect PSU + to dedicated board terminal printed `+` in `7~36V` group.
7. Connect PSU 0 V to dedicated board terminal printed `-`.
8. Re-check polarity.
9. Turn PSU ON.
10. Confirm normal PWR indication/boot.
11. Watch supply current and temperature.
12. PSU OFF before touching/moving any terminal wire.

STOP if:

```text
current limit hits unexpectedly
voltage collapses
board smells/heats abnormally
repeated reboot
polarity is uncertain
```

---

# 12. First DI test — exact physical wiring

Use board 24 V power as established above. Keep DO loads disconnected.

For DI1:

```text
INPUT COM ---- dry switch ---- DI1
```

Do not connect input GND for this passive-contact test.

Test:

```text
switch open   -> DI1 OFF
switch closed -> DI1 ON
switch open   -> DI1 OFF
```

Then move only the DI-side wire:

```text
DI1 -> DI2 -> DI3 -> ... -> DI8
```

Record every channel in `FIRST_BOARD_CHECKLIST.md`.

If a channel appears inverted:

1. stop;
2. verify terminal label;
3. verify switch continuity;
4. verify HMI/serial signal name;
5. only then consider `DI_INVERT_MASK`.

Do not modify firmware just to hide a wiring error.

---

# 13. First DO test — exact dummy-load concept

Only after G1 safe boot is understood.

Use one small 24 V lamp first.

```text
PSU +24 V -> OUTPUT COM
PSU 0 V   -> OUTPUT GND
PSU +24 V -> one side of lamp
other lamp side -> DO1
```

Then use the approved bench/service output-control path to test DO1.

Expected:

```text
DO command OFF -> lamp OFF
DO command ON  -> lamp ON
RESET          -> lamp OFF
power cycle    -> no unintended pulse
fault          -> lamp OFF
```

Repeat DO1..DO8.

Never use random one-off GPIO firmware to bypass the production output-ownership model.

Before using a real coil later, measure:

```text
coil nominal voltage
steady current
inrush if relevant
duty cycle
number of channels that may be ON together
```

The vendor 500 mA/channel maximum is not a commissioned allowance at 70 °C.

---

# 14. RESET and power-cycle safety

With dummy load installed, test at least:

```text
normal boot
RESET button
USB power cycle
24 V board power cycle
missing TLB
Wi-Fi absent
```

For every case:

```text
NO unintended DO pulse
NO output remains ON after reset
NO reboot loop
```

This is core G1 evidence.

---

# 15. Do not use these interfaces during first onboarding

```text
PoE/Ethernet
CAN H/L/PE
IO1/IO0/RXD/TXD/SDA/SCL/GND/VCC service terminal
RS485 A+/B-/PE
TF card
real machine sensors
real solenoids
motor contactor/VFD input
```

They are introduced only when their gate requires them.

---

# 16. Stop conditions

POWER OFF immediately if:

```text
smoke
burning smell
abnormal heat
unexpected current-limit hit
polarity uncertain
terminal label uncertain
unexpected DO pulse
DO remains ON after reset
repeated reboot
one DI activates another channel unexpectedly
wire strand/short found
```

Never troubleshoot by moving energized screw-terminal wires.

---

# 17. Evidence to save

For G1/G2 record:

```text
board front photo
terminal-label photo
board/revision label
VERSION
GIT_SHA
USB first boot result
24 V first boot result
DI1..DI8 results
DO1..DO8 results
RESET result
power-cycle result
unexpected behavior
bench PSU voltage/current
ambient temperature
technician/date
```

Do not mark a gate PASS from memory. Keep evidence.

---

# 18. Gate interpretation

```text
G1 PASS requires:
  safe boot
  no reset loop
  missing TLB handled safely
  all physical DO safe OFF through boot/reset

G2 PASS requires:
  DI1..DI8 physical identity proven
  DO1..DO8 dummy-load identity proven
  reset/power-cycle behavior safe

G2T later:
  elevated temperature + serviceability

G4 later:
  TLB485 RS485 weight transport
```

Current situation:

```text
ESP board: IN HAND
TLB485:    NOT YET IN HAND
machine:   MUST REMAIN DISCONNECTED
```

---

# 19. One-page memory aid

```text
1  Read labels
2  Antenna on
3  All screw terminals empty
4  USB only
5  Flash approved GitHub artifact
6  Verify safe boot / no TLB / all DO OFF
7  USB off
8  24 V to dedicated + / - only
9  Dry contact: INPUT COM <-> switch <-> DIx
10 Dummy lamp: +24 -> lamp -> DOx; OUTPUT COM=+24; OUTPUT GND=0V
11 Test reset/power cycle
12 Save evidence
13 Machine stays disconnected
```
