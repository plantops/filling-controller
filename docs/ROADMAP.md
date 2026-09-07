# Roadmap

## V0 - digital twin

- Python reference runtime
- one simulated filling unit
- semantic signals
- coarse/fine controller
- projected cutoff
- SQLite cycle/event recording
- engineering web UI
- deterministic tests

## V1 - hard load cell in shadow

- HX711 or industrial transmitter adapter
- real weight channel as shadow
- comparison metrics: sim vs hard
- no machine outputs

## V2 - hard discrete inputs

- bag detect
- machine running / process initiative
- product permissive
- discharge/position input

## V3 - controller shadow mode

The physical packer remains authoritative. The new controller observes real signals and records when it would issue coarse, fine, cutoff, and push commands.

## V4 - one controlled output

Introduce one actuator under controlled commissioning conditions, with explicit safe-state and timeout behavior.

## V5 - full SP01 pilot

Physical single-spout controller with real sensors and actuators.

## V6 - portable statechart and conformance suite

Move statechart semantics into the language-neutral spec, freeze event schemas, and require Python/Rust/Go/C/C++ runtimes to pass the same vectors.
