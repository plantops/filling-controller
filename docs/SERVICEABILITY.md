# SP01 Serviceability and Asset-Life Extension

## Mission

This project exists primarily to keep a mechanically valuable rotary packer serviceable after the original electronic controller has become discontinued or difficult to source.

The objective is not to reproduce a premium proprietary controller with a 20-year electronics lifetime. The objective is to remove the controller as a single-source obsolescence risk while preserving the machine.

```text
DISCONTINUED VENDOR CONTROLLER
            |
            v
open, documented, replaceable SP01 node
            |
            v
extend life of the existing mechanical packer
```

## Design principle

> **DESIGN FOR REPLACEMENT, NOT IMMORTALITY.**

The ESP32 controller may be treated as a serviceable consumable provided that failure is contained, safe, quickly recoverable and inexpensive compared with machine downtime or replacement of the packer.

Current economic assumption: replacement of a controller costing roughly VND 1.5 million even on the order of six months is acceptable. This is an economic tolerance, **not** a prescribed preventive-replacement interval. Actual replacement interval must come from field evidence.

## Failure-containment rules

- one controller owns one spout; no SP01 node is master for the other seven;
- loss of one node must not require replacement of the packer or redesign of the other spouts;
- boot/reset/watchdog/fault must command the SP01 output image safe;
- Wi-Fi, HMI and remote services are never required for local filling control;
- a controller replacement must not require changing the load-cell bridge or TLB calibration when the TLB remains healthy;
- critical machine knowledge must not exist only inside one ESP flash device.

## Replaceable field unit

The field installation should converge on a connectorized, labelled module:

```text
fixed machine harness
   |
   +-- 24 V control power
   +-- DI1..DI8
   +-- DO1..DO8
   +-- RS485 A/B to TLB485
   `-- antenna / service USB as applicable
             |
             v
      replaceable SP01 controller
```

A board swap should avoid individual field-wire re-termination. Use labelled terminal blocks, keyed/pluggable connectors or a short adapter harness where practical.

Do not define a hard MTTR number until a real replacement drill is performed. The design goal is a swap measured in minutes rather than a rewiring/commissioning job.

## Spare strategy

For field pilot and production use:

1. keep at least one known-good controller spare for the packer;
2. pre-flash the approved firmware before storage;
3. verify safe boot and dummy I/O on the spare;
4. keep the current firmware tag/commit and as-built configuration with the machine record;
5. keep a printed/portable minimum recovery sheet with wiring, branch/tag, TLB address and service steps.

Eight spouts should use the same firmware image wherever possible. Per-spout differences belong in explicit configuration such as `spout_id`, I/O inversion, target recipe and commissioned timing values.

## Configuration ownership

The long-term source of truth is outside the disposable controller.

Required recoverable information includes:

```text
firmware version / commit
spout_id
I/O inversion / channel mapping
TLB485 serial settings and address
active and approved target recipes
commissioned timing values
service token / credentials through an appropriate secure process
calibration audit metadata
```

Current v0.1 still uses ESP-IDF `menuconfig` for many production parameters. Runtime export/import and simplified spare provisioning are therefore **pending serviceability work**, not already-complete features.

Calibration itself remains owned by the weighing chain. If the ESP is replaced while the same calibrated TLB485 remains installed, controller replacement must not automatically write zero/span.

## 70 °C environment

Measured/possible machine ambient may reach approximately **70 °C**. This is a first-class field condition.

The project does not assume that the complete current prototype board or TLB assembly is automatically qualified for 70 °C merely because individual semiconductor parts may tolerate high temperature.

Instead, v0.1 uses two controls:

### 1. Characterize the environment

Record temperature at the actual controller and TLB locations during:

```text
cold start
normal operation
hottest expected production period
prolonged operation
representative solenoid/DO switching
```

### 2. Design for safe degradation and replacement

Thermal failure must not create an unsafe output state. If the low-cost ESP node shows an economically acceptable field lifetime, planned or condition-based replacement is allowed.

Prefer, in order:

- coolest practical mounting point on the rotating machine;
- shielding from direct radiant heat;
- passive thermal path / ventilation compatible with cement dust;
- higher-rated controller/transmitter only when evidence shows it is needed.

Do not make a dust-sensitive fan a required control dependency unless passive measures are insufficient and a maintainable cooling design is explicitly chosen.

The TLB485 is a separate service module from the cheap ESP controller. If its verified environmental rating or measured behavior is inadequate at the installed location, relocate it or select a suitable weighing transmitter rather than assuming the same disposable lifetime strategy as the ESP node.

## G2T — thermal/serviceability characterization

Add a dedicated bench/field gate after basic dummy I/O:

```text
G2T THERMAL + SERVICEABILITY
  measure actual installation temperatures
  run controller with representative I/O + RS485 + Wi-Fi load
  exercise boot/reset/brownout/fault at elevated temperature
  verify outputs fail safe
  record temperature-related resets/errors
  perform one spare-controller replacement drill
  verify configuration recovery
  verify TLB calibration is not changed by ESP replacement
```

A controlled 70 °C soak is desirable where safe test equipment is available. The purpose is to understand behavior and replacement economics, not to claim an unverified 20-year component lifetime.

## Success definition

The project succeeds when the packer is no longer hostage to a discontinued controller.

A successful production system therefore optimizes:

```text
safe failure containment
low replacement cost
low repair/recovery time
known-good spares
recoverable configuration
independent weighing calibration
open firmware and documentation
continued use of the mechanical asset
```

Long electronics life is useful, but it is secondary to safe operation and recoverability for this asset-life-extension mission.
