# SP01 Hardware v0.1

## Mission

The hardware is an open, replaceable controller node intended to extend the useful life of a mechanically valuable rotary packer after the original controller becomes obsolete or difficult to source.

The field philosophy is:

> **DESIGN FOR REPLACEMENT, NOT IMMORTALITY.**

A low-cost controller may be replaced periodically if failure is contained, safe and quick to recover. The machine and weighing chain are the valuable assets; the ESP node is a serviceable module.

## Topology

```text
STATIONARY
Laptop -> dedicated 2.4 GHz AP/router
                         ))) supervisory Wi-Fi

ROTATING SP01
ESP32-S3
  |- 8 isolated DI
  |- 8 protected DO
  |- isolated RS485 -> TLB485 -> load cell
  `- local web HMI
```

No Ethernet crosses the rotating boundary.

## Actual board terminal map — photographed and frozen

Physical board in hand:

```text
Waveshare ESP32-S3-POE-ETH-8DI-8DO
MLAB SKU 32108
```

The cover explicitly labels the field groups:

```text
DIGITAL OUTPUTS                 DIGITAL INPUTS                 POWER
COM GND 8 7 6 5 4 3 2 1        COM GND 8 7 6 5 4 3 2 1       7~36 V  +  -
```

Service edge labels visible on the cover:

```text
IO1 IO0 RXD TXD SDA SCL GND VCC
CAN:   H L PE
RS485: A+ B- PE
USB / PoE / ANT / BOOT
```

The open-board photo also shows:

```text
DI1..DI8 / DGND
DO1..DO8 / DGND
RESET
RS485 NC/120R termination selector
CAN NC/120R termination selector
```

Canonical literal map and diagrams:

- [`BOARD_TERMINALS.md`](BOARD_TERMINALS.md)
- [`assets/BOARD_TERMINALS.svg`](assets/BOARD_TERMINALS.svg)
- [`ONBOARDING.md`](ONBOARDING.md)

**Rule:** always identify a connection by printed terminal name, never only by left/right physical position.

## First DI bench topology

For the initial passive/dry-contact G2 test:

```text
INPUT COM ---- switch ---- DIx
INPUT GND ---- unused for this dry-contact test
```

Test DI1..DI8 one at a time before connecting any real packer field signal.

## First DO bench topology

The DO stage is open-collector/sinking NPN Darlington.

Use a small 24 V dummy lamp:

```text
24 V PSU + -------------------+--------------------> OUTPUT COM
                              |
                              +---- LAMP -----> DOx

24 V PSU 0V --------------------------------------> OUTPUT GND
```

Do not connect real solenoids for first G2. `DO7 filling.motor` is always only a command to an external contactor/VFD input, never motor power.

## Replaceable field unit

Production wiring should converge on a connectorized, labelled controller interface:

```text
fixed machine side
  24 V control power
  DI1..DI8
  DO1..DO8
  RS485 A/B
       |
       v
short labelled / keyed harness or pluggable terminals
       |
       v
replaceable SP01 controller
```

Avoid making controller replacement a field rewiring job. Where practical, use pluggable terminals, labelled adapters or a short service harness so a known-good spare can replace a failed node without individual conductor re-termination.

One controller serves one spout. No SP01 node is master for the other seven.

See [`SERVICEABILITY.md`](SERVICEABILITY.md).

## Environmental condition — possible 70 °C ambient

Possible machine ambient at the rotating electronics may reach approximately **70 °C**.

Do not assume the complete controller board, enclosure, connectors or TLB assembly are qualified at 70 °C merely from individual semiconductor ratings. The installed system must be characterized.

Prefer the following passive measures before adding maintenance-heavy cooling:

```text
coolest practical mounting point
shield from direct radiant heat
separate control and actuator heat sources
reasonable passive conduction / ventilation compatible with cement dust
short, serviceable wiring
```

A dust-sensitive fan is not a required design dependency in v0.1.

The low-cost ESP controller may be treated as a replaceable consumable if field lifetime is economically acceptable. The TLB485 is a separate weighing module; if its verified environmental capability or measured field behavior is inadequate, move it to a cooler location or select an appropriate transmitter rather than assuming the same disposable policy.

## G2T thermal + serviceability characterization

After basic dummy I/O, add a dedicated G2T gate:

```text
measure real controller/TLB installation temperatures
run representative DI/DO load
run RS485 polling
run Wi-Fi/HMI
exercise boot/reset/brownout/fault
verify all-safe output behavior
record temperature-related resets/errors
perform one spare-controller replacement drill
verify config recovery
verify ESP replacement does not alter TLB calibration
```

A controlled elevated-temperature soak up to the expected 70 °C ambient is desirable where suitable test equipment is available. The purpose is to characterize behavior and maintenance economics, not to claim an unverified long lifetime.

## Weighing hardware boundary

Production weight follows one canonical path:

```text
load cell bridge
    -> LAUMAS TLB485
    -> isolated RS485 terminal on ESP32 board
    -> digital WeightSnapshot in firmware
```

Rules:

- do not connect the raw mV/V load-cell bridge to ESP32 ADC/GPIO;
- do not use a 24 V DI for continuous weight values;
- all eight DIs remain reserved for machine signals;
- optional binary TLB status via DI is future-only and would require I/O reallocation;
- use the board RS485 `A+ / B- / PE` terminal, not raw UART pins at the panel boundary.

Current onboard RS485 firmware mapping is TX GPIO17, RX GPIO18, RTS GPIO21 behind the board transceiver.

The actual board photo shows the RS485 termination selector marked `NC / 120R`. Leave it unchanged until G4 freezes the bus topology and termination requirement.

Current communication bring-up profile is 9600 bit/s, address 1, 50 ms poll. After G4 proves clean transport, target high-rate operation is 115200 bit/s with 20 ms polling; 10 ms is test-only after measured margin.

See [`WEIGHING.md`](WEIGHING.md).

## Bench first

Current immediate bench, before TLB arrives:

```text
USB-C -> first flash / debug
24 V dedicated + / - -> board-power test
INPUT COM + dry switch -> DI1..DI8
OUTPUT COM/GND + lamp -> DO1..DO8
RS485 -> leave unused until TLB arrives
```

When TLB arrives:

```text
RS485 A+ / B- -> TLB485 -> bench load cell
```

Use representative 24 V inductive loads only after logic testing. During G4, measure RS485 errors, weight age/latency and reconnect behavior while inductive loads are switching.

## Power

24 VDC is assumed available on the rotating machine. Before field connection measure voltage range, available current, grounding and voltage dip during switching.

Dedicated board power terminal is explicitly marked `7~36 V`, `+`, `-`.

Keep control power and actuator power separately protected. Existing E-stop/safety isolation remains independent of ESP32.

## Outputs

All local DO are used. `filling.motor` drives only a contactor/drive command, never motor power.

On boot/reset/watchdog/fault, software commands all outputs OFF. Confirm the real pneumatic/mechanical safe state before machine connection.

## Spare policy

For field pilot/production, keep at least one pre-flashed known-good controller spare for the packer. Bench-verify safe boot and I/O before storage. Store firmware version, as-built config and recovery instructions outside the spare itself.

A controller swap should not require TLB recalibration when the same healthy TLB remains installed.

## Network rule

AP/router loss may make the HMI offline, but must not change local control behavior.

## Hardware references

See [`../hardware/README.md`](../hardware/README.md).
