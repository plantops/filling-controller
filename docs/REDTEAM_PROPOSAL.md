# Red-Team Review Proposal — Universal Filling Controller

## 1. Review objective

Review the proposed architecture for retrofitting an eight-spout rotary cement bag packer while avoiding two bad endpoints:

1. a hobby-style software controller that owns timing-critical weighing and cutoff; and
2. a new vendor-locked controller where all machine intelligence is embedded in one proprietary device.

The intended architecture uses an industrial weighing controller such as WTX120 for deterministic weighing and fast dosing execution, while keeping machine intelligence, digital twin, advanced diagnostics, recipes, adaptation, data, and integration in an open controller layer defined by vendor-neutral contracts.

This document is intentionally written for hostile review. Challenge the assumptions, boundaries, timing model, failure behavior, maintainability, migration path, and claims of portability.

## 2. Plant context

Legacy machine:

- rotary packer;
- 8 filling spouts;
- nominal throughput: 2,000 bags/h or higher;
- nominal bag target: 50 kg;
- coarse/fine pneumatic dosing cylinder;
- filling motor/impeller;
- spout aeration;
- scanner/bag-position cylinder;
- pressure-based bag detection;
- load-cell weighing;
- pneumatic bag push-off;
- downstream bag conveyor.

The theoretical cycle period available to one spout at exactly 2,000 bags/h is:

```text
3600 s/h × 8 spouts / 2000 bags/h = 14.4 s/spout-cycle
```

This is the complete spout cycle period, not the usable filling duration. Bag detection, mechanical positioning, settling, and discharge consume part of the 14.4 s.

## 3. Legacy I/O baseline

Normalized from the legacy control description:

### Digital inputs

| ID | Semantic signal | Legacy meaning |
|---|---|---|
| DI01 | `hopper.feeder_running` | hopper feeder ON/OFF |
| DI02 | `downstream.conveyor_ready` | downstream bag conveyor status |
| DI03 | `machine.motor_running` | packer/main motor status |
| DI04 | `process.initiative` | process initiative/permissive |
| DI05 | `cycle.fill_position` | legacy INPUT_4; exact field device still to be identified |
| DI06 | `bag.present` | bag-detect pressure switch |
| DI07 | `position.push` | push/discharge position signal |

### Weighing input

| ID | Semantic signal | Type |
|---|---|---|
| W01 | `weight.net` | one independent load-cell weighing channel per spout |

The load-cell bridge is analog at the sensor level. The weighing controller performs A/D conversion; the open controller should receive a digital measurement/event stream.

### Digital outputs

| ID | Semantic actuator | Legacy meaning |
|---|---|---|
| DO01 | `scanner.down` | scanner cylinder solenoid |
| DO02 | `bag_detect_air` | bag-detection air solenoid |
| DO03 | `dosing.valve_a` | dosing pneumatic valve A |
| DO04 | `dosing.valve_b` | dosing pneumatic valve B |
| DO05 | `dosing.valve_c` | dosing pneumatic valve C |
| DO06 | `filling.motor` | filling motor command |
| DO07 | `spout.aeration` | aeration solenoid |
| DO08 | `bag.push` | push-off cylinder solenoid |

Baseline requirement per spout/controller domain:

```text
7 × DI
1 × independent weighing channel
8 × DO
0 × AO currently required
```

## 4. Current problem statement

The legacy Raspberry Pi/HX711 prototype had the correct process idea but the wrong final control boundary. The pilot is still useful for learning, simulation, and shadow testing, but timing-critical weighing should not depend on a general-purpose Linux/Python loop.

At the same time, moving all logic into a proprietary filling controller creates a different problem:

- recipe semantics become vendor-specific;
- state transitions become vendor-specific;
- diagnostics and data structures become vendor-specific;
- advanced control features are constrained by vendor firmware;
- migration to another weighing vendor becomes expensive;
- simulation and replay become difficult to keep behaviorally equivalent.

The proposal separates these concerns.

