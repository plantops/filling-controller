# Adapter Contract

## Rule

Adapters translate between semantic channels and implementations. Control logic only sees semantic values and commands.

## Sources

Supported source classes:

- `sim`: virtual machine or virtual sensor;
- `hard`: physical sensor/actuator;
- `derived`: soft sensor;
- `replay`: recorded event stream;
- `manual`: engineering injection;
- `shadow`: non-authoritative comparison source.

## Sensor value envelope

Each value carries at least:

```text
tag
value
unit
timestamp
quality
source
```

Quality values are `good`, `stale`, `bad`, `unknown`, or `simulated`.

## Adapter interface

Language-neutral conceptual interface:

```text
start()
stop()
read(tag) -> SignalValue
write(tag, value)
health() -> diagnostics
```

A hardware adapter owns hardware-specific details such as GPIO numbers, Modbus addresses, PLC symbols, register endianness, scaling, and electrical polarity.

## Shadow migration

A router may expose an authoritative source and one or more shadow sources for the same semantic signal.

Example:

```text
weight.net.primary = sim.loadcell
weight.net.shadow  = hard.modbus_weigher
```

After comparison, authority can be switched without changing controller code.
