# Architecture

## Purpose

`filling-controller` is a language-neutral control model for industrial bag filling machines. The Python implementation is the first reference runtime, not the machine definition.

## Permanent contracts

The parts intended to survive language, hardware, and vendor changes are:

1. semantic signal names;
2. capability model;
3. machine profile schema;
4. controller state semantics;
5. strategy interfaces;
6. adapter interfaces;
7. event and telemetry schema;
8. conformance scenarios;
9. frontend API contract.

## Layering

```text
UI / MES / historian
        |
API + events
        |
application / recipe
        |
portable control engine
        |
logical channel router
        |
adapters: sim | hard | replay | manual | shadow
        |
plant / hardware
```

The control engine must not import GPIO, HX711, Modbus, PLC, EtherCAT, or simulator-specific modules.

## Three data domains

Keep these separate:

- **truth**: physical state known only by the simulator, such as true bag mass and actual gate opening;
- **measurement**: values reported by sensors, such as raw load-cell weight;
- **estimate**: controller-derived values, such as filtered weight, fill rate, stability, inflight mass, and projected final weight.

A real machine has measurements and estimates but normally no direct access to truth.

## Logical channels

Controllers use semantic names such as:

- `machine.running`
- `product.available`
- `bag.present`
- `position.discharge_window`
- `weight.net`
- `fill.rate`
- `weight.projected_final`
- `actuator.fill_stage`
- `actuator.aeration`
- `actuator.push`

A profile maps each logical channel to a source. The source may be simulation, physical hardware, replay, manual injection, or a derived soft sensor.

## Hardware migration

A channel is migrated independently:

```text
weight.net: sim -> hard
bag.present: sim -> hard
machine.running: sim -> hard
position.discharge_window: sim -> hard
```

During commissioning, a hard source should first run as a shadow source while simulation remains authoritative. The frontend displays both values and their difference.

## Timing

Timing requirements are part of the machine profile, not assumptions hidden in a runtime. A runtime must report whether it can meet declared control periods and latency/jitter requirements.

The bootstrap reference runtime uses a 10 ms simulation tick and a 20 ms controller tick. These are engineering defaults, not verified HAVER timing requirements.
