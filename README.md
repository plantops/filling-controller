# Filling Controller

Universal, digital-twin-first bag filling controller.

The permanent product is the machine behavior contract, not a specific implementation language. A machine profile, semantic signal model, state machine, strategies, adapters, events, and conformance scenarios define behavior. Runtime implementations may be replaced by Python, Go, Rust, C, C++, PLC gateways, or other targets while retaining the same contracts.

## Current scope

The first reference implementation models one filling unit (`SP01`) for a HAVER rotary packer pilot. It starts with simulated plant physics and soft sensors. Physical sensors and actuators are introduced through adapters and can run as primary, shadow, replay, or manual sources without changing controller logic.

## Architectural rules

1. Control logic never imports GPIO, HX711, Modbus, PLC, or simulator implementations.
2. All I/O is addressed by semantic signal names.
3. Plant truth, sensor measurements, and controller estimates are separate domains.
4. State transitions are deterministic and observable.
5. Simulation and real hardware use the same controller interfaces.
6. Hardware migration is channel-by-channel, with shadow comparison before authority transfer.
7. Machine/vendor differences belong in profiles, capabilities, strategies, and adapters.
8. Every runtime must pass the same conformance scenarios.

## Initial target

- One-spout digital twin
- Coarse/fine filling
- Gate and flow lag
- Material-in-flight model
- Noisy load-cell model
- Soft sensors for filtered weight, fill rate, stability, and projected final weight
- State-machine controller
- REST/WebSocket engineering interface
- SQLite cycle/event recording
- Minimal engineering web UI
- Adapter boundary for simulated and future hard I/O

See `docs/` and `spec/` in the bootstrap branch for the normative design.
