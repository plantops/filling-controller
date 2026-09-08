# Embedded Web HMI

## Status

**Prototype HMI proposal. Supervisory only. Not part of the safety chain or time-critical filling loop.**

For v0.1, SP01 hosts its own web HMI and connects as a Wi-Fi client to **one dedicated stationary AP/router**. A laptop/browser connects to the same AP/router.

There is no live Ethernet link across the rotating/stationary boundary.

The controller must continue a safe filling/abort sequence if the browser, laptop, Wi-Fi link, AP/router, machine HMI, or historian disappears.

The initial HMI should be mostly read-only. **Weighing calibration is the deliberate v0.1 exception:** a controlled service workflow is required to zero/span/verify the SP01 weighing chain before any useful hardware commissioning can occur.

## 1. v0.1 HMI topology

```text
                     STATIONARY

Laptop / browser
      │
 Wi-Fi or LAN
      │
      ▼
┌───────────────────┐
│ dedicated AP/     │
│ router            │
│ PACKER01-CONTROL  │
└─────────┬─────────┘
          │ 2.4 GHz Wi-Fi
          )))

                     ROTATING
          (((
          │
          ▼
┌──────────────────────┐
│ ESP32-S3 SP01        │
│                      │
│ deterministic FSM    │
│ embedded HTTP server │
│ HTML/CSS/JS/SVG      │
│ REST API             │
│ WebSocket telemetry  │
│ RS485 → TLB          │
│ local DI/DO          │
└──────────────────────┘
```

The laptop executes no controller logic.

The AP/router executes no controller logic.

## 2. Network role

The AP/router is only a supervisory transport and IP network endpoint.

It may provide:

```text
Wi-Fi association
DHCP/reservations or routing
engineering-browser access
optional stationary LAN uplink later
```

It must not provide:

```text
spout state machine
interlocks
cutoff timing
output arbitration
safety logic
required cycle storage
```

AP/router loss is an HMI/communications warning, not permission for unsafe controller behavior.

## 3. SP01 addressing

For prototype use either a DHCP reservation or fixed address on a dedicated subnet, for example:

```text
AP/router       10.20.1.1
SP01            10.20.1.101
engineering PC  DHCP/static on 10.20.1.0/24
```

Exact address plan, credentials and security settings are deployment configuration, not controller semantics.

Future SP02..SP08 addresses are reserved conceptually but are not part of the v0.1 hardware build.

## 4. Wi-Fi behavior

The SP01 node operates in Wi-Fi station/client mode during normal service.

Prototype rules:

- dedicated packer SSID, not office/guest Wi-Fi;
- 2.4 GHz first;
- AP placed near/above the machine where practical;
- fixed or intentionally managed RF channel;
- no cloud or Internet dependency;
- automatic reconnect is allowed only in a low-priority network task;
- power saving should not compromise connectivity on a mains-powered controller;
- expose RSSI, reconnect count, last disconnect reason, uptime, packet loss/latency estimates where practical;
- mark HMI data stale immediately when telemetry age exceeds the configured limit;
- buffer cycle/event data locally during outages.

The v0.1 RF acceptance test must include full 360° machine rotation at representative speed and with the node in its intended physical enclosure/location.

## 5. Optional service hotspot

A per-spout ESP SoftAP may be added later for local maintenance, but it is not a v0.1 requirement.

If implemented, it should be manually/service-mode enabled rather than permanently broadcasting.

Normal architecture remains:

```text
SP01 = Wi-Fi client
AP/router = stationary network
```

Do not make SP01 an AP/master for future spouts.

## 6. HMI design principle: one object, multiple skins

The digital twin is not one decorative animation. The selected machine object has synchronized representations:

```text
OUTER SKIN  = physical/operator view
INNER SKIN  = controller/I-O/wiring anatomy
TIMING      = state + I/O waveforms + weight/flow vs time/angle
HISTORY     = bag/cycle performance and diagnostics
FAULTS      = active/latched faults and causal context
CONFIG      = controlled parameters and commissioning data
SERVICE     = guarded commissioning/calibration functions
```

All views must be generated from the same semantic machine model and event stream.

## 7. Outer skin

Purpose: answer "what is SP01 physically doing now?"

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
- fault/permissive summary;
- Wi-Fi status as supervisory health, not machine state.

