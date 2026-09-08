# Embedded Web HMI

## Status

**Prototype HMI proposal. Supervisory only. Not part of the safety chain or time-critical filling loop.**

Each spout controller hosts its own web HMI. Any laptop, tablet, panel PC, or service computer with a browser can access it over wired Ethernet.

The controller must continue a safe filling/abort sequence if the browser, laptop, Ethernet cable, machine HMI, or historian disappears.

## 1. HMI topology

One-spout prototype:

```text
Laptop browser
     │
     │ Ethernet / HTTP + WebSocket
     ▼
ESP32-S3 SP01
     │
     ├── deterministic filling FSM
     ├── embedded HTTP server
     ├── static HTML/CSS/JS/SVG
     ├── REST API
     ├── WebSocket telemetry
     ├── RS485 → TLB
     └── local DI/DO
```

The laptop executes no controller logic.

For the eight-spout machine:

```text
                         MACHINE LAN
                             │
           ┌──────┬──────┬───┴───┬──────┬──────┐
           ▼      ▼      ▼       ▼      ▼      ▼
         SP01   SP02   SP03      ...   SP07   SP08
          ESP    ESP    ESP             ESP    ESP
           │      │      │               │      │
          TLB    TLB    TLB             TLB    TLB

                             │
                             ▼
                   optional machine HMI /
                    historian / PlantOps
```

Each spout remains directly serviceable even when the optional machine-level aggregator is unavailable.

## 2. Addressing

Example static/service subnet:

```text
SP01  10.20.1.101
SP02  10.20.1.102
SP03  10.20.1.103
SP04  10.20.1.104
SP05  10.20.1.105
SP06  10.20.1.106
SP07  10.20.1.107
SP08  10.20.1.108
```

Actual network design, VLAN, gateway, DNS, authentication, and plant firewall rules must be reviewed before production deployment.

## 3. HMI design principle: one object, multiple skins

The digital twin is not one decorative animation. The selected machine object has synchronized representations:

```text
OUTER SKIN  = physical/operator view
INNER SKIN  = controller/I-O/wiring anatomy
TIMING      = state + I/O waveforms + weight/flow vs time/angle
HISTORY     = bag/cycle performance and diagnostics
FAULTS      = active/latched faults and causal context
CONFIG      = controlled parameters and commissioning data
```

All views must be generated from the same semantic machine model and event stream.

## 4. Outer skin

Purpose: answer "what is the physical spout doing now?"

Show:

- hopper/product path;
- dosing mechanism;
- filling motor/impeller;
- aeration;
- scanner/bag clamp/detection mechanism;
- bag;
- push-off cylinder;
- downstream conveyor;
- current state;
- current weight;
- target weight;
- coarse/fine/off state;
- fault/permissive summary.

For the full packer, show eight spouts around the rotary machine. Clicking one spout opens that same node in Inner/Timing/History without losing the selected cycle/time context.

## 5. Inner skin

Purpose: answer "why is the machine doing that?"

Represent the actual control anatomy:

```text
                   ESP32-S3 SP01
            ┌────────────┬────────────┐
            │ INPUTS     │ OUTPUTS    │
            │            │            │
 field ───► │ DI01       │ DO01 ─────► scanner
 field ───► │ DI02       │ DO02 ─────► detect air
 field ───► │ DI03       │ DO03 ─────► dosing A
 field ───► │ DI04       │ DO04 ─────► dosing B
 field ───► │ DI05       │ DO05 ─────► dosing C
 field ───► │ DI06       │ DO06 ─────► fill motor
 field ───► │ DI07       │ DO07 ─────► aeration
            │ DI08 spare │ DO08 ─────► push
            └──────┬─────┴────────────┘
                   │ RS485
                   ▼
                 LAUMAS TLB
                   │
                load cell
```

Recommended visual semantics:

```text
blue    input path
orange  output path
green   active/healthy
red     fault/blocked
gray    inactive/unknown/stale
```

The Inner view must expose:

- semantic signal name;
- physical channel;
- current value;
- source (`SIM`, `HARD`, `SHADOW`, `REPLAY`, etc.);
- quality/staleness;
- owning adapter/device;
- upstream condition for an output;
- last transition timestamp.

## 6. Cause-and-effect linkage

The HMI must show **causality**, not only ON/OFF lamps.

Example:

```text
hopper OK ───────┐
conveyor OK ─────┤
machine running ─┼──► MACHINE PERMISSIVE
process enable ──┘
                         │
fill position ───────────┤
                         ▼
                   BAG ACQUIRE
                         │
bag present ─────────────┤
                         ▼
                     START FILL
                         │
              ┌──────────┴─────────┐
              ▼                    ▼
          coarse/fine          motor/aeration
```

Clicking an output should show:

1. the expression/conditions that authorize it;
2. which conditions are currently true/false;
3. the controller state;
4. the last causal input/event changes;
5. the physical output channel.

Do not duplicate logic manually in the frontend. The linkage view must be derived from the same controller/specification metadata used by tests and documentation.

## 7. Timing / scope view

This is the primary engineering view.

For one spout at 2,000 bags/h machine throughput:

```text
one spout cycle ≈ 14.4 s
one machine bag interval ≈ 1.8 s
```

The X-axis should support both:

- elapsed time;
- rotary angle when angle is available.

Display on one synchronized timeline:

```text
controller STATE
DI01..DI08
DO01..DO08
raw/filtered weight
fill rate
projected final weight
coarse/fine transition
cutoff event
push event
fault/event markers
```

Required modes:

- `LIVE` — current cycle;
- `REPLAY` — historical cycle;
- `COMPARE` — overlay selected cycles or SIM vs HARD/SHADOW.

Continuous measurements should be sampled separately from event transitions. Digital I/O transitions should be stored as events with exact monotonic timestamps where possible.

## 8. History / bag record

Minimum per-cycle record:

```text
cycle_id
spout_id
recipe_id
target_weight
final_weight
error
cycle_start
cycle_end
fill_total_time
coarse_start/end
fine_start/end
weight_at_cutoff
estimated/actual residual flow
peak/coarse/fine fill rate
push time
result
faults
firmware_version
config_version
```

The local ESP may retain only a bounded recent history. Long-term records belong on an optional machine/edge historian.

## 9. Configuration view

Parameters likely required:

- target weight;
- tolerance;
- coarse transition/cutoff strategy;
- predictive/residual-flow parameters;
- bag-detect delay;
- scanner timing;
- push delay/duration;
- fill/push angle windows;
- sensor debounce;
- timeout values;
- TLB communication/configuration subset;
- node/spout identity;
- network settings.

Configuration is data, not source code.

Every accepted configuration change must produce:

```text
config_version
who/role changed it
when
old value
new value
reason/comment when required
```

The controller must support rollback to the last known-good configuration.

## 10. API boundary

Illustrative endpoints:

```text
GET  /api/v1/state
GET  /api/v1/io
GET  /api/v1/cycles
GET  /api/v1/faults
GET  /api/v1/config
POST /api/v1/commands/reset-fault
POST /api/v1/config/apply
WS   /ws/live
```

The browser must never directly address GPIO, TLB registers, or Modbus coils.

All commands pass through the controller's authorization, state, and interlock validation.

## 11. Telemetry rates

Keep HMI refresh independent from controller timing.

Illustrative domains:

```text
controller/process image      2–5 ms class
TLB communication             measured/validated separately
HMI live telemetry            50–100 ms class (10–20 Hz)
history/event persistence     event driven / lower priority
```

The exact controller tick is a measured design parameter, not an HMI requirement.

The HMI must not request or render hundreds of updates per second merely because the controller runs faster.

## 12. Frontend implementation

For the embedded node prefer:

```text
static HTML
CSS
vanilla JavaScript or similarly small client code
SVG for outer/inner/timing views
WebSocket for live data
```

Avoid a heavyweight frontend framework unless it demonstrably improves maintainability within ESP flash/RAM constraints.

