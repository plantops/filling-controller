# Filling Controller

Universal, digital-twin-first bag filling controller.

The permanent product is the machine behavior contract, not a specific implementation language. A machine profile, semantic signal model, state semantics, strategies, adapters, events, and conformance scenarios define behavior. Runtime implementations may be replaced by Python, Go, Rust, C, C++, PLC gateways, or other targets while retaining those contracts.

## Current scope

The first reference implementation models one filling unit (`SP01`) for a HAVER rotary packer pilot. It starts with simulated plant physics and soft sensors. Physical sensors and actuators can later be introduced through adapters without changing controller logic.

## Architectural rules

1. Control logic never imports GPIO, HX711, Modbus, PLC, or simulator implementations.
2. All I/O is addressed by semantic signal names.
3. Plant truth, sensor measurements, and controller estimates are separate domains.
4. State transitions are deterministic and observable.
5. Simulation and real hardware use the same controller interfaces.
6. Hardware migration is channel-by-channel, with shadow comparison before authority transfer.
7. Machine/vendor differences belong in profiles, capabilities, strategies, and adapters.
8. Alternate runtimes must pass the same conformance scenarios before behavioral equivalence is claimed.

## Bootstrap contents

- `docs/ARCHITECTURE.md` — permanent layering and portability rules
- `docs/SPECIFICATION.md` — language-neutral packer model
- `docs/ADAPTERS.md` — sim/hard/shadow adapter contract
- `docs/SIMULATION.md` — filling physics model
- `docs/ROADMAP.md` — staged hard-sensor migration
- `spec/profiles/haver-rotary-pilot-sp01.yaml` — first machine profile
- `src/filling_controller/` — Python reference runtime
- `tests/test_cycle.py` — deterministic simulated-cycle conformance seed

## Run

Python 3.14 is the reference CI runtime. The package currently supports Python 3.12+.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[dev]'
filling-controller
```

On Windows PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -e ".[dev]"
filling-controller
```

Open `http://127.0.0.1:8000`.

The API is available at `http://127.0.0.1:8000/docs`.

## Select another profile

The runtime defaults to:

```text
spec/profiles/haver-rotary-pilot-sp01.yaml
```

Override it with:

```bash
FILLING_PROFILE=/path/to/profile.yaml filling-controller
```

## Test

```bash
pytest -q
```

The first deterministic scenario produces a completed 50 kg-class simulated bag while explicitly modeling gate lag, transport delay, sensor noise, and material in flight.