## 8. Inner skin — corrected I/O anatomy

```text
                    ESP32-S3 SP01
             ┌────────────┬────────────┐
             │ INPUTS     │ OUTPUTS    │
             │            │            │
 field ────► │ DI01       │ DO01 ─────► scanner
 field ────► │ DI02       │ DO02 ─────► detect air
 field ────► │ DI03       │ DO03 ─────► bag pusher
 field ────► │ DI04       │ DO04 ─────► dosing A
 field ────► │ DI05       │ DO05 ─────► dosing B
 field ────► │ DI06       │ DO06 ─────► dosing C
 field ────► │ DI07       │ DO07 ─────► fill motor command
             │ DI08 spare │ DO08 ─────► aeration
             └──────┬─────┴────────────┘
                    │ isolated RS485
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

The Inner view exposes:

- semantic signal name;
- physical channel;
- current command/value;
- source (`SIM`, `HARD`, `SHADOW`, `REPLAY`);
- quality/staleness;
- owning adapter/device;
- upstream condition for an output;
- last transition timestamp.

Do not display a commanded actuator as physically proven ON unless feedback exists. Command and measured feedback are separate concepts.

## 9. Cause-and-effect linkage

The HMI must show causality, not only lamps.

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

1. authorizing expression/conditions;
2. currently true/false conditions;
3. controller state;
4. last causal input/event changes;
5. physical output channel;
6. whether physical feedback exists.

Do not duplicate controller logic manually in frontend JavaScript. The linkage view should be generated from controller/spec metadata.

## 10. Timing / scope view

For SP01:

```text
one spout revolution/cycle ≈ 14.4 s at 2,000 bags/h machine throughput
```

Support X-axis by elapsed time and rotary angle when angle is available.

Display one synchronized timeline:

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
Wi-Fi outage markers for diagnostics only
```

Required modes:

```text
LIVE
REPLAY
COMPARE
```

Digital I/O transitions should be event records with controller monotonic timestamps. Browser arrival time is not the process timestamp.

## 11. History / local buffering

Minimum cycle record:

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

The ESP retains a bounded recent history/event queue locally.

When Wi-Fi is unavailable:

```text
continue local control
continue local cycle/event recording
mark uplink offline
upload/synchronize later when connection returns
```

Loss of network must not mean loss of the current bag record.

## 12. Configuration view

Parameters may include:

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
- Wi-Fi/network settings.

Configuration is data, not source code.

Accepted changes must produce a new configuration version and persist atomically before becoming active.

Prefer applying fill-related changes at a safe boundary such as `IDLE`/next cycle, not mid-fill.

## 13. API boundary

Illustrative endpoints:

```text
GET  /api/v1/state
GET  /api/v1/io
GET  /api/v1/cycles
GET  /api/v1/faults
GET  /api/v1/network
GET  /api/v1/config
GET  /api/v1/weighing
POST /api/v1/commands/reset-fault
POST /api/v1/config/apply
POST /api/v1/weighing/zero
POST /api/v1/weighing/check
POST /api/v1/weighing/span
POST /api/v1/weighing/calibration/save
WS   /ws/live
```

The browser never directly addresses GPIO, TLB registers, or Modbus coils.

All writes pass through authorization, current-state checks and interlocks. The endpoint names above are illustrative contracts, not permission to expose raw hardware operations.

## 14. Timing separation

Keep network/HMI refresh independent of controller timing.

Illustrative domains:

```text
controller/process image      2–5 ms class
TLB communications            measured independently
HMI live telemetry            50–100 ms class
Wi-Fi reconnect/service       low priority
history persistence           event-driven/lower priority
```

Exact control timing is verified by measurement. Wi-Fi latency is irrelevant to the local cutoff path because Wi-Fi is not in it.

## 15. Frontend implementation

Prefer a small embedded frontend:

```text
static HTML
CSS
vanilla JavaScript or equivalent small client
SVG
WebSocket
```

The web task must use bounded buffers/connections and remain lower priority than control and TLB communication.

Browser assets/API versions must be compatible after firmware update.

## 16. Roles and service commands

Suggested roles:

```text
VIEWER    read only
OPERATOR  limited normal commands
ENGINEER  controlled recipe/config/calibration changes
SERVICE   commissioning/I-O tests/firmware/network diagnostics
```