The browser application should be cache/version aware so a firmware update cannot leave stale incompatible assets silently running.

## 13. Roles and command policy

Suggested roles:

### VIEWER

- read state;
- read timing/history;
- read I/O;
- no writes.

### OPERATOR

- acknowledge/reset allowed faults;
- normal operational commands explicitly approved by machine design.

### ENGINEER

- recipe/config changes;
- calibration-related workflows where permitted;
- controlled commissioning actions.

### SERVICE

- I/O test/force;
- firmware update;
- network/service diagnostics.

## 14. I/O forcing

This is the highest-risk HMI feature.

Do not expose casual `DOx ON/OFF` buttons.

Output forcing should require all of the following or an equivalent validated policy:

```text
machine stopped
AND normal automatic control disabled
AND physical service/commissioning enable active
AND authenticated service role
AND explicit selected channel
AND bounded force timeout
AND visible force-active indication
AND audit record
```

A browser disconnect, timeout, role expiry, controller reset, or exit from service mode must clear all software forces to defined safe states.

The existing safety chain remains independent of HMI/service forcing.

## 15. HMI failure policy

The following events must have no adverse control effect:

- browser refresh;
- browser crash;
- laptop power loss;
- Ethernet disconnect;
- multiple simultaneous viewers;
- malformed HTTP request;
- slow client;
- WebSocket reconnect storm;
- historian unavailable;
- machine-level HMI unavailable.

The web task must never block the deterministic control path.

## 16. Machine-level HMI later

After several independent spout nodes are live, an optional aggregator can present:

```text
PACKER01                         2,0xx BPH

SP01   FINE       48.9 kg    OK
SP02   SETTLING   50.0 kg    OK
SP03   WAIT BAG              OK
SP04   COARSE     22.7 kg    OK
SP05   PUSH                  OK
SP06   FINE       46.4 kg    OK
SP07   COMPLETE   50.0 kg    OK
SP08   FAULT                 BAG DETECT

mean giveaway
standard deviation
under/overweight rate
per-spout throughput
per-spout drift
fault counts
```

The machine HMI is an aggregator, not a master required by the spout nodes.

## 17. Digital-twin synchronization

The current Python simulator should model the same semantic node contract as firmware.

A selected node can be:

```text
SIM
HARD
SHADOW
REPLAY
```

The HMI must be able to render any source through the same UI model.

This enables:

- simulation before hardware;
- real TLB + simulated outputs;
- real inputs + calculated shadow outputs;
- physical SP01;
- replay of a production fault;
- SIM-vs-real timing comparison.

## 18. Security minimums before plant network connection

At minimum:

- unique node identity;
- no default shared administrator password in production;
- authentication for all writes;
- role separation;
- rate limiting / bounded connections where practical;
- no unrestricted register/GPIO endpoints;
- configuration integrity check;
- firmware version identification;
- controlled firmware update path;
- audit of privileged actions;
- network segmentation/firewall review.

Signed firmware/secure boot/flash encryption should be evaluated before production rollout rather than assumed unnecessary.

## 19. HMI acceptance tests

Before live control:

1. disconnect Ethernet during every FSM state;
2. kill/restart the browser during every FSM state;
3. open multiple browser sessions;
4. flood reconnects within a controlled test environment;
5. verify HMI load cannot break controller-cycle timing;
6. verify stale/invalid data is visibly marked rather than shown as current;
7. verify config rollback after interrupted write/power loss;
8. verify unauthorized writes are rejected;
9. verify service output forces clear on disconnect/reset/timeout;
10. verify HMI assets and API version mismatch is detected;
11. replay a recorded cycle and compare timing/causality views;
12. verify every displayed output can trace back to its authorizing state/conditions.

## 20. Principle to preserve

```text
Browser = observability and controlled command surface
ESP32   = controller
TLB     = weighing front end
Safety  = independent hardwired/safety architecture
```

Never allow the convenience of a web HMI to blur those boundaries.