## 5. Proposed architecture

```text
                         FE / HMI / Engineering UI
                                   |
                            REST / WebSocket
                                   |
                    +---------------------------+
                    | OPEN FILLING CONTROLLER   |
                    |                           |
                    | machine state machine     |
                    | recipe model              |
                    | adaptive tuning           |
                    | soft sensors              |
                    | digital twin              |
                    | historian / cycle replay  |
                    | diagnostics / SPC         |
                    | MES/WMS integration       |
                    +-------------+-------------+
                                  |
                         semantic contracts
                                  |
              +-------------------+-------------------+
              |                   |                   |
              v                   v                   v
       WeighingAdapter        IOAdapter       PositionAdapter
              |                   |                   |
              v                   v                   v
          WTX120-class        Remote I/O       encoder/cams/
          fast weigher        or local I/O     position sensors
              |
          load cell(s)
              |
       local fast dosing
```

The permanent product is not the Python implementation. The permanent product is the machine behavior contract:

- semantic signals;
- capability model;
- state model;
- recipes;
- event schema;
- adapter contracts;
- test vectors;
- conformance scenarios;
- machine profiles.

Python is only the first reference runtime.

## 6. Responsibility split

### 6.1 Industrial weighing controller

Keep timing-sensitive functions close to the weight acquisition hardware:

- load-cell excitation;
- A/D conversion;
- digital filtering required for dosing;
- zero/tare;
- coarse/fine physical output execution;
- local cutoff execution;
- residual-flow/inflight handling available in the device;
- local tolerance result;
- local hardware diagnostics;
- bounded local behavior if the supervisory/open controller disappears mid-cycle.

The exact WTX120 feature set, I/O expansion behavior, network rates, and filler functions must be verified against the exact ordered hardware revision and firmware before procurement. Do not treat screenshots, marketing diagrams, or another firmware revision as the engineering contract.

### 6.2 Open filling controller

Keep machine intelligence outside the weighing vendor:

- machine-level state machine;
- packer-specific sequence;
- recipe ownership;
- multi-spout coordination;
- adaptive cutoff parameter tuning;
- bag-by-bag learning;
- flow characterization;
- soft sensors;
- material-in-flight estimation;
- spout bias/drift monitoring;
- fault diagnostics;
- digital twin;
- deterministic replay;
- shadow control;
- automatic commissioning tools;
- SPC and giveaway analysis;
- historian;
- FE/HMI;
- WMS/MES/API integration;
- vendor-neutral configuration.

## 7. Two-timescale control model

### Fast loop

Executed in the industrial weighing controller:

```text
load-cell sampling
-> filtering
-> coarse/fine execution
-> physical cutoff
```

The exact cycle frequency and output latency are hardware-specific and must be measured/verified.

### Intelligent loop

Executed in the open controller:

```text
observe bag N
-> evaluate actual cutoff / final weight / inflight
-> estimate drift and process condition
-> calculate parameters for bag N+1
-> send bounded parameter updates
```

The open layer should tune the next cycle or bounded future behavior rather than depend on a slow network/API path to issue the final physical cutoff at the last millisecond.

## 8. Vendor-neutral weighing contract

The controller core must not contain WTX register addresses, vendor command IDs, or vendor alarm bit numbers.

Conceptual contract:

```text
WeighingUnit
    configure(recipe)
    zero()
    tare()
    arm()
    start_fill()
    abort_fill()

    set_target()
    set_coarse_parameters()
    set_fine_parameters()
    set_cutoff_parameters()

    read_weight()
    read_status()
    read_result()
    read_diagnostics()

    health()
```

Implementations may include:

```text
VirtualWeigher
WTX120Adapter
SiwarexAdapter
GenericModbusWeigher
FutureVendorAdapter
```

Changing weighing hardware should require a new adapter and machine-profile binding, not a rewrite of the controller state machine.

## 9. Vendor-neutral I/O contract