I/O forcing is high risk and should require:

```text
machine stopped
AND automatic control disabled
AND physical service/commissioning enable
AND authenticated service role
AND bounded force timeout
AND visible force-active state
AND audit record
```

Wi-Fi loss, session timeout, reboot, or exit from service mode clears software forces to defined safe states.

## 17. Failure policy

The following must have no adverse local control effect:

- AP/router power loss;
- AP/router reboot;
- RF shadow during rotation;
- laptop power loss;
- browser refresh/crash;
- multiple viewers;
- Wi-Fi reconnect storm;
- malformed HTTP request;
- slow client;
- WebSocket disconnect/reconnect;
- historian unavailable.

The controller may report communications warnings and HMI staleness, but the control FSM remains local.

During a calibration transaction, Wi-Fi/browser loss must leave either the previous known-good calibration or a fully committed new calibration; never an ambiguous half-applied state.

## 18. Security minimums

Before plant-network connection:

- dedicated SSID/network;
- strong unique AP/router credentials;
- no default controller write credentials;
- authentication for all writes;
- role separation;
- no unrestricted GPIO/register endpoint;
- bounded HTTP/WebSocket resources;
- controlled OTA path;
- firmware/config version identification;
- audit privileged actions;
- network segmentation/firewall review if uplinked to plant LAN.

Signed firmware, secure boot and flash encryption should be evaluated before production rollout.

## 19. v0.1 HMI/RF acceptance tests

Before live SP01 control:

1. switch AP/router off during every FSM state;
2. reboot AP/router during coarse and fine fill;
3. kill browser/laptop during every FSM state;
4. rotate packer through 360° and log RSSI/disconnects/latency;
5. test at realistic packer speed and nearby electrical loads active;
6. verify stale data is visibly marked;
7. verify local history persists through network outage;
8. open multiple clients and measure control jitter;
9. force controlled reconnect storms and prove control task is unaffected;
10. interrupt configuration/calibration writes and verify atomic recovery;
11. verify unauthorized writes are rejected;
12. verify service output forces clear on disconnect/reset/timeout;
13. verify timing and causality use controller timestamps, not browser timestamps;
14. replay a recorded cycle after an RF outage and prove the local record is complete.

## 20. Future eight-spout HMI

Only after SP01 succeeds, the stationary AP/router may eventually serve eight peer nodes:

```text
                    AP/router
        ┌──────┬──────┼──────┬──────┐
        )))    )))    )))    )))    )))
       SP01   SP02   ...           SP08
```

A stationary machine-level HMI/historian may aggregate them.

The aggregator is not a master required for local filling.

If the RF survey proves eight rotating clients unreliable, later alternatives include a rotating Ethernet switch plus one wireless bridge or a proper Ethernet-rated rotary joint. Those are not v0.1 requirements.

## 21. Principle to preserve

```text
Browser     = observability / controlled command surface
AP/router   = supervisory network transport
Wi-Fi       = non-critical supervisory link
ESP32       = autonomous SP01 controller
TLB         = local weighing front end
RS485       = local deterministic fieldbus
Safety      = independent hardwired/safety architecture
```

The critical test is:

> **Unplug the AP/router. SP01 must continue to behave correctly because the network was never part of the control loop.**

## 22. Weighing calibration — required v0.1 service UI

See `docs/WEIGHING_CALIBRATION.md` for the authoritative workflow.

The normal commissioning sequence is:

```text
empty saddle → SET ZERO
20 kg known reference → CHECK
50 kg standard → SET SPAN
remove weight → VERIFY ZERO
20 kg → VERIFY
50 kg → VERIFY
SAVE + LOCK
```

Calibration actions are allowed only when the node is in a safe calibration/service state, automatic filling is disabled, all filling outputs are safe, TLB data is fresh/healthy and the measured weight is stable.

`ZERO`, `TARE`, and `CALIBRATION` must remain visibly separate concepts in the UI.

The browser never writes raw TLB register addresses. The service path is:

```text
Web UI
  ↓
CalibrationService
  ↓
WeighingUnit
  ↓
TLB adapter
  ↓
TLB485
```

Each completed calibration produces an append-only calibration/audit record and preserves the prior known-good calibration reference.