Do not encode machine logic using physical names such as `WTX_DO3` or `ModbusRegister40013`.

Use semantic channels:

```text
sensor.bag_present
sensor.product_available
sensor.machine_running
sensor.push_position
actuator.scanner_down
actuator.bag_detect_air
actuator.dosing_valve_a
actuator.dosing_valve_b
actuator.dosing_valve_c
actuator.filling_motor
actuator.aeration
actuator.push
```

A profile binds these semantic channels to actual hardware.

The I/O transport may later be:

- WTX local I/O;
- WTX-supported external I/O;
- Modbus TCP remote I/O;
- Modbus RTU through an explicitly supported gateway;
- PLC I/O;
- EtherCAT I/O;
- another fieldbus.

The transport choice must not change controller semantics.

## 10. Dosing-cylinder truth table

Legacy pneumatic logic:

| State | Valve A | Valve B | Valve C |
|---|---:|---:|---:|
| Closed | 0 | 0 | 0 |
| Fine ~30% | 1 | 0 | 1 |
| Coarse 100% | 1 | 1 | 1 |

This can be represented semantically as `OFF`, `FINE`, and `COARSE` while the adapter/profile expands those states into the three physical valve outputs.

The controller core should not know that this particular packer requires three solenoids to create two useful feed states.

## 11. Digital twin role

Simulation is not a separate application. `VirtualWeigher`, `VirtualIO`, and `VirtualPlant` implement the same contracts as physical adapters.

The simulator must separate:

```text
plant truth
sensor measurement
controller estimate
```

Required simulated dynamics:

- gate opening/closing lag;
- coarse/fine material flow;
- transport delay;
- material in flight after cutoff;
- load-cell noise;
- motor/vibration disturbance;
- settling;
- bag detection;
- discharge position;
- mechanical timing;
- sensor faults;
- actuator faults.

The open controller must be able to run unchanged against:

```text
100% SIM
mixed SIM/HARD
HARD primary + SIM shadow
SIM primary + HARD shadow
100% HARD
REPLAY
```

## 12. Hardware introduction path

Proposed migration order:

```text
Stage 0  Full simulation
Stage 1  Real WTX weight in shadow; simulated machine
Stage 2  Real load cell + simulated I/O
Stage 3  Real machine DI in shadow
Stage 4  Real position/bag signals
Stage 5  Controller computes outputs but does not actuate machine
Stage 6  One physical actuator at a time
Stage 7  WTX executes coarse/fine dosing under bounded parameters
Stage 8  Full SP01 pilot
Stage 9  Repeat on additional spouts
```

Authority transfer must be explicit per channel. A hard sensor being connected does not automatically make it authoritative.

## 13. Data required per bag

Minimum cycle record:

```text
cycle_id
spout_id
recipe_id
target_weight
start_time
coarse_start
fine_start
cutoff_command_time
weight_at_cutoff
estimated_flow_at_cutoff
projected_final_at_cutoff
settled_final_weight
actual_inflight
final_error
giveaway
coarse_duration
fine_duration
total_fill_time
controller_state_transitions
weigher_status
faults
parameter_set_id
adapter/source map
```

High-rate raw data may be stored separately from cycle summaries.

## 14. Portability claim

The project does not claim that Python, Go, Rust, C, C++, PLCs, and RTOS targets have identical timing behavior.

The portability target is behavioral contract compatibility:

```text
same PackerSpec
same semantic signals
same events
same state semantics
same conformance scenarios
same bounded numerical behavior
```

Every runtime must declare timing capabilities such as:

```text
control_period
maximum observed jitter
adapter latency
network latency assumptions
critical-action latency
```

A runtime that cannot meet the profile requirement should report the profile as unsupported or degraded rather than silently claim equivalence.

## 15. Failure-boundary proposal

The design should tolerate loss of the open controller without leaving the current bag in an uncontrolled dosing state.

[Inference] The intended pattern is:

```text
open controller unavailable
-> industrial weigher finishes or aborts according to configured local policy
-> outputs enter a defined state
-> next cycle does not start until supervisory authority and permissives are restored
```

This behavior is not yet verified for WTX120 and must be proven on bench hardware before machine connection.

Machine safety functions such as emergency stop, personnel protection, motor protection, and safety-rated interlocks are outside the application software and must remain in appropriate hardwired/safety-rated circuits.

## 16. What must not be vendor-specific

Reject the design if any of the following becomes embedded in controller-core logic:

- WTX-specific register numbers;
- vendor-specific recipe structure;
- vendor-specific state names used as business logic;
- vendor-specific alarms as the only fault ontology;
- hardcoded Modbus addresses;
- GPIO numbers;
- a specific remote-I/O product;
- a specific network transport;
- a vendor HMI as the only commissioning interface;
- proprietary historical data formats as the system of record.

Vendor-specific details belong in adapters and profiles only.

## 17. Red-team questions

### Architecture

1. Is the boundary between fast deterministic control and open intelligence placed correctly?
2. Is there any hidden timing-critical dependency on Ethernet, API polling, Python, browser state, or database writes?
3. Can the open controller disappear during coarse fill, fine fill, cutoff, settling, and discharge without creating an undefined actuator state?
4. Does the proposed contract leak WTX-specific concepts into the core?
5. Is `WeighingUnit` too abstract to express real vendor differences?
6. Is it too specific to gross-weight valve-bag packers to fit net weighers, gravity fillers, screw fillers, or other packer types?

### I/O and machine semantics

7. Is the normalized 7 DI / 8 DO baseline complete?
8. What exactly is legacy `INPUT_4`?
9. Are missing feedbacks required for each pneumatic cylinder rather than relying only on commands?
10. Should filling-motor feedback and aeration-pressure feedback be mandatory?
11. Should product availability be one permissive or several independent signals?
12. Which interlocks must be hardware-level rather than controller-level?

### Weighing

13. Can the selected WTX120 variant expose enough high-rate data for shadow analysis without compromising local dosing performance?
14. What is the actual communication update rate for each available interface?
15. Can parameters be updated per bag without disturbing the active fill cycle?
16. Which parameters are writable only while idle?
17. What is the exact behavior on communication loss?
18. What is the exact behavior on load-cell open circuit, saturation, unstable zero, overload, or ADC failure?
19. Does the weigher provide enough event timing information to reconstruct cutoff/inflight accurately?
20. Is one WTX120 required per independently weighed spout? Verify electrical and firmware architecture.

### Control algorithm

21. Is projected-final-weight control materially better than the vendor's own cutoff optimization for this machine?
22. What advanced feature justifies the open controller if vendor adaptive cutoff already meets weight and throughput requirements?
23. Could external tuning destabilize a locally adaptive vendor algorithm?
24. Should only one adaptive layer be enabled at a time?
25. What parameter bounds stop a bad tuner from commanding nonsensical cutoff values?
26. How is tuning rolled back after a bad bag sequence?
27. Is adaptation per spout, material, recipe, temperature, hopper state, or all of these?

### Simulation

28. Is the plant model accurate enough to validate controller architecture, or only useful for UI/state-machine development?
29. Which parameters can actually be identified from real bag-cycle data?
30. How will sim-to-real mismatch be measured?
31. Can recorded physical cycles be replayed deterministically through new controller versions?
32. Can old and new algorithms be compared on the same physical trace?

### Throughput

33. At 2,000 bags/h, what is the actual measured fill window per spout?
34. What is the throughput impact of tare, stability checking, redosing, and discharge timing?
35. Can a 50 kg bag meet both mass accuracy and 14.4 s spout-cycle constraints under worst credible flow variation?
36. What is the bottleneck: weighing, material flow, bag handling, rotor position, or discharge?

### Fault handling

37. What happens if the dosing cylinder sticks open?
38. What happens if the fine valve does not move after the command?
39. What happens if weight stops increasing while outputs remain active?
40. What happens if weight jumps due to vibration or bag contact?
41. What happens if the bag falls off during coarse fill?
42. What happens if the bag-present signal remains stuck ON?
43. What happens if discharge position is never reached?
44. What happens after power restoration halfway through a cycle?
45. What state is restored after process restart?

### Vendor lock

46. Can WTX120 be replaced with another weighing controller without changing `controller.py` or machine state semantics?
47. What vendor-specific behavior cannot realistically be abstracted?
48. Are calibration data and legal-metrology settings exportable in an open representation?
49. Can historical bag records still be interpreted after a vendor replacement?
50. Does the FE work when the weighing adapter changes?

### Cyber/operations

51. Are control-plane and monitoring-plane network permissions separated?
52. Can a browser/API client write dosing parameters directly?
53. What authorization is required for recipe edits and authority transfer from SIM/SHADOW to HARD?
54. Are configuration changes fully logged with operator, timestamp, old value, and new value?
55. Can remote maintenance accidentally leave an output in manual mode?
56. What happens if two clients attempt to modify the same recipe?

## 18. Explicit non-goals for the first pilot

The first SP01 pilot should not attempt to solve:

- eight-spout synchronization;
- plant-wide MES orchestration;
- machine learning;
- automatic recipe optimization across all cement types;
- safety-PLC replacement;
- full unattended operation;
- legal-for-trade certification;
- arbitrary vendor support.

The pilot objective is narrower:

```text
prove the architecture boundary
prove one-spout simulation
prove one-spout WTX integration
prove shadow measurement
prove bounded parameter control
prove repeatable cycle recording/replay
prove adapter replacement does not alter controller semantics
```

## 19. Decision gates

### Gate A — bench architecture

Required evidence:

- exact WTX120 variant/firmware documented;
- exact interface/update rates measured;
- external I/O method verified;
- communication-loss behavior measured;
- local output behavior measured;
- all required DI/DO mapped;
- load-cell compatibility verified.

### Gate B — shadow on machine

Required evidence:

- no physical outputs controlled by the new system;
- real weight and machine signals recorded;
- simulator calibrated against physical traces;
- event timestamps adequate for cutoff/inflight analysis;
- legacy cycle sequence reconstructed.

### Gate C — controlled SP01 actuation

Required evidence:

- hard output authority is explicit;
- each actuator has a defined safe/off state;
- fill timeout tested;
- bag-loss case tested;
- no-flow case tested;
- stuck-output scenario tested where feasible;
- communication-loss behavior tested;
- manual recovery procedure documented.

### Gate D — production candidate

Required evidence:

- weight distribution measured over a statistically meaningful bag population;
- throughput measured;
- average giveaway measured;
- spout repeatability measured;
- fault recovery tested;
- operator procedure defined;
- configuration backup/restore demonstrated;
- vendor-adapter boundary demonstrated with at least a simulator replacement and preferably a second real or emulated backend.

## 20. Requested red-team output

Return findings in this format:

```text
SEVERITY: BLOCKER | HIGH | MEDIUM | LOW
AREA: architecture | timing | weighing | io | safety | simulation | vendor-lock | data | operations
CLAIM/ASSUMPTION UNDER ATTACK:
FAILURE MODE:
WHY IT MATTERS:
EVIDENCE REQUIRED:
PROPOSED CHANGE:
```

Then provide:

1. top five blockers;
2. architecture changes required before hardware purchase;
3. tests required before connecting any physical output;
4. claims that are currently unsupported;
5. parts of the proposal that should be deleted as unnecessary complexity;
6. final verdict: `REJECT`, `REWORK`, `PILOTABLE`, or `PRODUCTION-CANDIDATE`.

## 21. Current proposal status

Status: `REWORK / RED-TEAM REVIEW REQUIRED`.

No claim is made that the current Python simulator or the WTX120 integration is production-ready. The architecture is deliberately being reviewed before hardware authority is transferred to the new controller.